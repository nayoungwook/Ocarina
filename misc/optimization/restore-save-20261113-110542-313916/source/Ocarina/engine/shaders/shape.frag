#version 330 core

in vec2 localPos;
in vec2 renderPosition;
out vec4 FragColor;

uniform vec4 uColor;
uniform int uShapeType;
uniform int uMaskEnabled;
uniform vec4 uMaskRect;

void main() {
    if (uMaskEnabled != 0 &&
        (uMaskRect.x >= uMaskRect.z || uMaskRect.y >= uMaskRect.w ||
         renderPosition.x < uMaskRect.x || renderPosition.x > uMaskRect.z ||
         renderPosition.y < uMaskRect.y || renderPosition.y > uMaskRect.w)) {
        discard;
    }
    if (uShapeType == 1 && dot(localPos * 2.0, localPos * 2.0) > 1.0) {
        discard;
    }

    FragColor = vec4(uColor.rgb * uColor.a, uColor.a);
}
