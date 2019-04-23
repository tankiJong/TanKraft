#include "GenGBuffer.hlsli"

[RootSignature(GenBuffer_RootSig)]
PSOutput main(PSInput input)
{
	PSOutput output;

	float4 texColor = gTexAlbedo.Sample(gSampler, input.uv);
	// float4 texColor = input.color;
  // output.color = float4(
	// 	PhongLighting(input.worldPosition, input.normal, texColor.xyz, input.eyePosition), 1.f);
	
	float3 indoorLight = input.color.x * gWorldConstant.y * float3(1.f, 0.9f, 0.8f);

	float timeInDay = gTime / 86400.f;
	timeInDay = timeInDay - floor(timeInDay);

	float ambientStrength = input.color.y * (lerp(1, 1, -cos(2 * 3.1415926 * timeInDay) * .5f + .5f) + gWorldConstant.x);
	ambientStrength = clamp(ambientStrength, 0, 1.3f);
	float3 ambientLight = ambientStrength * float3(.8f, .9f, 1.f);
	ambientLight *= ambientStrength;

	// ambientLight = 1.f.xxx;

	output.color = texColor;
	output.normal = float4(input.normal * .5f + .5f, 1.f);
	output.tangent = float4(input.tangent * .5f + .5f, 1.f);

	float3 bitangent = normalize(cross(input.normal, input.tangent));

	output.bitangent = float4(bitangent * .5f + .5f, 1.f);
	output.position = float4(input.worldPosition, 1.f);

	// output.color = float4(1, 0,0,1);
	return output;
}