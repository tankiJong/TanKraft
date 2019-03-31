#include "Common.hlsli"
#include "Rt_Util.hlsli"


#define RT_RootSig \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
     RootSig_Common  \
		"DescriptorTable(SRV(t0, numDescriptors = 2), UAV(u0, numDescriptors = 1), visibility = SHADER_VISIBILITY_ALL)," \
    "StaticSampler(s0, filter = FILTER_ANISOTROPIC, maxAnisotropy = 1, visibility = SHADER_VISIBILITY_PIXEL)," 

struct Block {
	uint data;
};

struct BlockDef {
  uint id;
  float emission;
  float opaque;
  float2 ___padding0;
  struct { float2 mins; float2 maxs; } uvs[4];
};

StructuredBuffer<BlockDef> tBlockDef: register(t0);
Texture3D<uint> block: register(t1);

RWTexture2D<float4> uOutput: register(u0);


#define BLOCK_DIM_X 16
#define BLOCK_DIM_Y 16
#define BLOCK_DIM_Z 256

#define BLOCK_DIM BLOCK_DIM_X, BLOCK_DIM_Y, BLOCK_DIM_Z

void GetCoords(float3 worldPosition, out uint2 chunkCoords, uint3 blockCoords) {
	
	float3 div = worldPosition / float3(BLOCK_DIM_X,BLOCK_DIM_Y, BLOCK_DIM_Z);
	float3 chunkc = floor(div);
	chunkCoords = chunkc.xy;

	float3 offsetInChunk = worldPosition - chunkc * float3(BLOCK_DIM);
	float3 blockc = floor(offsetInChunk);
	blockCoords = blockc;
}
							

float3 GetCameraPosition() {
	float4x4 inverView = inverse(view);
	float4 world = mul(inverView, float4(0.f.xxx, 1.f));
	return world.xyz / world.w;
}



[RootSignature(RT_RootSig)]
[numthreads(1, 1, 1)]
void main( uint3 pixelCoords : SV_DispatchThreadID )
{
	uint2 dim;
	uOutput.GetDimensions(dim.x, dim.y);

	float4x4 vp = mul(projection, view);
	float4x4 inverCamVP = inverse(vp);
	float4x4 inverView = inverse(view);

	Ray primRay;
	
	{
		float2 uv = float2(pixelCoords.xy) / float2(dim);
		uv.y = 1 - uv.y;
		float3 ndc1 = float3(uv * 2.f - 1.f, .2f);
		float4 p1 = mul(inverCamVP, float4(ndc1, 1.f));
		float3 position1 = p1.xyz / p1.w;

		float3 ndc2 = float3(uv * 2.f - 1.f, .5f);
		float4 p2 = mul(inverCamVP, float4(ndc2, 1.f));
		float3 position2 = p2.xyz / p2.w;

		primRay.direction = normalize(position2 - position1);
	}

	{
		float3 cameraWorld = GetCameraPosition();
		GetCoords(cameraWorld, primRay.chunkCoords, primRay.blockCoords);
		primRay.origin = cameraWorld;
	}


	while(any(primRay.blockCoords < uint3(BLOCK_DIM))) {
		
	}


	
}