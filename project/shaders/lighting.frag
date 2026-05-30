#version 450

// ------------ GLOBAL CONSTS -----------------
const float PI = 3.14159265358979;
const float INV_PI = 1 / PI;

// ------------ STRUCTS -----------------
const uint PointLightType = 1;
const uint DirectionalLightType = 2;
const uint AmbientLightType = 3;
const uint SpotLightType = 4;

struct Light
{
    vec3 pos;
    float intensity;
    vec3 color;
    uint type;
};

struct ShadowLight
{
    vec3 direction;
    float intensity;
    vec3 color;
    uint layerIndex;
    mat4 view;
    mat4 proj;
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
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);
vec3 SampleIBL_Diffuse(in vec3 normalWS, in float environmentIntensity);

// ------------ BINDINGS -----------------
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    ivec2 resolution;
    vec4 cameraPos;
    mat4 invView;
    mat4 invProj;
} ubo;

const int lightCount = 100;
layout(binding = 1) uniform LightBufferObject {
    Light lights[lightCount];
} lightBuffer;

const int shadowLightCount = 100;
layout(binding = 2) uniform ShadowLightBufferObject {
    ShadowLight shadowLights[shadowLightCount];
} shadowLightBuffer;

layout(binding = 3) uniform sampler2DArrayShadow shadowMapsSampler;
layout(binding = 4) uniform samplerCube cubeSampler;
layout(binding = 5) uniform samplerCube diffuseIrradianceSampler;
layout(binding = 6) uniform sampler2D depthSampler;
layout(binding = 7) uniform sampler2D colorSampler;
layout(binding = 8) uniform sampler2D normalSampler;
layout(binding = 9) uniform sampler2D metallicSampler;
layout(binding = 10) uniform sampler2D geometricNormalSampler;

layout(location = 0) out vec4 outColor;

// ------------ CONSTANTS -----------------
const float environmentIntensity = 1;
const float exposureCompensation = 1;

// ------------ CODE -----------------
void main()
{
	const ivec2 pos = ivec2(gl_FragCoord.xy);
	float sampledDepth = texelFetch(depthSampler, pos, 0).r;
    const vec3 worldPosition = GetWorldPositionFromDepth(sampledDepth, 
        pos, ubo.resolution, 
        ubo.invProj, ubo.invView);
    
    vec3 V = normalize(ubo.cameraPos.xyz - worldPosition);

    if (sampledDepth >= 1) 
    {
        outColor = vec4(texture(cubeSampler, -V).rgb, 1);
        return;
    }
    
    vec4 albedo = texelFetch(colorSampler, pos, 0);
    vec2 metallic = texelFetch(metallicSampler, pos, 0).rg;
    vec3 N = Decode(texelFetch(normalSampler, pos, 0).rg);

    
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo.rgb, metallic.g);

    vec3 Lo = vec3(0.0);
    for (int i = 0; i < lightCount; i++)
    {
        if (lightBuffer.lights[i].type == 0) break;

        vec3 L, irradiance;
        // calculate per-light radiance
        if (lightBuffer.lights[i].type == PointLightType)
        {
            vec3 distanceVector = lightBuffer.lights[i].pos - worldPosition;
            if (dot(N, distanceVector) < 0) continue;

            float sqrDistance = max(dot(distanceVector, distanceVector), 0.0001);
            L = normalize(distanceVector);

            const float luminousIntensity = lightBuffer.lights[i].intensity / (4 * PI);
            const float attenuation = 1.0 / sqrDistance;
            const float illuminance = luminousIntensity * attenuation;
            irradiance = lightBuffer.lights[i].color * illuminance;  
        }
        if (lightBuffer.lights[i].type == DirectionalLightType)
        {
            L = -lightBuffer.lights[i].pos;
            if (dot(N, L) < 0) continue;

            irradiance = lightBuffer.lights[i].color * lightBuffer.lights[i].intensity;
        }
        if (lightBuffer.lights[i].type == AmbientLightType)
        {
            Lo += albedo.rgb * lightBuffer.lights[i].color * lightBuffer.lights[i].intensity;
            continue;
        }

        vec3 H = normalize(V + L);

        // cook-torrance brdf
        float NDF = DistributionGGX(N, H, metallic.r);        
        float G   = GeometrySmith(N, V, L, metallic.r);      
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);       
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic.g;
        
        vec3 numerator    = NDF * G * F;
        float NdotL = max(dot(N, L), 0.0);     
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
        vec3 specular     = numerator / denominator;  
            
        // add to outgoing radiance Lo              
        Lo += (kD * albedo.rgb / PI + specular) * irradiance * NdotL;
    }

    for (int i = 0; i < shadowLightCount; i++)
    {
        if (shadowLightBuffer.shadowLights[i].direction == vec3(0)) break;
        const ShadowLight light = shadowLightBuffer.shadowLights[i];

        vec3 L = -light.direction;
        //if (dot(N, L) < 0) continue;
        vec3 irradiance = light.color * light.intensity;

        // Get UV coordinate in shadow map
        vec4 LightSpacePosition = light.proj * light.view * vec4(worldPosition, 1.f);
        LightSpacePosition /= LightSpacePosition.w;
        vec3 shadowMapUV = vec3(LightSpacePosition.xy * 0.5f + 0.5f,
        LightSpacePosition.z);

        // Manual compare - point sampling or nearest filtering required!
        float shadowTerm = texture(shadowMapsSampler, vec4(shadowMapUV.xy, light.layerIndex, shadowMapUV.z)).r;
        vec3 geometricNormal = Decode(texelFetch(geometricNormalSampler, pos, 0).rg);
        float NdotL = max(dot(geometricNormal, L), 0.0);  
        shadowTerm *= smoothstep(0, 0.2, NdotL);

        vec3 H = normalize(V + L);

        // cook-torrance brdf
        float NDF = DistributionGGX(N, H, metallic.r);        
        float G   = GeometrySmith(N, V, L, metallic.r);      
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);       
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic.g;
        
        vec3 numerator    = NDF * G * F;   
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
        vec3 specular     = numerator / denominator;  
            
        // add to outgoing radiance Lo              
        Lo = (kD * albedo.rgb / PI + specular) * irradiance * NdotL * shadowTerm;
    }

    // Calculate diffuse and specular terms based on roughness
    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.f), F0, metallic.r);
    vec3 kS = F;
    vec3 kD = 1.f - kS;
    kD *= 1.f - metallic.g;

    // Calculate ambient diffuse lighting
    const vec3 prefilteredDiffuseIrradiance = SampleIBL_Diffuse(N, environmentIntensity);
    const vec3 diffuse = prefilteredDiffuseIrradiance * albedo.rgb;

    // Finalize color
    vec3 ambient = kD * diffuse * exposureCompensation;
    vec3 finalColor = ambient + Lo;

    outColor = vec4(finalColor, 1);
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

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}  

vec3 SampleIBL_Diffuse(in vec3 normalWS, in float environmentIntensity)
{
    const vec3 prefilteredDiffuseIrradiance = texture(
    diffuseIrradianceSampler,
    normalWS).rgb;
    const vec3 exposedPrefilteredDiffuseIrradiance = prefilteredDiffuseIrradiance * environmentIntensity;
    return exposedPrefilteredDiffuseIrradiance;
}