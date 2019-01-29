#include "DeferredShading.hlsli"


[RootSignature(DeferredShading_RootSig)]
PostProcessingVSOutput main(uint vertexId: SV_VertexID) {
	PostProcessingVSOutput output;

	output.tex =  float2((2-vertexId << 1) & 2, 2 - vertexId & 2);
	output.position = float4(output.tex * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
	return output;
}