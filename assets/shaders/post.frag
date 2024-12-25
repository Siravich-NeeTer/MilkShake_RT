#version 460 core

layout(location = 0) out vec4 fragColor;
layout(set = 0, binding = 0) uniform sampler2D renderedImage;

void main()
{
    vec2 uv = gl_FragCoord.xy/vec2(1280, 768);
    fragColor = pow(texture(renderedImage, uv), vec4(1.0 / 2.2));
}
