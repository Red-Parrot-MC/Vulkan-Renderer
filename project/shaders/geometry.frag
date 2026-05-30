#version 450

const float PI = 3.14159265358979;
const float INV_PI = 1 / PI;

layout(binding = 1) uniform sampler2D colorSampler;
layout(binding = 2) uniform sampler2D normalSampler;
layout(binding = 3) uniform sampler2D metalicRoughnessSampler;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec4 fragTangent;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec2 outNormal;
layout(location = 2) out vec2 outMetalic;
layout(location = 3) out vec2 outGeometricNormal;

vec2 Encode(vec3 n);

void main() {
    // ALBEDO
    outAlbedo = texture(colorSampler, fragTexCoord);
    if (outAlbedo.a < 0.5) discard;

    // NORMAL
    vec3 normal = normalize(fragNormal);
    outGeometricNormal = Encode(normal);
    vec3 tangent = normalize(fragTangent.xyz);
    if (tangent.length() > 0.5)
    {
        vec3 binormal = cross(normal, tangent) * fragTangent.w;
        vec3 sampledNormal = texture(normalSampler, fragTexCoord).xyz * 2.0 - 1;
        mat3 TBN = mat3(
            tangent,
            binormal,
            normal
        );

        normal = normalize(TBN * sampledNormal);
    }

    outNormal = Encode(normal);

    // METALIC + ROUGHNESS
    outMetalic = texture(metalicRoughnessSampler, fragTexCoord).gb;
}

vec2 Encode(vec3 n) // Octahedron normal vector encoding
{
    vec2 f;
    f.x = atan(n.y, n.x) * INV_PI;
    f.y = n.z;
 
    f = f * 0.5 + 0.5;
    return f;
}