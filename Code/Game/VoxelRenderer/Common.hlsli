#ifndef __SR_COMMON_INCLUDED__
#define __SR_COMMON_INCLUDED__

#include "Math.hlsl"
#include "Random.hlsli"

#define RootSig_Common "DescriptorTable(CBV(b0, numDescriptors = 3), visibility = SHADER_VISIBILITY_ALL),"

#define M_PI 3.141592653

struct vertex_t {
	float4 position;
	float4 color;
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


SamplerState gSampler : register(s0);

	




#endif