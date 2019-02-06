#include "DeferredShading.hlsli"
#include "../SceneRenderer/Lighting.hlsli"


[RootSignature(DeferredShading_RootSig)]
PSOutput main(PostProcessingVSOutput input)
{
	PSOutput output;

	float4 color = gTexAlbedo.Sample(gSampler, input.tex);
	// float4 color = float4(1, 1, 1, 1);
	float3 position = gTexPosition.Sample(gSampler, input.tex).xyz;
	float3 normal = gTexNormal.Sample(gSampler, input.tex).xyz * 2.f - 1.f;


	uint total,_;
	gLights.GetDimensions(total, _);

	float4 camPosition = mul(inverse(view), float4(0,0,0,1));
	camPosition /= camPosition.w;

	float3 eye = normalize(position - camPosition.xyz);

	float3 finalColor = color * .1f;
	// float3 finalColor = float3(0,0,0);
	for(uint i = 0; i < total; i++) {
		light_info_t l = gLights[i];
		l.position.y += sin(gTime);
		finalColor += Diffuse(position, normal, color.xyz, l);
		finalColor += Specular(position, normal, eye, .1, 3, l);
	}

	output.color = float4(saturate(finalColor), 1);
	return output;
}