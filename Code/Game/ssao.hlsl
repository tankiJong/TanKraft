#define IDENTITY_MATRIX float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)

float4x4 inverse(float4x4 m) {
    float n11 = m[0][0], n12 = m[1][0], n13 = m[2][0], n14 = m[3][0];
    float n21 = m[0][1], n22 = m[1][1], n23 = m[2][1], n24 = m[3][1];
    float n31 = m[0][2], n32 = m[1][2], n33 = m[2][2], n34 = m[3][2];
    float n41 = m[0][3], n42 = m[1][3], n43 = m[2][3], n44 = m[3][3];

    float t11 = n23 * n34 * n42 - n24 * n33 * n42 + n24 * n32 * n43 - n22 * n34 * n43 - n23 * n32 * n44 + n22 * n33 * n44;
    float t12 = n14 * n33 * n42 - n13 * n34 * n42 - n14 * n32 * n43 + n12 * n34 * n43 + n13 * n32 * n44 - n12 * n33 * n44;
    float t13 = n13 * n24 * n42 - n14 * n23 * n42 + n14 * n22 * n43 - n12 * n24 * n43 - n13 * n22 * n44 + n12 * n23 * n44;
    float t14 = n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 + n12 * n24 * n33 + n13 * n22 * n34 - n12 * n23 * n34;

    float det = n11 * t11 + n21 * t12 + n31 * t13 + n41 * t14;
    float idet = 1.0f / det;

    float4x4 ret;

    ret[0][0] = t11 * idet;
    ret[0][1] = (n24 * n33 * n41 - n23 * n34 * n41 - n24 * n31 * n43 + n21 * n34 * n43 + n23 * n31 * n44 - n21 * n33 * n44) * idet;
    ret[0][2] = (n22 * n34 * n41 - n24 * n32 * n41 + n24 * n31 * n42 - n21 * n34 * n42 - n22 * n31 * n44 + n21 * n32 * n44) * idet;
    ret[0][3] = (n23 * n32 * n41 - n22 * n33 * n41 - n23 * n31 * n42 + n21 * n33 * n42 + n22 * n31 * n43 - n21 * n32 * n43) * idet;

    ret[1][0] = t12 * idet;
    ret[1][1] = (n13 * n34 * n41 - n14 * n33 * n41 + n14 * n31 * n43 - n11 * n34 * n43 - n13 * n31 * n44 + n11 * n33 * n44) * idet;
    ret[1][2] = (n14 * n32 * n41 - n12 * n34 * n41 - n14 * n31 * n42 + n11 * n34 * n42 + n12 * n31 * n44 - n11 * n32 * n44) * idet;
    ret[1][3] = (n12 * n33 * n41 - n13 * n32 * n41 + n13 * n31 * n42 - n11 * n33 * n42 - n12 * n31 * n43 + n11 * n32 * n43) * idet;

    ret[2][0] = t13 * idet;
    ret[2][1] = (n14 * n23 * n41 - n13 * n24 * n41 - n14 * n21 * n43 + n11 * n24 * n43 + n13 * n21 * n44 - n11 * n23 * n44) * idet;
    ret[2][2] = (n12 * n24 * n41 - n14 * n22 * n41 + n14 * n21 * n42 - n11 * n24 * n42 - n12 * n21 * n44 + n11 * n22 * n44) * idet;
    ret[2][3] = (n13 * n22 * n41 - n12 * n23 * n41 - n13 * n21 * n42 + n11 * n23 * n42 + n12 * n21 * n43 - n11 * n22 * n43) * idet;

    ret[3][0] = t14 * idet;
    ret[3][1] = (n13 * n24 * n31 - n14 * n23 * n31 + n14 * n21 * n33 - n11 * n24 * n33 - n13 * n21 * n34 + n11 * n23 * n34) * idet;
    ret[3][2] = (n14 * n22 * n31 - n12 * n24 * n31 - n14 * n21 * n32 + n11 * n24 * n32 + n12 * n21 * n34 - n11 * n22 * n34) * idet;
    ret[3][3] = (n12 * n23 * n31 - n13 * n22 * n31 + n13 * n21 * n32 - n11 * n23 * n32 - n12 * n21 * n33 + n11 * n22 * n33) * idet;

    return ret;
}


