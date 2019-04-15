#include "Common.hlsli"

#define RT_RootSig \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), " \
     RootSig_Common  \
		"DescriptorTable(SRV(t0, numDescriptors = 5), UAV(u0, numDescriptors = 1), visibility = SHADER_VISIBILITY_ALL)," \
    "StaticSampler(s0, filter = FILTER_ANISOTROPIC, maxAnisotropy = 1, visibility = SHADER_VISIBILITY_PIXEL)," 

Texture3D<uint> tVisible: register(t0);
Texture2D<float4> gTexNormal:   register(t1);
Texture2D<float4> gTexTangent:   register(t2);
Texture2D<float4> gTexBitangent:   register(t3);
Texture2D<float4> gTexDepth:   register(t4);

RWTexture2D<float4> uTexAO: register(u0); 

RWTexture2D<float4> uOutput: register(u0);

#define BD_X 16
#define BD_Y 16
#define BD_Z 256

#define BD	BD_X, BD_Y, BD_Z

static uint seed;

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
	bool valid() { return pos1.w >= pos2.w; }
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
	float3 origin;
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

aabb3 voxelBound(uint3 coords, uint mip) {
	uint size = 1 << mip;
	aabb3 bounds;
	bounds.mins = coords * size;
	bounds.maxs = (coords + 1u.xxx) * size;
	return bounds;
}

void toWorldCoords(float3 posW, out uint2 chunkCoords, uint3 blockCoords) {
	
	float3 div = posW / float3(BD_X,BD_Y, BD_Z);
	float3 chunkc = floor(div);
	chunkCoords = chunkc.xy;

	float3 offsetInChunk = posW - chunkc * float3(BD);
	float3 blockc = floor(offsetInChunk);
	blockCoords = blockc;
}

float3 toVolumeSpace(float3 posV) {
	return posV + float3(128.f.xx, 0);
}

float3 toViewSpace(float3 posVol) {
	return posVol - float3(128.f.xx, 0);
}

uint3 toVolumeCoords(float3 posV, uint mip = 0) {

	// transform the player to center of the voxel
	posV = toVolumeSpace(posV);
	uint3 coords = floor(posV);

	return coords >> mip;
}

uint3 stepCoords(uint3 coords, float3 dir, uint mip) {
	return coords;
}

bool inRange(float3 posV) {
	return all(posV <= 127.f.xxx) && all(posV >= -128.f.xxx);
}



VContact trace(VRay ray) {
	return miss();
	float3 nextposV = ray.origin;
	uint currentMip = 8;
	uint3 vCoords = toVolumeCoords(nextposV, currentMip);
	while(inRange(nextposV)) {
		if(currentMip == 0) {
			aabb3 bounds = voxelBound(vCoords, 0);
			VContact vc;
			vc.vCoords = vCoords;
			vc.contact = bounds.intersect(ray);
			
		}
		
		if(tVisible.Load(uint4(vCoords, currentMip)) == 0) {
			vCoords = stepCoords(vCoords, ray.direction, currentMip);
			aabb3 bounds = voxelBound(vCoords, currentMip);
			Contact c = bounds.intersect(ray);
			nextposV = c.pos2.xyz;
		}	else {
			currentMip--;
			vCoords	= toVolumeCoords(nextposV, currentMip);
		}
	}

	return miss();
}


 

float3 GetCameraPosition() {
	float4x4 inverView = inverse(view);
	float4 world = mul(inverView, float4(0.f.xxx, 1.f));
	return world.xyz / world.w;
}





[RootSignature(RT_RootSig)]
[numthreads(16, 16, 1)]
void main( uint3 pixelCoords : SV_DispatchThreadID )
{
	seed = pixelCoords.x * 901003 + 121143 * pixelCoords.y + 179931 * pixelCoords.z;
	
	uint2 dim, _;
	gTexDepth.GetDimensions(dim.x, dim.y);

	float depth = gTexDepth[pixelCoords.xy].x;
	float2 uv = float2(pixelCoords.xy) / float2(dim);
	
	float3 posV = depthToView(uv, depth);

	float3 normal = gTexNormal[pixelCoords.xy].xyz;
	float3 tangent = gTexTangent[pixelCoords.xy].xyz;
	float3 bitangent = gTexBitangent[pixelCoords.xy].xyz;
	normal = normal * 2.f - 1.f;
	tangent = tangent * 2.f - 1.f;
	bitangent = bitangent * 2.f - 1.f;

	VRay ray;
	ray.origin = toVolumeSpace(posV);
	ray.direction = GetRandomDirection(seed, normal, tangent, bitangent);

	VContact c = trace(ray);
	
}