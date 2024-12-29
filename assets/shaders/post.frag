#version 460 core

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform sampler2D renderedImage;
layout(set = 0, binding = 1) uniform PostProcessData
{
    vec2 screenSize;
} postProcessData;

void main()
{
    vec2 uv = gl_FragCoord.xy / vec2(postProcessData.screenSize.x, postProcessData.screenSize.y);
    fragColor = texture(renderedImage, uv);
}
