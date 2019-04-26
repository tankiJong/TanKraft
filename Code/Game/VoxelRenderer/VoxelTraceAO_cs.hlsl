#include "Common.hlsli"
#include "Debug.hlsli"
#include "VolumeUtil.hlsli"

#define RT_RootSig \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
     RootSig_Common  \
		 DebugDraw_RootSig \
		"DescriptorTable(SRV(t0, numDescriptors = 6), UAV(u0, numDescriptors = 1), visibility = SHADER_VISIBILITY_ALL)," \
    "StaticSampler(s0, filter = FILTER_ANISOTROPIC, maxAnisotropy = 1, visibility = SHADER_VISIBILITY_PIXEL)," 

Texture3D<uint> tVisible: register(t0);
Texture2D<float4> gTexNormal:   register(t1);
Texture2D<float4> gTexTangent:   register(t2);
Texture2D<float4> gTexBitangent:   register(t3);
Texture2D<float4> gTexPosition:   register(t4);
Texture2D<float4> gTexDepth:   register(t5);

RWTexture2D<float4> uTexAO: register(u0); 
static uint seed;

#define BD_X 16
#define BD_Y 16
#define BD_Z 256

#define BD	BD_X, BD_Y, BD_Z

static const uint2 kPlayerTileIndex = 16u.xx >> 1;
static const uint3 kPlayerChunk00Block = uint3(16u.xx * kPlayerTileIndex, 0);
static uint3 kPixCoords;

static float3 kVolumeAnchorPositionW;

void toVoxelCoords(float3 posW, out int2 chunkCoords, out uint3 blockCoords) {
	
	float3 div = posW / float3(BD);
	float2 chunkc = floor(div.xy);
	chunkCoords = chunkc;

	float3 offsetInChunk = posW - float3(chunkc * float2(BD_X, BD_Y), 0);
	float3 blockc = floor(offsetInChunk);
	blockCoords = blockc;
}

void init() {
 float4 playerPosition = mul(inverse(view), float4(0,0,0,1));
 playerPosition = playerPosition / playerPosition.w;

 int2 chunkCoords;
 uint3 blockCoords;
 toVoxelCoords(playerPosition.xyz, chunkCoords, blockCoords);

 float3 playerChunkAnchor = float3(chunkCoords * 16.f, 0);

 kVolumeAnchorPositionW = playerChunkAnchor - float3(16 * 8, 16 * 8, 0);
}



// http://iquilezles.org/www/articles/boxfunctions/boxfunctions.htm
// http://www.iquilezles.org/www/articles/intersectors/intersectors.htm
// axis aligned box centered at the origin, with size boxSize
float2 boxIntersection( in float3 ro, in float3 rd, float3 boxSize, out float3 outNormal ) 
{
    float3 m = 1.0/rd;   // could precompute if traversing a big set of algined boxes
    float3 n = m*ro;     // could precompute if traversing a big set of algined boxes
    float3 k = abs(m)*boxSize;
	
    float3 t1 = -n - k;
    float3 t2 = -n + k;

    float tN = max( max( t1.x, t1.y ), t1.z );
    float tF = min( min( t2.x, t2.y ), t2.z );
	
    if( tN > tF || tF < 0.f) return float2(-1.f.xx); // no intersection
    
    outNormal = -sign(rd)*step(t1.yzx,t1.xyz)*step(t1.zxy,t1.xyz);

    return float2( tN, tF );
}

float2 aabbIntersect(in float3 ro, in float3 rd, float3 mins, float3 maxs, out float3 oNorm) {
	 float3 center = (mins + maxs) * .5f;
	 ro = ro - center;
	 return boxIntersection(ro, rd, maxs - mins, oNorm);
}

struct Contact {
	float4 pos1; // mint
	float4 pos2; // maxt
	float3 norm;
	bool valid() { return pos1.w <= pos2.w; }
};

struct VContact {
	Contact contact;
	uint3 vCoords;
};

VContact miss() {
	VContact c;
	Contact cc;
	cc.pos1 = 1.f.xxxx;
	cc.pos2 = -1.f.xxxx;
	cc.norm = 0.f.xxx;

	c.contact = cc;
	c.vCoords = 0u.xxx;

	return c;
}
struct VRay {
	float3 direction;
	float  mint;
	float3 origin;
	float  maxt;
};

struct aabb3 {
	float3 mins;
	float3 maxs;

	Contact intersect(VRay ray) {
		Contact c;
		float2 ts = aabbIntersect(ray.origin, ray.direction, mins, maxs, c.norm);
		float mint = min(ts.x, ts.y);
		float maxt = max(ts.x, ts.y);

		float3 pos1 = ray.origin + mint * ray.direction;
		float3 pos2 = ray.origin + maxt * ray.direction;

		c.pos1 = float4(pos1, mint);
		c.pos2 = float4(pos2, mint);

		return c;
	}

