#version 450

const float PI = 3.14159265358979;
const float INV_PI = 1 / PI;

layout(binding = 0) uniform sampler2D depthSampler;
layout(binding = 1) uniform sampler2D colorSampler;
layout(binding = 2) uniform sampler2D normalSampler;
layout(binding = 3) uniform sampler2D metallicSampler;

layout(location = 0) out vec4 outColor;

vec3 Decode(vec2 f);

void main()
{
	ivec2 pos;
	pos.x = int(gl_FragCoord.x);
	pos.y = int(gl_FragCoord.y);
	float sampledDepth = texelFetch(depthSampler, pos, 0).r;
	float depth = (2*0.1*100)/(100 + 0.1 - sampledDepth * (100 - 0.1)) / 100;
    vec3 normal = Decode(texelFetch(normalSampler, pos, 0).xy);

	outColor = mix(texelFetch(colorSampler, pos, 0), vec4(0), pow(depth, 0.1));

	//outColor = vec4(depth / 100);
	//outColor = vec4(texelFetch(depthSampler, pos, 0).r);
    //outColor = vec4(normal, 1);
}

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