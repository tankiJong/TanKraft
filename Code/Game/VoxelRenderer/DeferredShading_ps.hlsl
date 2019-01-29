#include "DeferredShading.hlsli"
#include "../SceneRenderer/Lighting.hlsli"


[RootSignature(DeferredShading_RootSig)]
PSOutput main(PostProcessingVSOutput input)
{
	PSOutput output;

	float4 color = gTexAlbedo.Sample(gSampler, input.tex);
	float3 position = gTexPosition.Sample(gSampler, input.tex).xyz;
	float3 normal = gTexNormal.Sample(gSampler, input.tex).xyz;

	uint total,_;
	gLights.GetDimensions(total, _);

	float4 camPosition = mul(inverse(view), float4(0,0,0,1));
	camPosition /= camPosition.w;

	float3 eye = normalize(position - camPosition.xyz);

	float3 finalColor = float3(0,0,0);
	for(uint i = 0; i < total; i++) {
		light_info_t l = gLights[i];
		finalColor += Diffuse(position, normal, color.xyz, l);
		finalColor += Specular(position, normal, eye, .01, 2, l);
	}

	output.color = float4(finalColor, 1);
	return output;
}