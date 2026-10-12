#version 450
//! #extension GL_KHR_vulkan_glsl : enable

layout(push_constant) uniform PushConstants {
    mat4 projection;
	uint mipLevel;
} pushConstants;

// Vertex Shader Code
// Hardcoded vertices for 3D unit cube
const vec3 g_vertexPositions [36] = vec3[]
(
	// +X
	vec3(1, 1,-1), vec3(1,-1,-1), vec3(1, -1, 1),
	vec3(1, 1,-1), vec3(1,-1, 1), vec3(1, 1,1),

	// -X
	vec3(-1,-1,-1), vec3(-1, 1,-1), vec3(-1, 1, 1),
	vec3(-1,-1,-1), vec3(-1, 1, 1), vec3(-1, -1, 1),

	// +Y
	vec3(-1,1,-1), vec3(1,1,-1), vec3(1, 1, 1),
	vec3(-1, 1,-1), vec3(1,1, 1), vec3(-1, 1, 1),

	// -Y
	vec3(-1,-1, 1), vec3(1,-1, 1), vec3(1, -1, -1),
	vec3(-1,-1,1), vec3(1,-1,-1), vec3(-1, -1, -1),

	// +Z
	vec3(1,-1,1), vec3(-1,-1,1), vec3(-1, 1,1),
	vec3(1,-1,1), vec3(-1,1,1), vec3(1, 1,1),

	// -Z
	vec3(-1,-1,-1), vec3(1,-1,-1), vec3(1, 1,-1),
	vec3(-1,-1,-1), vec3(1, 1,-1), vec3(-1, 1, -1)
);

// Output
layout(location = 0) out vec3 fragLocalPosition;

// Shader
void main()
{
	vec3 position = g_vertexPositions[gl_VertexIndex];
	fragLocalPosition = position;
	gl_Position = pushConstants.projection
		* vec4(position.xyz, 1.f);
}