#include "../VoxelRenderer/VolumeUtil.hlsli"


#define GenVolumeMipmap_RootSig \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
		"DescriptorTable(UAV(u0, space = 0, numDescriptors = 2), visibility = SHADER_VISIBILITY_ALL)," \
    "StaticSampler(s0, filter = FILTER_ANISOTROPIC, maxAnisotropy = 1, visibility = SHADER_VISIBILITY_PIXEL)," 

RWTexture3D<uint> uVisibility[2]: register(u0, space0);		 // mip 0 ~ 8


uint toIndex(uint x, uint y, uint z, uint dim) {
	return z * dim * dim + y * dim + x;
}

uint toIndex(uint3 coords, uint dim) {
	return toIndex(coords.x, coords.y, coords.z, dim);
}

groupshared uint total;
[RootSignature(GenVolumeMipmap_RootSig)]
[numthreads(2, 2, 2)]
void main( uint3 localId : SV_GroupThreadID, uint3 groupId: SV_GroupId )
{
	uVisibility[1][groupId] = 0;
	total = 0;
	AllMemoryBarrierWithGroupSync();
	
	InterlockedAdd(total, uVisibility[0][(groupId << 1) + localId]);

	GroupMemoryBarrierWithGroupSync();

	if(all(localId == 0u.xxx)) {
		uVisibility[1][groupId] = clamp(total, 0, 1);
	}
}