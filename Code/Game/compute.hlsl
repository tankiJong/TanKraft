#define IDENTITY_MATRIX float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)

float4x4 inverse(float4x4 m) {
    float n11 = m[0][0], n12 = m[1][0], n13 = m[2][0], n14 = m[3][0];
    float n21 = m[0][1], n22 = m[1][1], n23 = m[2][1], n24 = m[3][1];
    float n31 = m[0][2], n32 = m[1][2], n33 = m[2][2], n34 = m[3][2];
    float n41 = m[0][3], n42 = m[1][3], n43 = m[2][3], n44 = m[3][3];

    float t11 = n23 * n34 * n42 - n24 * n33 * n42 + n24 * n32 * n43 - n22 * n34 * n43 - n23 * n32 * n44 + n22 * n33 * n44;
    float t12 = n14 * n33 * n42 - n13 * n34 * n42 - n14 * n32 * n43 + n12 * n34 * n43 + n13 * n32 * n44 - n12 * n33 * n44;
    float t13 = n13 * n24 * n42 - n14 * n23 * n42 + n14 * n22 * n43 - n12 * n24 * n43 - n13 * n22 * n44 + n12 * n23 * n44;
    float t14 = n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 + n12 * n24 * n33 + n13 * n22 * n34 - n12 * n23 * n34;

    float det = n11 * t11 + n21 * t12 + n31 * t13 + n41 * t14;
    float idet = 1.0f / det;

    float4x4 ret;

    ret[0][0] = t11 * idet;
    ret[0][1] = (n24 * n33 * n41 - n23 * n34 * n41 - n24 * n31 * n43 + n21 * n34 * n43 + n23 * n31 * n44 - n21 * n33 * n44) * idet;
    ret[0][2] = (n22 * n34 * n41 - n24 * n32 * n41 + n24 * n31 * n42 - n21 * n34 * n42 - n22 * n31 * n44 + n21 * n32 * n44) * idet;
    ret[0][3] = (n23 * n32 * n41 - n22 * n33 * n41 - n23 * n31 * n42 + n21 * n33 * n42 + n22 * n31 * n43 - n21 * n32 * n43) * idet;

    ret[1][0] = t12 * idet;
    ret[1][1] = (n13 * n34 * n41 - n14 * n33 * n41 + n14 * n31 * n43 - n11 * n34 * n43 - n13 * n31 * n44 + n11 * n33 * n44) * idet;
    ret[1][2] = (n14 * n32 * n41 - n12 * n34 * n41 - n14 * n31 * n42 + n11 * n34 * n42 + n12 * n31 * n44 - n11 * n32 * n44) * idet;
    ret[1][3] = (n12 * n33 * n41 - n13 * n32 * n41 + n13 * n31 * n42 - n11 * n33 * n42 - n12 * n31 * n43 + n11 * n32 * n43) * idet;

    ret[2][0] = t13 * idet;
    ret[2][1] = (n14 * n23 * n41 - n13 * n24 * n41 - n14 * n21 * n43 + n11 * n24 * n43 + n13 * n21 * n44 - n11 * n23 * n44) * idet;
    ret[2][2] = (n12 * n24 * n41 - n14 * n22 * n41 + n14 * n21 * n42 - n11 * n24 * n42 - n12 * n21 * n44 + n11 * n22 * n44) * idet;
    ret[2][3] = (n13 * n22 * n41 - n12 * n23 * n41 - n13 * n21 * n42 + n11 * n23 * n42 + n12 * n21 * n43 - n11 * n22 * n43) * idet;

    ret[3][0] = t14 * idet;
    ret[3][1] = (n13 * n24 * n31 - n14 * n23 * n31 + n14 * n21 * n33 - n11 * n24 * n33 - n13 * n21 * n34 + n11 * n23 * n34) * idet;
    ret[3][2] = (n14 * n22 * n31 - n12 * n24 * n31 - n14 * n21 * n32 + n11 * n24 * n32 + n12 * n21 * n34 - n11 * n22 * n34) * idet;
    ret[3][3] = (n12 * n23 * n31 - n13 * n22 * n31 + n13 * n21 * n32 - n11 * n23 * n32 - n12 * n21 * n33 + n11 * n22 * n33) * idet;

    return ret;
}

struct light_info_t {
  float4 color;

  float3 attenuation;
  float dotInnerAngle;

  float3 specAttenuation;
  float dotOuterAngle;

  float3 position;
  float directionFactor;

  float3 direction;
  float __pad00;

  float4x4 vp;
};

struct vertex {
  float4 position;
};

cbuffer cCamera : register(b0) {
  float4x4 projection;
  float4x4 view;
};

cbuffer cLight: register(b1) {
  light_info_t light;
}

struct Ray {
	float3 position;
	float3 direction;
};

struct Contact {
	float4 position;
	float3 normal;
	float t;
	bool valid;
};

Ray GenPrimaryRay(float3 ndc) {
	Ray ray;
	
	float4x4 invView = inverse(view);
	float4 _worldCoords = mul(invView, mul(inverse(projection), float4(ndc, 1.f)));
	float3 worldCoords = _worldCoords.xyz / _worldCoords.w;

	ray.position = worldCoords;

	float3 origin = mul(invView, float4(0, 0, 0, 1.f)).xyz;
	ray.direction = normalize(worldCoords - origin);

	return ray;
}

