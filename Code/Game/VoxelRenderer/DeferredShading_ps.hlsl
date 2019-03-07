#include "DeferredShading.hlsli"
#include "../SceneRenderer/Lighting.hlsli"



#define _in(T) const in T
#define _inout(T) inout T
#define _out(T) out T
#define _begin(type) type (
#define _end )


struct ray_t {
	float3 origin;
	float3 direction;
};
#define BIAS 1e-4 // small offset to avoid self-intersections

struct sphere_t {
	float3 origin;
	float radius;
	int material;
};

struct plane_t {
	float3 direction;
	float distance;
	int material;
};

ray_t get_primary_ray(
	_in(float3) cam_local_point,
	_inout(float3) cam_origin,
	_inout(float3) cam_look_at
){
	float3 fwd = normalize(cam_look_at - cam_origin);
	float3 up = float3(0, 1, 0);
	float3 right = cross(up, fwd);
	up = cross(fwd, right);

	ray_t r;
	r.origin = cam_origin,
	r.direction = normalize(fwd + up * cam_local_point.y + right * cam_local_point.x);
	return r;
}

bool isect_sphere(_in(ray_t) ray, _in(sphere_t) sphere, _inout(float) t0, _inout(float) t1)
{
	float3 rc = sphere.origin - ray.origin;
	float radius2 = sphere.radius * sphere.radius;
	float tca = dot(rc, ray.direction);
	float d2 = dot(rc, rc) - tca * tca;
	if (d2 > radius2) return false;
	float thc = sqrt(radius2 - d2);
	t0 = tca - thc;
	t1 = tca + thc;

	return true;
}

#define earth_radius gPlanetRadius	 /*meter*/
#define atmosphere_radius (gPlanetRadius + gPlanetAtmosphereThickness) /*meter*/

// scattering coefficients at sea level (m)
#define betaR gRayleigh /* Rayleigh */
#define betaM gMie /*Mie	*/

// scale height (m)
// thickness of the atmosphere if its density were uniform
// #define hR 7994.0
// #define hM 1200.0

#define hR gScatterThickness.x
#define hM gScatterThickness.y
float rayleigh_phase_func(float mu)
{
	return
			3. * (1. + mu*mu)
	/ //------------------------
				(16. * PI);
}

// Henyey-Greenstein phase function factor [-1, 1]
// represents the average cosine of the scattered directions
// 0 is isotropic scattering
// > 1 is forward scattering, < 1 is backwards
#define g 0.76f
float henyey_greenstein_phase_func(float mu)
{
	return
						(1.f - g*g)
	/ //---------------------------------------------
		((4.f + PI) * pow(1. + g*g - 2.*g*mu, 1.5));
}

// Schlick Phase Function factor
// Pharr and  Humphreys [2004] equivalence to g above
#define k 1.55*g - 0.55 * (g*g*g)
float schlick_phase_func(float mu)
{
	return
					(1. - k*k)
	/ //-------------------------------------------
		(4. * PI * (1. + k*mu) * (1. + k*mu));
}

#define sun_dir		gSunDir
#define sun_power gSunPower


sphere_t atmosphere() {
	sphere_t s;
	s.origin = float3(0, 0, 0);
	s.radius = atmosphere_radius;
	s.material = 0;
	return s;
};

#define num_samples 16
#define num_samples_light 16

bool get_sun_light(
	_in(ray_t) ray,
	_inout(float) optical_depthR,
	_inout(float) optical_depthM
){
	float t0 = 0, t1 = 0;
	isect_sphere(ray, atmosphere(), t0, t1);

	float march_pos = 0.;
	float march_step = t1 / float(num_samples_light);

	sphere_t earth = atmosphere();
	for (int i = 0; i < num_samples_light; i++) {
		float3 s =
			ray.origin +
			ray.direction * (march_pos + 0.5 * march_step);
		float height = distance(s, earth.origin) - earth_radius;
		if (height < 0.)
			return false;

		optical_depthR += exp(-height / hR) * march_step;
		optical_depthM += exp(-height / hM) * march_step;

		march_pos += march_step;
	}

	return true;
}


