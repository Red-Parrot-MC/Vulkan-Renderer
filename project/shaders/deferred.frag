#version 450
layout(binding = 0) uniform sampler2D depthSampler;
layout(binding = 1) uniform sampler2D colorSampler;
layout(binding = 2) uniform sampler2D normalSampler;
layout(binding = 3) uniform sampler2D metallicSampler;

layout(location = 0) out vec4 outColor;

void main()
{
	ivec2 pos;
	pos.x = int(gl_FragCoord.x);
	pos.y = int(gl_FragCoord.y);
	float sampledDepth = texelFetch(depthSampler, pos, 0).r;
	float depth = (2*0.1*100)/(100 + 0.1 - sampledDepth * (100 - 0.1)) / 100;

	outColor = mix(texelFetch(colorSampler, pos, 0), vec4(0), pow(depth, 0.1));

	//outColor = vec4(depth / 100);
	//outColor = vec4(texelFetch(depthSampler, pos, 0).r);
}