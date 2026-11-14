#pragma once

struct Color {
	float r = 1.0f;
	float g = 1.0f;
	float b = 1.0f;
	float a = 1.0f;

	constexpr Color() = default;
	constexpr Color(float r, float g, float b, float a = 1.0f)
		: r(r), g(g), b(b), a(a) {}

	static constexpr Color white() { return Color(1.0f, 1.0f, 1.0f, 1.0f); }
	static constexpr Color black() { return Color(0.0f, 0.0f, 0.0f, 1.0f); }
	static constexpr Color red() { return Color(1.0f, 0.0f, 0.0f, 1.0f); }
	static constexpr Color blue() { return Color(0.0f, 0.0f, 1.0f, 1.0f); }
	static constexpr Color transparent() {
		return Color(0.0f, 0.0f, 0.0f, 0.0f);
	}
	static constexpr Color outline() {
		return Color(39.0f / 255.0f, 39.0f / 255.0f,
			54.0f / 255.0f, 1.0f);
	}
};
