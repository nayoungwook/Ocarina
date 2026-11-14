#version 330 core

in vec2 uv;
in vec4 color;
in vec2 renderPosition;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform int uMaskEnabled;
uniform vec4 uMaskRect;

void main() {
	if (uMaskEnabled != 0 &&
		(uMaskRect.x >= uMaskRect.z || uMaskRect.y >= uMaskRect.w ||
		 renderPosition.x < uMaskRect.x || renderPosition.x > uMaskRect.z ||
		 renderPosition.y < uMaskRect.y || renderPosition.y > uMaskRect.w)) {
		discard;
	}
	FragColor = texture(uTexture, uv) * color;
}
