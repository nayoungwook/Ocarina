#version 330 core

in vec2 uv;
out vec4 FragColor;

uniform sampler2D uWorldTex;
uniform sampler2D uUITex;

void main() {
	vec4 world = texture(uWorldTex, uv);
    vec4 ui = texture(uUITex, uv);

    FragColor = vec4(mix(world.rgb, ui.rgb, ui.a),
        ui.a + world.a * (1.0 - ui.a));
}
