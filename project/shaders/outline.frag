#version 450

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    layout(offset = 64) vec4 color; 
} pc;

void main()
{
	outColor = pc.color;
	gl_FragDepth = 0;
}