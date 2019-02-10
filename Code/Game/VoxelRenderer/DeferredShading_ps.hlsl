#include "DeferredShading.hlsli"
#include "../SceneRenderer/Lighting.hlsli"


[RootSignature(DeferredShading_RootSig)]
PSOutput main(PostProcessingVSOutput input)
{
	PSOutput output;

	float4 color = gTexAlbedo.Sample(gSampler, input.tex);
	color = pow(color, 2.2f);
	// float4 color = float4(1, 1, 1, 1);
	float3 position = gTexPosition.Sample(gSampler, input.tex).xyz;
	float3 normal = gTexNormal.Sample(gSampler, input.tex).xyz;
	normal = normal  * 2.f - 1.f;

	float3 tan = normalize(gTexTangent.Sample(gSampler, input.tex).xyz * 2.f - 1.f);
	// float3 bitan = gTexBitangent.Sample(gSampler, input.tex).xyz * 2.f - 1.f;
	float3 bitan = normalize(cross(tan, normal));
	// output.color = gTexTangent.Sample(gSampler, input.tex);
	// return output;

	uint total,_;
	gLights.GetDimensions(total, _);

	float4 camPosition = mul(inverse(view), float4(0,0,0,1));
	camPosition /= camPosition.w;

	float3 eye = normalize(camPosition.xyz - position);

	// float3 finalColor = color.xyz * .1f;
	float3 finalColor = 0.f.xxx;
	float3 diffuse = 0.f.xxx, specular = 0.f.xxx;
	/*
	for(uint i = 0; i < total; i++) {
		light_info_t l = gLights[i];
		// l.position.y += sin(gTime);
		// finalColor += Diffuse(position, normal, color.xyz, l);
		// finalColor += Specular(position, normal, eye, .1, 3, l);
		 finalColor += 
			pbrDirectLighting(color.xyz, gRoughness, gMetallic, position, camPosition.xyz, normal, tan, bitan, l, diffuse, specular);
	}
	 */
	// finalColor += pbrIndirectLighting(diffuse, 1.f.xxx, 1.f.xxx);
	// finalColor += pbrEnvironmentLighting(color.xyz, normal, gRoughness, gMetallic, 1, eye, 1.f.xxx);

	finalColor += color.xyz * 1.f;

	output.color = float4(saturate(finalColor), 1);
	return output;
}