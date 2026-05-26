#version 450

// ------------ GLOBAL CONSTS -----------------
const float PI = 3.14159265358979;
const float INV_PI = 1 / PI;

// ------------ STRUCTS -----------------
const uint PointLightType = 0;
const uint DirectionalLightType = 1;
const uint AmbientLightType = 2;

struct Light
{
    vec3 pos;
    float intensity;
    vec3 color;
    uint type;
};

struct PointLight
{
    vec3 pos;
    vec3 color;
};

// ------------ FUNCTION DEFINITIONS -----------------
vec3 Decode(vec2 f);
vec3 GetWorldPositionFromDepth(in float depth, in ivec2 fragCoords, in ivec2 resolution, in mat4 invProj, in mat4 invView);

float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlick(float cosTheta, vec3 F0);

// ------------ BINDINGS -----------------
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    ivec2 resolution;
    vec4 cameraPos;
} ubo;

const int lightCount = 100;
layout(binding = 1) uniform LightBufferObject {
    Light lights[lightCount];
} lightBuffer;

layout(binding = 2) uniform sampler2D depthSampler;
layout(binding = 3) uniform sampler2D colorSampler;
layout(binding = 4) uniform sampler2D normalSampler;
layout(binding = 5) uniform sampler2D metallicSampler;

layout(location = 0) out vec4 outColor;

// ------------ CODE -----------------
void main()
{
	ivec2 pos = ivec2(gl_FragCoord.xy);
	float sampledDepth = texelFetch(depthSampler, pos, 0).r;
    if (sampledDepth >= 1) discard;

    mat4 testView = mat4(
        vec4(0, 0, -1, 0),
        vec4(0, 1, 0, 0),
        vec4(1, 0, 0, 0),
        vec4(0, -1, 0, 1)
    );

    mat4 testProj = mat4(
        vec4(1.81066, 0, 0, 0),
        vec4(0, -2.41421, 0, 0),
        vec4(0, 0, -1.001, -1),
        vec4(0, 0, -1.0001, 0)
    );
    
    const vec3 worldPosition = GetWorldPositionFromDepth(sampledDepth, 
        pos, ubo.resolution, 
        inverse(ubo.proj), inverse(ubo.view));
    vec4 albedo = texelFetch(colorSampler, pos, 0);
    vec3 N = Decode(texelFetch(normalSampler, pos, 0).rg);
    vec4 metallic = texelFetch(metallicSampler, pos, 0);

    vec3 V = normalize(ubo.cameraPos.xyz - worldPosition);
    
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo.rgb, metallic.b);

    vec3 Lo = vec3(0.0);
    for (int i = 0; i < lightCount; i++)
    {
        vec3 L, H, irradiance;
        // calculate per-light radiance
        if (lightBuffer.lights[i].type == PointLightType)
        {
            vec3 distanceVector = lightBuffer.lights[i].pos - worldPosition;
            float sqrDistance = max(dot(distanceVector, distanceVector), 0.0001);
            L = normalize(distanceVector);
            H = normalize(V + L);

            const float luminousIntensity = lightBuffer.lights[i].intensity / (4 * PI);
            const float attenuation = 1.0 / sqrDistance;
            const float illuminance = luminousIntensity * attenuation;
            irradiance = lightBuffer.lights[i].color * illuminance;  
        }
        if (lightBuffer.lights[i].type == DirectionalLightType)
        {
            L = lightBuffer.lights[i].pos;
            H = normalize(V + L);

            irradiance = lightBuffer.lights[i].color * lightBuffer.lights[i].intensity;
        }
        if (lightBuffer.lights[i].type == AmbientLightType)
        {
            Lo += albedo.rgb * lightBuffer.lights[i].color * lightBuffer.lights[i].intensity;
            continue;
        }

        // cook-torrance brdf
        float NDF = DistributionGGX(N, H, metallic.g);        
        float G   = GeometrySmith(N, V, L, metallic.g);      
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);       
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic.b;
        
        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular     = numerator / denominator;  
            
        // add to outgoing radiance Lo
        float NdotL = max(dot(N, L), 0.0);                
        Lo += (kD * albedo.rgb / PI + specular) * irradiance * NdotL;
    }

    vec3 color = Lo;
	
    color = color / (color + vec3(1.0));

    outColor = vec4(color, 1);
}



// ------------ FUNCTIONS -----------------

vec3 Decode(vec2 f)
{
    vec2 ang = f * 2.0 - 1.0;
 
    float angPI = ang.x * PI;
    vec2 scth;
    //sincos(ang.x * PI, scth.x, scth.y);
    scth.x = sin(angPI);
    scth.y = cos(angPI);
    vec2 scphi = vec2(sqrt(1.0 - ang.y * ang.y), ang.y);
 
    vec3 n;
    n.x = scth.y * scphi.x;
    n.y = scth.x * scphi.x;
    n.z = scphi.y;
    return n;
}

// --- World Position From Depth
vec3 GetWorldPositionFromDepth(in float depth,
    in ivec2 fragCoords, in ivec2 resolution,
    in mat4 invProj, in mat4 invView)
{
    // Convert from fragment coordinate system to NDC and
    // reconstruct clip space coordinate position
    vec2 ndc = vec2(
        (float(fragCoords.x) / resolution.x) * 2.0f - 1.0f,
        (float(fragCoords.y) / resolution.y) * 2.0f - 1.0f);
    //ndc.y *=- 1.f;
    const vec4 clipPos = vec4(ndc, depth, 1.0f);

    // Inverse projection to view space
    vec4 viewPos = invProj * clipPos;
    viewPos /= viewPos.w;

    // Inverse view to world space
    vec4 worldPos = invView * viewPos;
    return worldPos.xyz;
}


float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness;
    const bool squareRoughness = false;
    if (squareRoughness)
        a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
} 