Contact triIntersection(float3 a, float3 b, float3 c, float color, Ray ray) {

	Contact contact;

	float3 ab = b - a;
	float3 ac = c - a;
	float3 normal = normalize(cross(ac, ab));
	contact.normal = normal;

	contact.valid = dot(normal, ray.direction) < 0;

	float t = (dot(a - ray.position, normal)) / dot(normal, ray.direction);
	contact.t = t;
	contact.position.xyz = ray.position + t * ray.direction;
	contact.position.w = color;

	float3 p = contact.position.xyz;
	contact.valid = contact.valid && dot(cross(p - a, b - a), normal) > 0;
	contact.valid = contact.valid && dot(cross(p - b, c - b), normal) > 0;
	contact.valid = contact.valid && dot(cross(p - c, a - c), normal) > 0;

	return contact;
}


RWTexture2D<float4> Output: register(u0);
RWStructuredBuffer<float4> cVerts: register(u1);

struct Random {
	uint value;
	uint seed;
};

Random rnd(uint seed)
{
	Random re;
	re.value = seed;

	seed ^= (seed << 13);
  seed ^= (seed >> 17);
  seed ^= (seed << 5);

	re.seed = seed;
	return re;
}

Ray GenShadowRay(inout uint seed, Contact contact) {
	float MAX_UINT = 4294967296.0;

	float3 direction = contact.normal;

	Random r = rnd(seed);
	seed = r.seed;
	float x = float(r.value) * (1.0f / MAX_UINT)- .5f;
	 
	r = rnd(seed);
	seed = r.seed;
	float y = float(r.value) * (1.0f / MAX_UINT)- .5f;

	r = rnd(seed);
	seed = r.seed;
	float z = float(r.value) * (1.0f / MAX_UINT)- .5f;

	float3 right = float3(.33f, .33f, .33f) + float3(x,y,z);
	float3 _tan = normalize(right);
	float3 bitan = cross(_tan, contact.normal);
	float3 tan = cross(bitan, contact.normal);

	float3x3 tbn = transpose(float3x3(tan, contact.normal, bitan));

	r = rnd(seed);
	seed = r.seed;
	float b = abs(float(r.value) * (1.0f / MAX_UINT) - .5f) + .001f;

	r = rnd(seed);
	seed = r.seed;
	float a = float(r.value) * (1.0f / MAX_UINT)- .5f;

	r = rnd(seed);
	seed = r.seed;
	float c = float(r.value) * (1.0f / MAX_UINT) - .5f;
	
	float3 sample = normalize(mul(tbn, float3(a,b,c)));

	Ray ray;

	ray.direction = sample;
	ray.position = contact.position.xyz + contact.normal * 0.01f;

	return ray;
}

Contact trace(Ray ray) {
	uint vertCount, stride;
	cVerts.GetDimensions(vertCount, stride);

	Contact contact;
	contact.t = 1e6;
	contact.valid = false;

	for(uint i = 0; i < vertCount; i+=3) {
		Contact c = triIntersection(cVerts[i].xyz, cVerts[i+1].xyz, cVerts[i+2].xyz, cVerts[i].w, ray);
		bool valid = c.valid && (c.t < contact.t) && (c.t > 0.001f);	 // third is bias prevent from hitting self
		if(valid)	{
			contact = c;
		}
	}

	return contact;
}

static uint SSeed;

[numthreads(32, 32, 1)]
void main( uint3 threadId : SV_DispatchThreadID, uint groupIndex: SV_GroupIndex)
{

	uint2 pix = threadId.xy;
	uint2 size;
	Output.GetDimensions(size.x, size.y);

	if(pix.x >= size.x || pix.y >= size.y) return;

	float2 screen = float2(pix.x, size.y - pix.y);
	float2 ndcxy = float2(screen) / float2(size);
	ndcxy = ndcxy * 2.f - 1.f;
	float3 ndc = float3(ndcxy, 0.f);

	Ray primRay = GenPrimaryRay(ndc);

	Contact contact = trace(primRay);
	
	// now contact is the first intersect position
	// next spawn shadow ray and see whether it can hit the light

	float occlusion = 0;

	uint original;
	SSeed = threadId.x * 1024 + threadId.y + groupIndex;

	for(uint i = 0; i < 2; i++) {
		Ray ray = GenShadowRay(SSeed, contact);
		//ray.direction = float3(-0.5f, 0.5f, 0.f);
		if(dot(ray.direction, contact.normal) > 0 ) {
			Output[threadId.xy] = float4(1, 1, 1, 1.f);
		}	else {
			Output[threadId.xy] = float4(0, 0, 0, 1.f);
		}

		Contact c = trace(ray);

		bool occluded = c.valid;
		if(occluded) {
			// Output[threadId.xy] = float4(contact.position.w,contact.position.w,contact.position.w, 1.f);

			occlusion+= dot(contact.normal, ray.direction) / (c.t + 1.f);
		}
	}
	 		 
	occlusion = occlusion / 2.f;
	occlusion = 1.f - occlusion;
	/*	
	if(contact.valid) {
		float4 world = float4(contact.position.xyz, 1.f);
		float4 clip = mul(projection, mul(view, world));

		float3 ndc = clip.xyz / clip.w;
		*/
		float3 color = float3(occlusion, occlusion, occlusion);
		Output[threadId.xy] = float4(color, 1.f);
	// }					
}

