#include "Common.hlsli"


#define SSAO_RootSig \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
     RootSig_Common  \
		"DescriptorTable(SRV(t1, numDescriptors = 5), UAV(u0, numDescriptors = 1), visibility = SHADER_VISIBILITY_ALL)," \
    "StaticSampler(s0, filter = FILTER_ANISOTROPIC, maxAnisotropy = 1, visibility = SHADER_VISIBILITY_PIXEL)," 


Texture2D<float4> gTexNormal:   register(t1);
Texture2D<float4> gTexTangent:   register(t2);
Texture2D<float4> gTexBitangent:   register(t3);
Texture2D<float4> gTexPosition:   register(t4);
Texture2D<float4> gTexDepth:   register(t5);

RWTexture2D<float4> uTexAO: register(u0); 

static uint seed;

static const float SSAO_DEPTH_RADIUS = .8f;
#define SSAO_SAMPLE_COUNT 32

[RootSignature(SSAO_RootSig)]
[numthreads(16, 16, 1)]
void main( uint3 pixCoords : SV_DispatchThreadID ) {
	seed = pixCoords.x * 90000 + 121144 * pixCoords.y + gFrameCount;
	
	uint2 dim;

	gTexNormal.GetDimensions(dim.x, dim.y);

	if(pixCoords.x > dim.x || pixCoords.y > dim.y) return;

	float3 normal = gTexNormal[pixCoords.xy].xyz;


	float3 tangent = gTexTangent[pixCoords.xy].xyz;
	float3 bitangent = gTexBitangent[pixCoords.xy].xyz;
	normal = normal * 2.f - 1.f;
	tangent = tangent * 2.f - 1.f;
	bitangent = bitangent * 2.f - 1.f;

	if(length(normal) < .5f) return;
	float3 position = gTexPosition[pixCoords.xy].xyz;
	
	float4 clip = mul(mul(projection, view), float4(position, 1.f));

	float4 camPosition = mul(inverse(view), float4(0,0,0,1));
	camPosition = camPosition / camPosition.w;

	float occlusion = 0.f;
	for(uint i = 0; i < SSAO_SAMPLE_COUNT; i++) {
		float3 dir = GetRandomDirection(seed, normal, tangent, bitangent);
		
		// step and compare
		float3 dest = dir * SSAO_DEPTH_RADIUS + position;

		float sampleDepth = length(dest - camPosition.xyz);
		
		float4 clip = mul(projection, mul(view, float4(dest, 1.f)));
		
		float3 uvd = clip.xyz / clip.w;
		float2 uv = uvd.xy;
		uv = uv * .5f + .5f;
		uv.y = 1 - uv.y;
		
		float depth = gTexDepth[saturate(uv) * dim].x;
		
		float3 ndc = float3(uvd.xy, depth);
		
		float4 sampleWorld = mul(inverse(mul(projection, view)), float4(ndc, 1.f));
		sampleWorld = sampleWorld / sampleWorld.w;
		
		float sceneDepth = length(sampleWorld.xyz - camPosition.xyz);

		//float rangeAtten = smoothstep(1.f, 0.f, abs(sceneDepth - sampleDepth) / SSAO_DEPTH_RADIUS);
		float rangeAtten = step(abs(sceneDepth - sampleDepth), SSAO_DEPTH_RADIUS);
		
		occlusion += rangeAtten * step(sceneDepth + .1f, sampleDepth);
		// uTexAO[pixCoords.xy] = float4(uv, 0, 1.f);
	}
	
	float occ = 1.f - occlusion / SSAO_SAMPLE_COUNT;
	// float occ = occlusion;
	uTexAO[pixCoords.xy] = float4(occ, occ, occ, 1.f);
}