float3 get_incident_light(_in(ray_t) ray, float end)
{

	float march_step = end / float(num_samples);

	// cosine of angle between view and light directions
	float mu = dot(ray.direction, sun_dir);

	// Rayleigh and Mie phase functions
	// A black box indicating how light is interacting with the material
	// Similar to BRDF except
	// * it usually considers a single angle
	//   (the phase angle between 2 directions)
	// * integrates to 1 over the entire sphere of directions
	float phaseR = rayleigh_phase_func(mu);
	float phaseM =
#if 1
		henyey_greenstein_phase_func(mu);
#else
		schlick_phase_func(mu);
#endif

	// optical depth (or "average density")
	// represents the accumulated extinction coefficients
	// along the path, multiplied by the length of that path
	float optical_depthR = 0.;
	float optical_depthM = 0.;

	float3 sumR = 0.f.xxx;
	float3 sumM = 0.f.xxx;
	float march_pos = 0.;

	sphere_t earth = atmosphere();
	for (int i = 0; i < num_samples; i++) {
		float3 s =
			ray.origin +
			ray.direction * (march_pos + 0.5 * march_step);
		float height = distance(s, earth.origin) - earth_radius;
		// integrate the height scale
		float hr = exp(-height / hR) * march_step;
		float hm = exp(-height / hM) * march_step;
		optical_depthR += hr;
		optical_depthM += hm;

		// gather the sunlight
		ray_t light_ray;
		light_ray.origin = s;
		light_ray.direction = sun_dir;

		float optical_depth_lightR = 0.;
		float optical_depth_lightM = 0.;
		bool overground = get_sun_light(
			light_ray,
			optical_depth_lightR,
			optical_depth_lightM);

		if (overground) {
			float3 tau =
				betaR * (optical_depthR + optical_depth_lightR) +
				betaM * 1.1 * (optical_depthM + optical_depth_lightM);
			float3 attenuation = exp(-tau);
			sumR += hr * attenuation;
			sumM += hm * attenuation;

		}
		march_pos += march_step;
	}

	// float timeInDay = gTime / 86400.f;
	// timeInDay = timeInDay - floor(timeInDay);
	// float ambientStrength = lerp(.1, 2, -cos(2 * 3.1415926 * timeInDay) * .5f + .5f);
	return
		sun_power * 
		(sumR * phaseR * betaR +
		sumM * phaseM * betaM);
}


float3 get_incident_light(_in(ray_t) ray)
{
	// "pierce" the atmosphere with the viewing ray
	float t0 = 0, t1 = 0;
	if (!isect_sphere(
		ray, atmosphere(), t0, t1)) {
		return 0.f.xxx;
	}

	return get_incident_light(ray, t1);
}


float3 jodieReinhardTonemap(float3 c){
    float l = dot(c, float3(0.2126, 0.7152, 0.0722));
    float3 tc = c / (c + 1.0);

    return lerp(c / (l + 1.0), tc, tc);
}
	// https://www.shadertoy.com/view/XtBXDz
float3 SkyColor(float height, float3 direction, float end) {

	height = earth_radius + (height - gPlanetRadius) / 256 * (atmosphere_radius - earth_radius) * .1f;

	ray_t ray;
	ray.origin = float3(0, height, 0);
	ray.direction = direction;

	sphere_t s;
	s.origin = float3(0,0,0);
	s.radius = gPlanetRadius;
	s.material = 0;

	float t0 = 0, t1 = 0;
	
	// if (isect_sphere(ray, s, t0, t1) && t0 > 0) {
	// 	return .333f.xxx;
	// } else {
		return (get_incident_light(ray, end));
	// }
}


float3 depthToWorld(float2 uv, float depth) {
	float4x4 vp = mul(projection, view);
	float4x4 inverCamVP = inverse(vp);
	float2 _uv = float2(uv.x, 1 - uv.y);
	float3 ndc = float3(_uv * 2.f - 1.f, depth);
	float4 world = mul(inverCamVP, float4(ndc, 1.f));
	world = world / world.w;

	return world.xyz;
}

