#ifndef RT_UTIL_H
#define RT_UTIL_H

// from ray tracing gem
struct HaltonState
{
  uint dimension;
  uint sequenceIndex;
};

float haltonSample(uint dimension, uint index)
{
  int base = 0;
  
  // Use a prime number.
  switch (dimension)
  {
  case 0:  base = 2;   break;
  case 1:  base = 3;   break;
  case 2:  base = 5;   break;
  default: base = 2;   break;
  }

  // Compute the radical inverse.
  float a = 0;
  float invBase = 1.0f / float(base);
  
  for (float mult = invBase;
       index != 0; index /= base, mult *= invBase)
  {
    a += float(index % base) * mult;
  }
  
  return a;
}

float haltonNext(inout HaltonState state)
{
  return haltonSample(state.dimension++, state.sequenceIndex);
}

// Modified from [pbrt]
uint halton3Inverse(uint index, uint digits)
{
  uint result = 0;
  for (uint d = 0; d < digits; ++d)
  {
    result = result * 3 + index % 3;
    index /= 3;
  }
  return result;
}

// Modified from [pbrt]
uint halton2Inverse(uint index, uint digits)
{
  index = (index << 16) | (index >> 16);
  index = ((index & 0x00ff00ff) << 8) | ((index & 0xff00ff00) >> 8);
  index = ((index & 0x0f0f0f0f) << 4) | ((index & 0xf0f0f0f0) >> 4);
  index = ((index & 0x33333333) << 2) | ((index & 0xcccccccc) >> 2);
  index = ((index & 0x55555555) << 1) | ((index & 0xaaaaaaaa) >> 1);
  return index >> (32 - digits);
}

// Modified from [pbrt]
uint haltonIndex(uint x, uint y, uint i)
{
	uint scale1 = 1;
	while(scale1 < min(x, 256)) {
		scale1 *= 2;
	}

	uint scale2 = 1;
	while(scale2 < min(y, 256)) {
		scale2 *= 3;
	}

  return ((halton2Inverse(x % 256, 8) * 76545 +
      halton3Inverse(y % 256, 6) * 110080) % (scale1 * scale2)) + i * 186624;
}

void haltonInit(inout HaltonState hState,
                int x, int y,
                int path, int numPaths,
                int frameId,
                int loop)
{
  hState.dimension = 2;
  hState.sequenceIndex = haltonIndex(x, y,
        (frameId * numPaths + path) % (loop * numPaths));
}

#endif