	bool contains(float3 position) {
		return all(mins <= position) && all(position < maxs);
	}
};

aabb3 voxelBounds(uint3 coords, uint mip) {
	uint size = 1 << mip;
	aabb3 bounds;
	bounds.mins = coords * size;
	bounds.maxs = bounds.mins + size.xxx;
	return bounds;
}


int3 toVolumeCoords(float3 posVol, uint mip) {
	return floor(posVol / float(1 << mip).xxx);
}

float3 toVolumePosition(float3 posW) {
	return posW - kVolumeAnchorPositionW;
}

float3 toWorldPosition(float3 posVol) {
	return posVol + kVolumeAnchorPositionW;
}

void DebugDrawVoxel(uint3 volumeCoords, uint mip, float4 color = float4(1.f, 0.f, 0.f, .5f)) {
	aabb3 volumeBounds = voxelBounds(volumeCoords, mip);
	DebugDrawCube(toWorldPosition(volumeBounds.mins), toWorldPosition(volumeBounds.maxs), color);
}


uint voxelSize(uint mip) {
	return 1 << mip;
}

#define MAX_T 10000
float3 maxT(float3 originVol, float3 direction, uint mip) {
	float3 relativePosition = originVol / float(voxelSize(mip));
	float3 dist = floor(relativePosition + clamp(sign(direction), 0.f.xxx, 1.f.xxx)) - relativePosition;
	return dist / direction;
}

float3 deltaT(float3 direction, uint mip) {
	return voxelSize(mip).xxx / direction * sign(direction);
}

bool inRange(int3 volumeCoords, uint mip) {
	return all(volumeCoords < int3( 256u.xxx / (1 << mip))) && all(volumeCoords >= 0);
}

bool isOpaque(uint3 coords, uint mip) {
	uint val = 	tVisible.mips[mip][coords];
	return val != 0;
}

// assume I am in the volume, trace it.
// asume in volume space, which means voxel(0,0,0) right, bottom, backward corner is (0,0,0)
VContact trace(VRay ray) {
	int3 step = sign(ray.direction);
	
	float3 maxt = maxT(ray.origin, ray.direction, 0);
	float3 deltat = deltaT(ray.direction, 0);

	float3 norm = 0.f.xxx;
	// int3 volumeCoords = volCoords;
	int3 volumeCoords = toVolumeCoords(ray.origin, 0);
	{
		float t = min(maxt.x, min(maxt.y, maxt.z));
		float3 world = toWorldPosition(ray.origin);
		DebugDrawLine(world, world + ray.maxt * ray.direction, float4(1,1,0,1), float4(1,1,0,1), 0.f.xx, 0.f.xx);
	}
	//if(isOpaque(volumeCoords, 0)) {
	//	float t = 0;
	//	VContact c;
	//	c.vCoords = volumeCoords;
	//	c.contact.norm = norm;
	//	c.contact.pos1 = float4(ray.origin, 0);
	//	DebugDrawPoint(toWorldPosition(c.contact.pos1.xyz), float4(1.f, 0.f, 0.f, 1));			
	//	DebugDrawVoxel(volumeCoords, 0, float4(0, 1, 1, 1));
	//	c.contact.pos2 = 0.f.xxxx;												 
	//	return c;
	//}

	while( any(maxt < ray.maxt.xxx) ) {

		// DebugDrawPoint(toWorldPosition(ray.origin +min(maxt.x, min(maxt.y, maxt.z)) * ray.direction), float4(1.f, 0.f, 0.f, 1.f));
		// volumeCoords.x += step.x;
		DebugDrawVoxel(volumeCoords, 0, float4(0, 0, 1, 1));

		if(maxt.x <= maxt.y && maxt.x <= maxt.z) {
			volumeCoords.x += step.x;
			norm = float3(-step.x, 0, 0);
		} else if(maxt.y <= maxt.x && maxt.y <= maxt.z) {
			volumeCoords.y += step.y;
			norm = float3(0, -step.y, 0);
		} else {
			volumeCoords.z += step.z;
			norm = float3(0, 0, -step.z);
		}

		if(isOpaque(volumeCoords, 0)) {
			float t = min(maxt.x, min(maxt.y, maxt.z));
			VContact c;
			c.vCoords = volumeCoords;
			c.contact.norm = norm;
			c.contact.pos1 = float4(ray.origin + ray.direction * t, t);
			DebugDrawPoint(toWorldPosition(c.contact.pos1.xyz), float4(1.f, 1, 1.f, 1));			
			c.contact.pos2 = t.xxxx;
			return c;
		} 

		if(!inRange(volumeCoords, 0)) break;


		if(maxt.x <= maxt.y && maxt.x <= maxt.z) {
			maxt.x += deltat.x;
		} else if(maxt.y <= maxt.x && maxt.y <= maxt.z) {
			maxt.y += deltat.y;
		} else {
			maxt.z += deltat.z;
		}
		// maxt.x += deltat.x;

	}

	return miss();
}


