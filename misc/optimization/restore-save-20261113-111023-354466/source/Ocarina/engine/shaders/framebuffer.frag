#version 330 core

uniform sampler2D uTexture;
uniform vec4 uColor;
uniform vec2 uSourceSize;
uniform vec2 uOutputSize;
uniform vec2 uCameraOffset;
uniform float uCameraZoom;
uniform float uCameraRotation;
uniform float uWorldUnitsPerPixel;
uniform int uPixelPerfect;
uniform int uMaskEnabled;
uniform vec4 uMaskRect;

in vec2 uv;
in vec2 renderPosition;
out vec4 FragColor;

void main() {
	if (uMaskEnabled != 0 &&
		(uMaskRect.x >= uMaskRect.z || uMaskRect.y >= uMaskRect.w ||
		 renderPosition.x < uMaskRect.x || renderPosition.x > uMaskRect.z ||
		 renderPosition.y < uMaskRect.y || renderPosition.y > uMaskRect.w)) {
		discard;
	}
	vec2 sampleCoords = uv;
	if (uPixelPerfect != 0) {
		vec2 outputPixel = uv * uOutputSize - uOutputSize * 0.5;
		float zoom = max(abs(uCameraZoom), 0.0001);
		float cosine = cos(-uCameraRotation);
		float sine = sin(-uCameraRotation);
		// GLSL matrices are column-major. This is the inverse camera rotation,
		// matching Camera::calculate_matrix()'s world-to-screen orientation.
		vec2 sourceOffset = mat2(cosine, sine, -sine, cosine) * outputPixel /
			(zoom * max(uWorldUnitsPerPixel, 0.0001));
		if (uCameraZoom < 0.0) sourceOffset = -sourceOffset;
		sampleCoords = (uSourceSize * 0.5 + sourceOffset + uCameraOffset) / uSourceSize;
	}
	FragColor = texture(uTexture, sampleCoords) * uColor;
}
