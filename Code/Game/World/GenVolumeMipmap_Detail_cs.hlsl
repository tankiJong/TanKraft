#include "../VoxelRenderer/VolumeUtil.hlsli"


#define GenVolumeMipmap_RootSig \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
		"DescriptorTable(SRV(t0, numDescriptors = 1, flags = DATA_VOLATILE), visibility = SHADER_VISIBILITY_ALL)," \
		"DescriptorTable(UAV(u0, space = 0, numDescriptors = 3), visibility = SHADER_VISIBILITY_ALL)," \
    "StaticSampler(s0, filter = FILTER_ANISOTROPIC, maxAnisotropy = 1, visibility = SHADER_VISIBILITY_PIXEL)," 

Texture3D<uint>	gInput: register(t0);		// mip 0
RWTexture3D<uint> uVisibility[3]: register(u0, space0);		 // mip 0 ~ 8


uint toIndex(uint x, uint y, uint z, uint dim) {
	return z * dim * dim + y * dim + x;
}

uint toIndex(uint3 coords, uint dim) {
	return toIndex(coords.x, coords.y, coords.z, dim);
}

groupshared uint visibility[3][512];

[RootSignature(GenVolumeMipmap_RootSig)]
[numthreads(8, 8, 8)]
void main( uint3 localId : SV_GroupThreadID, uint3 groupId: SV_GroupId )
{
	uint3 globalId = groupId * 8 + localId;
	
	uint val = gInput.Load(uint4(globalId, 0));
	Block block = unpack(val);

	// filling top level mip
	visibility[0][toIndex(localId, 8)] = block.opaque() ? 1 : 0;

	for(uint i = 1; i < 4; i++) {
		visibility[i][toIndex(localId, 8)] = 0;
	}
	// uVisibility[0][globalId] = block.opaque();

	// clear 1 ~ 3
	{
		[unroll]
		for(uint i = 0; i < 3; i++) {
			uint3 mipIndex = globalId >> i;
			uVisibility[i][mipIndex] = 0;
		}
	}

	
	AllMemoryBarrierWithGroupSync();

	// fill 0 ~ 3
	{
		[unroll]
		for(uint i = 1; i < 3; i++) {	
			if( all( localId >> (i - 1) << (i - 1) == localId) ) {
				InterlockedAdd(visibility[i][toIndex(localId >> i, 8 >> i)], visibility[i - 1][toIndex(localId >> (i - 1), 8 >> (i - 1))]);
			}
			GroupMemoryBarrierWithGroupSync();
		}

	}
		atmoicAddU8(uVisibility[0], globalId, visibility[0][toIndex(localId, 8)]);
	{
		uint bitShift = 3;
		for(uint i = 1; i < 3; i++) {
			if(all((globalId >> i << i) == globalId)) {
				uint totalVisibility = visibility[i][toIndex(localId >> i, 8 >> i)];
				atmoicAddU8(uVisibility[i], globalId >> i, clamp(totalVisibility, 0, 1));
			}
			bitShift = bitShift + 3;
		}

	}
	
}