#version 450

const float PI = 3.14159265358979;
const float INV_PI = 1 / PI;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outAlbedo;
layout(location = 2) out vec2 outNormal;

vec2 Encode(vec3 n);

void main() {
    outAlbedo = texture(texSampler, fragTexCoord);
    outNormal = Encode(fragNormal);
    if (outAlbedo.a < 0.5) discard;
    outColor = outAlbedo;
    //outColor = vec4(outNormal, 0, 1);
}

vec2 Encode(vec3 n) // Octahedron normal vector encoding
{
    vec2 f;
    f.x = atan(n.y, n.x) * INV_PI;
    f.y = n.z;
 
    f = f * 0.5 + 0.5;
    return f;
}