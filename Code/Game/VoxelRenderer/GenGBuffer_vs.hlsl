#include "GenGbuffer.hlsli"

[RootSignature(GenBuffer_RootSig)]
PSInput main(
	float3 position: POSITION,
	float4 color:    COLOR,
	float2 uv:       UV,
	float3 normal:   NORMAL,
	float3 tangent:  TANGENT) {

  PSInput result;

	result.worldPosition = mul(model, float4(position, 1.f));
  result.position = mul(mul(projection, view), float4(result.worldPosition, 1.f));
  result.color = color;
  result.uv = uv;
  result.normal = normal;
	result.tangent = tangent;

	return result;
}