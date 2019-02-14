#ifndef __SR_COMMON_INCLUDED__
#define __SR_COMMON_INCLUDED__

#include "Math.hlsl"
#include "Random.hlsli"

#define RootSig_Common "DescriptorTable(CBV(b0, numDescriptors = 4), visibility = SHADER_VISIBILITY_ALL),"

#define M_PI 3.141592653

struct vertex_t {
	float4 position;
	float4 color;
};


struct DisneyBRDFParam {
	float subsurface;
	float specular;
	float specularTint;
	float anisotropic;
	float sheen;
	float sheenTint;
	float clearcoat;
	float clearcoatGloss;
};

struct light_info_t {
  float4 color;

  float3 attenuation;
  float dotInnerAngle;

  float3 specAttenuation;
  float dotOuterAngle;

  float3 position;
  float directionFactor;

  float3 direction;
  float __pad00;

  float4x4 vp;
};

cbuffer cFrameData: register(b0) {
	float gTime;
	float gFrameCount;
	float gRoughness;
	float gMetallic;
}

cbuffer cCamera : register(b1) {
  float4x4 projection;
  float4x4 view;
	float4x4 prev_projection;
  float4x4 prev_view;
};

cbuffer cModel: register(b2) {
	float4x4 model;
}

cbuffer cPbrParam: register(b3) {
	DisneyBRDFParam gPbrParam;
}

SamplerState gSampler : register(s0);

	

float3 GetRandomDirection(inout uint seed, float3 normal, float3 tangent, float3 bitangent) {

	float3x3 tbn = transpose(float3x3(tangent, normal, bitangent));

	Randomf r = rnd01(seed); 
	seed = r.seed;
	float b = r.value; // cosTheta -> y

	r = rnd01(seed);
	seed = r.seed;
	float sinTheta = sqrt(1 - b*b);
	float phi = 2 * 3.1415926f * r.value;

	float a = sinTheta * cos(phi);
	float c = sinTheta * sin(phi);

	float3 ssample = normalize(mul(tbn, float3(a, b, c)));

	return ssample;
}


#endif