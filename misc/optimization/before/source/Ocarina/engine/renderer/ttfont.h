#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "color.h"
#include "font_render_options.h"

class Texture;
struct TTF_Font;

class TTFont {
public:
	struct TextMetrics {
		int width = 0;
		int height = 0;
	};

	// font_path is UTF-8 on all platforms.
	TTFont(const std::string &font_path, float font_size,
		const FontRenderOptions &options = FontRenderOptions());
	~TTFont();
	TTFont(const TTFont &) = delete;
	TTFont &operator=(const TTFont &) = delete;

	std::unique_ptr<Texture> create_texture(
		const std::string &text, const Color &color,
		const Color &outline_color = Color::outline(),
		float outline_width = 0.0f);
	Texture *get_cached_texture(const std::string &text, const Color &color,
		const Color &outline_color = Color::outline(),
		float outline_width = 0.0f);
	void clear_cache();
	TextMetrics measure_text(const std::string &text) const;
	void set_render_options(const FontRenderOptions &options);
	const FontRenderOptions &get_render_options() const;

	float get_size() const;
	int get_last_width() const;
	int get_last_height() const;

private:
	std::string resolve_font_path(const std::string &font_path) const;

	TTF_Font *font = nullptr;
	FontRenderOptions render_options;
	bool ttf_initialized = false;
	float size = 16.0f;
	mutable int last_width = 0;
	mutable int last_height = 0;
	std::unordered_map<std::string, std::unique_ptr<Texture>> text_cache;
};
