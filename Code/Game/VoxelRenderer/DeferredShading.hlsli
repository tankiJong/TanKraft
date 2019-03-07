#include "Common.hlsli"
#include "../SceneRenderer/fullscreen.hlsli"


struct PSOutput {
	float4 color: SV_TARGET0;
};


struct light_t {
	float4 position;
	float4 color;
};

Texture2D<float4> gTexAlbedo:   register(t0);
Texture2D<float4> gTexNormal:   register(t1);
Texture2D<float4> gTexTangent:   register(t2);
Texture2D<float4> gTexBitangent:   register(t3);
Texture2D<float4> gTexPosition:   register(t4);
Texture2D<float4> gTexAO:   register(t5);
Texture2D<float4> gTexDepth:   register(t6);
StructuredBuffer<light_info_t> gLights: register(t7);
TextureCube<float4> gSky: register(t8);
// Texture2D gTexSpecular: register(t2);

#define DeferredShading_RootSig \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
     RootSig_Common  \
		"DescriptorTable(SRV(t0, numDescriptors = 9, flags = DATA_VOLATILE), visibility = SHADER_VISIBILITY_ALL)," \
    "StaticSampler(s0, filter = FILTER_ANISOTROPIC, maxAnisotropy = 1, visibility = SHADER_VISIBILITY_PIXEL)," 

