#version 450
//! #extension GL_KHR_vulkan_glsl : enable

// ------------ GLOBAL CONSTS -----------------
const float PI = 3.14159265358979;
const float INV_PI = 1 / PI;

// ------------ FUNCTION DEFINITIONS -----------------

float saturate(float x) {
    return clamp(x, 0.0, 1.0);
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

vec2 hammersley(in uint i, in uint N)
{
	return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

float V_SmithGGXCorrelated(float NoV, float NoL, float roughness) {
    float a2 = pow(roughness, 2.0);
    float GGXV = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2);
    float GGXL = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2);
    return 0.5 / (GGXV + GGXL);
}

vec3 importanceSampleGGX(vec2 Xi, float roughness, vec3 N)
{
    float a = roughness;
    // Sample in spherical coordinates
    float Phi = 2.0 * PI * Xi.x;
    float CosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float SinTheta = sqrt(1.0 - CosTheta * CosTheta);
    // Construct tangent space vector
    vec3 H;
    H.x = SinTheta * cos(Phi);
    H.y = SinTheta * sin(Phi);
    H.z = CosTheta;
    
    // Tangent to world space
    vec3 UpVector = abs(N.z) < 0.999 ? vec3(0.,0.,1.0) : vec3(1.0,0.,0.);
    vec3 TangentX = normalize(cross(UpVector, N));
    vec3 TangentY = cross(N, TangentX);
    return TangentX * H.x + TangentY * H.y + N * H.z;
}

// Output
layout(location = 0) out vec2 outColor;

void main()
{
	// In LUT horizontal is NdotV while vertical is roughness
	const float NoV = gl_FragCoord.x / 512.f;
	const float roughness = 1.f - gl_FragCoord.y / 512.f;

	vec3 V;
    V.x = sqrt(1.0 - NoV * NoV); // sin
    V.y = 0.0;
    V.z = NoV; // cos
    
    // N points straight upwards for this integration
    const vec3 N = vec3(0.0, 0.0, 1.0);
    
    float A = 0.0;
    float B = 0.0;
    const uint numSamples = 1024u;
    
    for (uint i = 0u; i < numSamples; i++) {
        vec2 Xi = hammersley(i, numSamples);
        // Sample microfacet direction
        vec3 H = importanceSampleGGX(Xi, roughness, N);
        
        // Get the light direction
        vec3 L = 2.0 * dot(V, H) * H - V;
        
        float NoL = saturate(dot(N, L));
        float NoH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));
        
        if(NoL > 0.0) {
            float V_pdf = V_SmithGGXCorrelated(NoV, NoL, roughness) * VoH * NoL / NoH;
            float Fc = pow(1.0 - VoH, 5.0);
            A += (1.0 - Fc) * V_pdf;
            B += Fc * V_pdf;
        }
    }

    outColor =  4.0 * vec2(A, B) / float(numSamples);
}