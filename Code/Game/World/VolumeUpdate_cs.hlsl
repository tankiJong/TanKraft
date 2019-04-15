
#define VolumeUpdate_RootSig \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
		"DescriptorTable(SRV(t0, numDescriptors = 256, flags = DATA_VOLATILE), UAV(u0, numDescriptors = 1), visibility = SHADER_VISIBILITY_ALL)," \
    "StaticSampler(s0, filter = FILTER_ANISOTROPIC, maxAnisotropy = 1, visibility = SHADER_VISIBILITY_PIXEL)," 

Texture3D<uint>	gTextures[256]: register(t0);
RWTexture3D<uint> uOutput: register(u0);


[RootSignature(VolumeUpdate_RootSig)]
[numthreads(1, 4, 64)]				// 16/16,  16/4, 256/4
void main( uint3 localId : SV_GroupThreadID, uint3 groupId: SV_GroupId )
{
	uint3 baseCoords = groupId * uint3(16, 16, 256);
	// 
	// for(uint k = 0; k < 256; k++) {
	// 	for(uint j = 0; j < 16; j++) {
	// 		for(uint i = 0; i < 16; i++) {
	// 			uint3 localCoords = uint3(i, j, k);
	// 			uint tex = gTextures[16 * groupId.y + groupId.x].Load(uint4(localCoords, 0));
	// 			uOutput[baseCoords + localCoords] = tex;
	// 		}
	// 	}
	// }

  uint3 offset = uint3(0, localId.y * 4, localId.z * 4);
  
  for(uint k = 0; k < 4; k++) {
  	for(uint j = 0; j < 4; j++) {
  		for(uint i = 0; i < 16; i++) {
  			uint3 localCoords = uint3(i + offset.x, j + offset.y, k + offset.z);
  			uint tex = gTextures[16 * groupId.y + groupId.x].Load(uint4(localCoords, 0));
  			uOutput[baseCoords + localCoords] = tex;
  		}
  	}
  }
}