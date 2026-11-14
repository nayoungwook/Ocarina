#version 330 core

struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
};

const int MAX_LIGHTS = 32;

uniform PointLight uLights[MAX_LIGHTS];
uniform int uLightCount;

in vec3 normal;
in vec3 worldPosition;

out vec4 FragColor;

uniform vec4 uColor;

void main() {
    vec3 N = normalize(normal);
    vec3 lighting = vec3(0.05);

    for(int i=0; i<min(MAX_LIGHTS, uLightCount); i++){
        vec3 toLight = uLights[i].position - worldPosition;
        float distanceToLight = length(toLight);
        vec3 L = toLight / max(distanceToLight, 0.0001);

        float brightness = max(dot(N, L), 0.0);
        float attenuation = 1.0 / (1.0 + 0.09 * distanceToLight
            + 0.032 * distanceToLight * distanceToLight);

        lighting += brightness * uLights[i].color
            * uLights[i].intensity * attenuation;
    }
    FragColor = vec4(uColor.rgb * lighting, uColor.a);
}
