#version 450
//! #extension GL_KHR_vulkan_glsl : enable

// ------------ GLOBAL CONSTS -----------------
const float PI = 3.14159265358979;
const float INV_PI = 1 / PI;

// ------------ FUNCTION DEFINITIONS -----------------
vec4 GetImportanceSampleGGX(in vec2 Xi, in vec3 normalWS, in float roughness);
float DistributionGGX(float cosTheta, float roughness);
vec3 TangentToWorld(in vec3 normal, in vec3 tangentSample);
float RadicalInverse_VdC(in uint bits);
vec2 Hammersley(in uint i, in uint N);

// Input
layout(location = 0) in vec3 fragLocalPosition;

// Output
layout(location = 0) out vec4 outColor;

// Descriptor Bindings
layout(set = 0, binding = 0) uniform textureCube environmentMapImage; // 2D HDRI Map
layout(set = 0, binding = 1) uniform sampler environmentMapSampler;

layout(push_constant) uniform PushConstants {
    mat4 projection;
    mat4 view;
	uint mipLevel;
} pushConstants;

void main()
{
	// The sample direction equals the hemisphere's orientation
	vec3 N = normalize(fragLocalPosition);
	N.y *= -1.f; // Important we flip here as in lighting pass
	const vec3 V = N;
	const vec3 R = N;

	const float roughness = min(pushConstants.mipLevel * 0.125f, 1);
	vec3 prefilteredColor = vec3(0.f);
	float totalWeight = 0.0f;
	const uint sampleCount = 1024;
	for(uint i = 0; i < sampleCount; ++i)
	{
		// 0. Get random seed for sample
		const vec2 Xi = Hammersley(i, sampleCount);

		// 1. Get sample and calculate pdf
		const vec4 sampleInfo = GetImportanceSampleGGX(Xi, N, roughness);
		const vec3 H = sampleInfo.xyz;
		const float pdf = sampleInfo.w;

		// 2. Determine mip level (lod) by:
		// - solid angle texel (saTexel)
		// = 4 * PI / (6 * w * h) -- > 4PI is total solid angle sphere and we have cube of 6 faces with a resolution
		// - solid angle single sample (saSample)
		// =1 / (sampleCount * pdf)
		// - lod is derived ratio of sample area to texel area. Using log2 to relate area difference to mip level and
		// zero check for safety.
		const float baseWidth = textureSize(samplerCube(environmentMapImage, environmentMapSampler), 0).x;
		const float saTexel = 4.f * PI / (6.f * baseWidth * baseWidth);
		const float saSample = 1.f / (sampleCount * pdf);
		const float lod = roughness == 0.f ? 0.f : 0.5f * log2(saSample / saTexel);

		// 3. Filter from environment map by reflecting around sample direction. Here V is N because we record for probe.
		const vec3 L = normalize(reflect(-V, H));
		const float NdotL = dot(N, L);
		if (NdotL > 0.0)
		{
			// Sample from environment map
			const vec3 sampleColor = textureLod(
			samplerCube(environmentMapImage, environmentMapSampler),
			L, lod).rgb;

			// Accumulate samples
			prefilteredColor += sampleColor * NdotL;
			totalWeight += NdotL;
		}
	}
	// Reweight by weight and return
	if(totalWeight > 0.0f)
		prefilteredColor /= totalWeight;
	outColor = vec4(prefilteredColor, 1.f);
}

vec4 GetImportanceSampleGGX(in vec2 Xi, in vec3 normalWS, in float roughness)
{
// Get sample (in spherical coordinates) based on GGX normal distribution
	float a = roughness * roughness;
	const float phi = 2.f * PI * Xi.x;
	const float cosTheta = clamp(sqrt((1.f - Xi.y) / (1.f + (a * a - 1.f) * Xi.y)), 0.001f, 1.f);
	const float sinTheta = sqrt(1.f - cosTheta * cosTheta);

	// Sample from spherical coordinates to cartesian coordinates
	const vec3 sampleTS = vec3(
	cos(phi) * sinTheta,
	sin(phi) * sinTheta,
	cosTheta);

	// Calculate the pdf based on GGX normal distribution. See:
	// - https://bruop.github.io/ibl/
	// - https://developer.nvidia.com/gpugems/gpugems3/part-iii-rendering/chapter-20-gpu-based-importance-sampling
	float pdf = DistributionGGX(cosTheta, roughness);
	pdf /= 4.f; // Apply the Jacobian to obtain a pdf that is parameterized by 1

	// From tangent-space vector to world-space sample vector
	const vec3 sampleDirectionWS = TangentToWorld(normalWS, sampleTS);

	// Return sample and pdf (for later use for LOD selection)
	return vec4(normalize(sampleDirectionWS), pdf);
}

float DistributionGGX(float cosTheta, float roughness)
{
	float a = roughness * roughness;

    float denom = cosTheta * cosTheta * (a - 1.0) + 1.0;

    return a / (PI * denom * denom);
}

vec3 TangentToWorld(in vec3 normal, in vec3 tangentSample)
{
	const vec3 up = normalize(abs(normal.y)) < 0.999f
		? vec3(0.f, -1.f, 0.f) : vec3(1.f, 0.f, 0.f);
	const vec3 tangent = normalize(cross(normal, up));
	const vec3 bitangent = cross(tangent, normal);

	return vec3(
		tangentSample.x * tangent +
		tangentSample.y * bitangent +
		tangentSample.z * normal);
}

float RadicalInverse_VdC(in uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley(in uint i, in uint N)
{
	return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}