#include "GenGbuffer.hlsli"


[RootSignature(GenBuffer_RootSig)]
PSInput main(
	float3 position: POSITION,
	float4 color:    COLOR,
	float2 uv:       UV,
	float3 normal:   NORMAL,
	float3 tangent:  TANGENT) {

  PSInput result;
	
	// do the sphere re position in view space

	float4 camPosition = mul(inverse(view), float4(0,0,0,1));
	camPosition /= camPosition.w;
	
	
	float4 worldPosition = mul(model, float4(position, 1.f));
	float3 relatvieToCameraPosition = worldPosition.xyz - camPosition.xyz;

	float3 planetCenterV = float3(0, 0, -camPosition.z - gPlanetRadius);

	float dist = length(relatvieToCameraPosition.xy);

	float R = distance(relatvieToCameraPosition.z, planetCenterV.z);
	float rad = dist / R;
	float deltaZ = R * cos(rad) - R;
	float newXY = R * sin(rad);
	
	deltaZ = deltaZ * (rad < 2 * 3.1415936f);
	newXY = (rad < 2 * 3.1415926f) ? newXY : 1;

	float3 newRelativeToCamera = relatvieToCameraPosition;
	newRelativeToCamera.z += deltaZ;
	
	newRelativeToCamera.xy = normalize(newRelativeToCamera.xy) * newXY;

	float3 world = newRelativeToCamera + camPosition.xyz;
	
	result.worldPosition = world;
  result.position = mul(mul(projection, view), float4(result.worldPosition, 1.f));
  result.color = color;
  result.uv = uv;
  result.normal = normal;
	result.tangent = tangent;

	return result;
}