struct PSInput {
	float4 position: SV_POSITION;
	float2 uv: UV;
};

PSInput VSMain(uint id: SV_VertexID)
{
	PSInput input;

	input.uv = float2((id << 1) & 2, id & 2);
	input.position = float4(input.uv * float2(2, -2) + float2(-1, 1), 0, 1);
	return input;
}

#define NUM_KERNEL 64

struct ssao_param_t {
	float4 kernels[NUM_KERNEL];
};

cbuffer cCamera : register(b0) {
  float4x4 projection;
  float4x4 view;
};
cbuffer cSSAOParam: register(b1) {
	ssao_param_t ssaoParam;
}

Texture2D gTexDepth : register(t0);
Texture2D gTexNormal : register(t1);
Texture2D gTexScene: register(t2);
Texture2D gTexNoise: register(t3);
SamplerState gSampler : register(s0);


struct PSOutput {
	float4 color: SV_TARGET0;
	float4 occlusion: SV_TARGET1;
};
PSOutput PSMain(PSInput input) : SV_TARGET 
{
	PSOutput output;
	// get world position, normal, tbn transform
	float2 xy = input.uv;
	xy.y = 1.f - xy.y;
	//output.color = float4(xy, 0, 1);
	//return output;
	float d = gTexDepth.Sample(gSampler, input.uv).r;
	float3 xyz = float3(xy, d);
	xyz = xyz * 2.f - 1.f;
	float4 ndc = float4(xyz, 1.f);
	float4 almostWorldPosition = mul(inverse(mul(projection,view)), ndc);
	float3 worldPosition = almostWorldPosition.xyz / almostWorldPosition.w;
	//float linearDepth = worldPosition.z / 100.f;
	//output.color = float4(linearDepth, linearDepth, linearDepth, 1.f);

	float3 normal = normalize(gTexNormal.Sample(gSampler, input.uv).xyz * 2.f - 1.f);

	float3 noise = (gTexNoise.Sample(gSampler, input.uv).xyz - .5f) * 2.f;
	noise.y = 0;
	float3 right = float3(1.f, 0.f, 0.f);
	float3 _tan = normalize(right+noise);
	float3 bitan = cross(_tan, normal);
	float3 tan = cross(bitan, normal);

	float3x3 tbn = transpose(float3x3(tan, normal, bitan));
	
	//normal = mul(tbn, float3(1,0,0)); 
	//float a = dot(normal, tan) + dot(normal, bitan) ;
	//output.color = float4(normal, 1.f);
	//return output;
	float validRange = 0.0001f;

	float occlusion = 0.f;

	for(uint i = 0; i < NUM_KERNEL; i++) {
		float3 kernel = ssaoParam.kernels[i].xyz;
		float3 sample = mul(tbn, kernel) * 5.f;
		sample += worldPosition;

		float4 clip = float4(sample, 1.f);
		clip = mul(mul(projection, view), clip);

		float3 uvd = clip.xyz / clip.w;
		uvd = uvd * .5f + .5f;
		float2 uv = uvd.xy;
		uv.y = 1 - uv.y;
		
		//float linDepth = uvd.z / 100.f;
		//output.occlusion = float4(linDepth, linDepth, linDepth, 1.f);
		//return output;
		float depth = gTexDepth.Sample(gSampler, uv).r;

		//output.color = float4(depth, depth, depth, 1.f);
		//return output;

		float rangeCheck = smoothstep(0.f , 1.f, validRange / abs(d - depth));
		occlusion += rangeCheck * (((depth+0.00003) < uvd.z) ? 1.f: 0.f);
	}

	occlusion = 1.f - (occlusion / (float)NUM_KERNEL);
	
	float4 color = gTexScene.Sample(gSampler, input.uv);

	
	output.color = pow(occlusion, 3.f) * color;
	output.occlusion = float4(occlusion, occlusion, occlusion, 1.f);

	return output;
}