float origin() { return 1.0f / 32.0f; }
float float_scale() { return 1.0f / 65536.0f; } 
float int_scale()   { return 256.0f; } 

float3 offset_ray(float3 p, float3 n) { 
	int3 of_i = int_scale() * int3(n);
	float3 p_i = asfloat(asint(p) + sign(p) * of_i);

	return float3(abs(p.x) < origin() ? p.x+ float_scale()*n.x : p_i.x,
							  abs(p.y) < origin() ? p.y+ float_scale()*n.y : p_i.y,
								abs(p.z) < origin() ? p.z+ float_scale()*n.z : p_i.z);
}


 

float3 GetCameraPosition() {
	float4x4 inverView = inverse(view);
	float4 world = mul(inverView, float4(0.f.xxx, 1.f));
	return world.xyz / world.w;
}




[RootSignature(RT_RootSig)]
[numthreads(32, 8, 1)]
void main( uint3 pixelCoords : SV_DispatchThreadID )
{
	enableDebug = (pixelCoords.x == (1620 >> 1) && pixelCoords.y == (972 >> 1));
	// seed = pixelCoords.x * 0xefcca + 0xff884 * pixelCoords.y + 0xab66f * pixelCoords.z;
	seed = pixelCoords.x * 901003 + 121143 * pixelCoords.y + 179931 * pixelCoords.z;
	kPixCoords = pixelCoords;
	init();
	uint2 dim, _;
	gTexDepth.GetDimensions(dim.x, dim.y);

	float4 playerPosition = mul(inverse(view), float4(0,0,0,1));
	playerPosition = playerPosition / playerPosition.w;

	float depth = gTexDepth[pixelCoords.xy].x;
	float2 uv = float2(pixelCoords.xy) / float2(dim);
	
	if(any(pixelCoords.xy > dim)) return;

	float3 posW = depthToWorld(uv, depth);

	float3 pixW = depthToWorld(uv, 0);

	DebugDrawLine(pixW, posW);

	float3 normal = gTexNormal[pixelCoords.xy].xyz;
	float3 tangent = gTexTangent[pixelCoords.xy].xyz;
	float3 bitangent = gTexBitangent[pixelCoords.xy].xyz;
	normal = normal * 2.f - 1.f;
	tangent = tangent * 2.f - 1.f;
	bitangent = bitangent * 2.f - 1.f;


	//float3 voxelPos = toVolumePosition(posW) - 0.1f * GetRandomDirection(seed, normal, tangent, bitangent);
	//int3 voxelCoords = toVolumeCoords(voxelPos, 0);
	//voxelCoords.z = 109;
	//for(int i = 0; i < 1; i++) {
	//	for(int j = -10; j < 10; j++) {
	//		for(int k = -10; k < 10; k++) {
	//			// enableDebug = true;
	//			// DebugDrawPoint(posW, float4(1.f, 0.f, 0.f, 1.f));
	//			if(!isOpaque(voxelCoords + int3(i, j, k), 0)) {
	//				DebugDrawVoxel(voxelCoords + int3(i, j, k), 0, float4(0.f, 1.f, 1.f, 1));
	//			}
	//		}
	//	}
	//}
	

	float ao = 0;
	for(uint i = 0; i < 1; i++) {
		VRay ray;
		ray.origin = toVolumePosition(posW);
		if(any(ray.origin >= 255.f) || any(ray.origin < 0.f)){
			continue;
		}	
		ray.direction = GetRandomDirection(seed, normal, tangent, bitangent);
		float3 maxt = maxT(ray.origin, ray.direction, 0);

		// ray.origin = ray.origin + ray.direction * min(maxt.x, min(maxt.y, maxt.z));
		ray.origin = offset_ray(ray.origin, normal);
		ray.maxt = 1.f;
		// DebugDrawVoxel(toVolumeCoords(ray.origin, 0), 0, float4(.5, .2, 1, 1));
		VContact c = trace(ray);
		//if(c.contact.valid()) {
		//	float3 tan = normalize(.5f.xxx);
		//	float3 bitan = normalize(cross(tan, c.contact.norm));
		//	VRay rray;
		//	rray.direction = GetRandomDirection(seed, c.contact.norm, tan, bitan);
		//	rray.origin = c.contact.pos1.xyz;
		//	rray.origin += ray.direction * 0.05f;
		//	if(all(ray.origin < 255.f) && all(ray.origin > 0.f)){
		//		ray.maxt = 10.f;
		//		c = trace(ray);
		//	}
		//}
		ao += c.contact.valid() ? 1.f : 0.f;
	}

	ao = 1.f - ao / 1.f;
	uTexAO[pixelCoords.xy] = float4(ao.xxx, 1.f);

	
}