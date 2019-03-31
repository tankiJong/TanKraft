struct Ray {
	float3 direction;
	float3 origin;
	uint2  chunkCoords;
	uint3  blockCoords;
};