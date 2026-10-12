#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    ivec2 resolution;
    vec4 cameraPos;
    mat4 invView;
    mat4 invProj;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec4 fragTangent;

void main() {
    gl_Position = ubo.proj * pc.model * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord;
    fragNormal = (pc.model * vec4(inNormal, 0)).xyz;

    fragTangent = inTangent;
}