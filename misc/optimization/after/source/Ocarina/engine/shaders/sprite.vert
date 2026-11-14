#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColor;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

out vec2 uv;
out vec4 color;
out vec2 renderPosition;

void main() {
    uv = aUv;
    color = aColor;
    vec4 modelPosition = uModel * vec4(aPos, 1.0);
    renderPosition = modelPosition.xy;
    gl_Position = uProjection * uView * modelPosition;
}
