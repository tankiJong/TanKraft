#ifndef __VOLUME_UTIL_H__
#define __VOLUME_UTIL_H__
	
# define kOpaqueFlag (0x1 << 0)

struct Block {
  uint type;
  uint light;
  uint bits;

	uint opaque()	{
		return bits & kOpaqueFlag;
	}
};


#define BLOCK_TYPE_BITS   0
#define BLOCK_LIGHT_BITS  8
#define BLOCK_BITS_BITS   16
#define BLOCK_TYPE_MASK   (0xff <<	BLOCK_TYPE_BITS	)
#define BLOCK_LIGHT_MASK  (0xff <<	BLOCK_LIGHT_BITS)
#define BLOCK_BITS_MASK   (0xff <<  BLOCK_BITS_BITS	)

Block unpack(uint bf) {
	Block block;

	block.type  = (BLOCK_TYPE_MASK & bf)  >> BLOCK_TYPE_BITS;
	block.light = (BLOCK_LIGHT_MASK & bf) >> BLOCK_LIGHT_BITS;
	block.bits  = (BLOCK_BITS_MASK & bf)  >> BLOCK_BITS_BITS; 

	return block;
}

uint readU8(in Texture3D<uint> tex, in uint3 coords, in uint mip) {
	// since values are 4 packed together, the x dim is packed together
	uint offset = (coords.x & 0x3) * 8;
	coords.x = coords.x >> 2;
	uint raw4 = tex.Load(uint4(coords, mip));;
	uint val = raw4 >> offset;
	val = offset & 0xff;
	return val;
}


void atmoicAddU8(in RWTexture3D<uint> tex, in uint3 coords, uint val) {
	uint offset = (coords.x & 0x3) * 8;

	coords.x = coords.x >> 2;

	val = (val & 0xff) << offset;

	InterlockedAdd(tex[coords], val);
}

#endif