[RootSignature(DeferredShading_RootSig)]
PSOutput main(PostProcessingVSOutput input)
{

	PSOutput output;

	float4 color = gTexAlbedo.Sample(gSampler, input.tex);
	color = pow(color, 2.2f);
	// float4 color = float4(1, 1, 1, 1);
	float depth = gTexDepth.Sample(gSampler, input.tex).x;
	float3 position = depthToWorld(input.tex, depth);

	float3 normal = gTexNormal.Sample(gSampler, input.tex).xyz;
	normal = normal  * 2.f - 1.f;
	
	float4 camPosition = mul(inverse(view), float4(0,0,0,1));
	camPosition /= camPosition.w;

	float distFromCam = distance(camPosition.xyz, position);

	const float3 planetCenterW = float3(0, 0, -gPlanetRadius);
	const float3 planetCenterV = planetCenterW - camPosition.xyz;
	// transform to planet space	()

	float3 world = depthToWorld(input.tex, .5f);

	float3 direction = normalize(world - camPosition.xyz);

	// R = mul(flipAxis, R);
	// direction = mul(flipAxis, direction);
	float3x3 flipAxis = {
		    0, -1, 0,
		    0,  0, 1,
		    1,  0, 0
	};
	float factor = length(normal) < 0.5f ? 0 : 1;
	float3 sampleDirection = clamp(normalize(direction), -1.f.xxx, 1.f.xxx);
	float3 samplePosition = position * factor + camPosition.xyz * (1 - factor);
	// float4 sky = gSky.Sample(gSampler, samplePosition);
	float3 cameraOnPlanet = -planetCenterV;

		float height = gPlanetRadius + samplePosition.z;
		float3 fdirection = float3(-sampleDirection.y, sampleDirection.z, sampleDirection.x);
		
		float t0 = 0, t1 = 0;

		ray_t ray;
		ray.origin = camPosition.xyz;
		ray.direction = fdirection;

		isect_sphere( ray, atmosphere(), t0, t1 );
		// float4 sceneScatter = float4(SkyColor(height, fdirection, distFromCam), 1.f);
		float4 skyScatter = float4(SkyColor(height, fdirection, t1), 1.f);
	

	float3 tan = normalize(gTexTangent.Sample(gSampler, input.tex).xyz * 2.f - 1.f);
	// float3 bitan = gTexBitangent.Sample(gSampler, input.tex).xyz * 2.f - 1.f;
	float3 bitan = normalize(cross(tan, normal));
	// output.color = gTexTangent.Sample(gSampler, input.tex);
	// return output;

	uint total,_;
	gLights.GetDimensions(total, _);


	// float3 eye = normalize(camPosition.xyz - position);

	// float3 finalColor = color.xyz * .1f;
	float3 finalColor = 0.f.xxx;
	float3 diffuse = 0.f.xxx, specular = 0.f.xxx;
	/*
	for(uint i = 0; i < total; i++) {
		light_info_t l = gLights[i];
		// l.position.y += sin(gTime);
		// finalColor += Diffuse(position, normal, color.xyz, l);
		// finalColor += Specular(position, normal, eye, .1, 3, l);
		 finalColor += 
			pbrDirectLighting(color.xyz, gRoughness, gMetallic, position, camPosition.xyz, normal, tan, bitan, l, diffuse, specular);
	}
	 */
	// finalColor += pbrIndirectLighting(diffuse, 1.f.xxx, 1.f.xxx);
	// finalColor += pbrEnvironmentLighting(color.xyz, normal, gRoughness, gMetallic, 1, eye, 1.f.xxx);
	float timeInDay = gTime / 86400.f;
	timeInDay = timeInDay - floor(timeInDay);
	float ambientStrength = lerp(0.f, 1.f, -cos(2 * 3.1415926 * timeInDay) * .5f + .5f);

	float ao = gTexAO.Sample(gSampler, input.tex).x;
	finalColor += color.xyz * ao;
	ao = clamp(0, 1, ao + lerp(.5f, 0.f, ambientStrength));

	finalColor = saturate(finalColor);


	float skyBlendFactor = (distFromCam - gViewDistance.x) / (gViewDistance.y - gViewDistance.x);
	skyBlendFactor = clamp(skyBlendFactor, 0, 1);

	// output.color = float4(skyBlendFactor, 0, 0, 1);
	// return output;
	skyBlendFactor = smoothstep(0, 1, skyBlendFactor);

	finalColor = finalColor * ao * ( 1 - skyBlendFactor) + skyScatter.xyz * skyBlendFactor;
	// finalColor = lerp(finalColor, 1.f.xxx, gWorldConstant.x);

	output.color = float4( finalColor, 1 )  * factor
	 								+ skyScatter * (1 - factor);

	return output;
}