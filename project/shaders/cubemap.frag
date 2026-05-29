#version 450
//! #extension GL_KHR_vulkan_glsl : enable

// Input
layout(location = 0) in vec3 fragLocalPosition;

// Output
layout(location = 0) out vec4 outColor;

// Descriptor Bindings
layout(set = 0, binding = 0) uniform texture2D hdriImage; // 2D HDRI Map
layout(set = 0, binding = 1) uniform sampler hdriSampler;

// Functions
const vec2 gInvAtan = vec2(0.1591f, 0.3183f);
vec2 SampleSphericalMap(in vec3 v)
{
	vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
	uv *= gInvAtan;
	uv += 0.5f;
	return uv;
}

// Shader
void main()
{
	// Get our position and use it as a direction to sample. Due to some
	// coordinate system changes/decisions, I need to swap and flip some
	// axis when loading in the IBL data ...
	vec3 direction = normalize(fragLocalPosition);
	direction = vec3(direction.z, direction.y, -direction.x);
	vec2 uv = SampleSphericalMap(direction);
	outColor = vec4(texture(sampler2D(hdriImage, hdriSampler), uv).rgb, 1.f);
}