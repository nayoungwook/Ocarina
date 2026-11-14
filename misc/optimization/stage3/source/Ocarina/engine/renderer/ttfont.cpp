#include "ttfont.h"

#include "error.h"
#include "texture.h"
#include "util.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include <SDL3_ttf/SDL_ttf.h>

static int ttf_init_count = 0;

static unsigned char to_byte(float value) {
	return static_cast<unsigned char>(
		std::max(0.0f, std::min(value, 1.0f)) * 255.0f + 0.5f);
}

static SDL_Color to_sdl_color(const Color &color) {
	return SDL_Color{
		to_byte(color.r),
		to_byte(color.g),
		to_byte(color.b),
		to_byte(color.a),
	};
}

static bool acquire_ttf() {
	// Fonts share SDL_ttf global state. Count live TTFont instances so the
	// subsystem shuts down only after the last font is destroyed.
	if (!TTF_Init()) {
		error(std::string("SDL3_ttf init failed: ") + SDL_GetError());
		return false;
	}

	++ttf_init_count;
	return true;
}

static void release_ttf() {
	if (ttf_init_count <= 0)
		return;

	--ttf_init_count;
	TTF_Quit();
}

static std::vector<std::string> split_lines(const std::string &text) {
	// Font rendering caches complete text blocks, but measuring and drawing
	// still happens one line at a time.
	std::vector<std::string> lines;
	size_t start = 0;

	while (start <= text.size()) {
		size_t end = text.find('\n', start);
		if (end == std::string::npos) {
			lines.push_back(text.substr(start));
			break;
		}

		lines.push_back(text.substr(start, end - start));
		start = end + 1;
	}

	if (lines.empty())
		lines.emplace_back();
	return lines;
}

static int measure_line_height(TTF_Font *font, const std::vector<std::string> &lines) {
	int line_height =
		std::max(1, std::max(TTF_GetFontLineSkip(font),
			TTF_GetFontHeight(font)));

	for (const std::string &line : lines) {
		if (line.empty())
			continue;

		int width = 0;
		int height = 0;
		if (!TTF_GetStringSize(font, line.c_str(), line.size(), &width,
				&height)) {
			error(std::string("Failed to measure text: ") +
				SDL_GetError());
		}
		line_height = std::max(line_height, height);
	}

	return line_height;
}

static TTFont::TextMetrics measure_lines(TTF_Font *font,
	const std::vector<std::string> &lines) {
	TTFont::TextMetrics metrics;
	int line_height = measure_line_height(font, lines);
	int max_width = 0;

	for (const std::string &line : lines) {
		int width = 0;
		int height = 0;
		if (!line.empty() &&
			!TTF_GetStringSize(font, line.c_str(), line.size(), &width,
				&height)) {
			error(std::string("Failed to measure text: ") +
				SDL_GetError());
		}
		max_width = std::max(max_width, width);
	}

	metrics.width = std::max(1, max_width);
	metrics.height = std::max(1,
		line_height * static_cast<int>(std::max<size_t>(1, lines.size())));
	return metrics;
}

static void blend_pixel(std::vector<unsigned char> &pixels, int width, int height,
	int x, int y, const unsigned char *src) {
	if (x < 0 || y < 0 || x >= width || y >= height || src[3] == 0)
		return;

	float src_a = src[3] / 255.0f;
	float dst_a = pixels[(y * width + x) * 4 + 3] / 255.0f;
	float out_a = src_a + dst_a * (1.0f - src_a);
	if (out_a <= 0.0f)
		return;

	int index = (y * width + x) * 4;
	float src_r = src[0] / 255.0f;
	float src_g = src[1] / 255.0f;
	float src_b = src[2] / 255.0f;
	float dst_r = pixels[index] / 255.0f;
	float dst_g = pixels[index + 1] / 255.0f;
	float dst_b = pixels[index + 2] / 255.0f;

	pixels[index] = to_byte((src_r * src_a + dst_r * dst_a *
		(1.0f - src_a)) / out_a);
	pixels[index + 1] = to_byte((src_g * src_a + dst_g * dst_a *
		(1.0f - src_a)) / out_a);
	pixels[index + 2] = to_byte((src_b * src_a + dst_b * dst_a *
		(1.0f - src_a)) / out_a);
	pixels[index + 3] = to_byte(out_a);
}

static void blend_surface(std::vector<unsigned char> &pixels, int width, int height,
	SDL_Surface *surface, int dst_x, int dst_y) {
	if (surface == nullptr)
		return;

	SDL_Surface *rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
	if (rgba == nullptr) {
		error(std::string("Failed to convert text surface: ") +
			SDL_GetError());
	}

	unsigned char *src_pixels =
		static_cast<unsigned char *>(rgba->pixels);
	for (int y = 0; y < rgba->h; ++y) {
		unsigned char *row = src_pixels + y * rgba->pitch;
		for (int x = 0; x < rgba->w; ++x) {
			blend_pixel(pixels, width, height, dst_x + x, dst_y + y,
				row + x * 4);
		}
	}

	SDL_DestroySurface(rgba);
}

static SDL_Surface *render_line(TTF_Font *font, const std::string &line,
	const SDL_Color &color, int outline,
	FontRasterization rasterization) {
	// SDL_ttf treats zero alpha as opaque in solid mode.
	if (line.empty() || color.a == 0)
		return nullptr;

	if (!TTF_SetFontOutline(font, outline)) {
		error(std::string("Failed to set font outline: ") +
			SDL_GetError());
	}

	SDL_Surface *surface = rasterization == FontRasterization::Solid
		? TTF_RenderText_Solid(font, line.c_str(), line.size(), color)
		: TTF_RenderText_Blended(font, line.c_str(), line.size(), color);
	if (surface == nullptr) {
		error(std::string("Failed to render text: ") + SDL_GetError());
	}

	return surface;
}

TTFont::TTFont(const std::string &font_path, float font_size,
	const FontRenderOptions &options)
	: render_options(options), size(std::max(1.0f, font_size)) {
	try {
		// TTFont owns the SDL font handle and lazily creates GL textures for text
		// strings as Renderer asks for them.
		ttf_initialized = acquire_ttf();

		std::string resolved = resolve_font_path(font_path);
		font = TTF_OpenFont(resolved.c_str(), size);
		if (font == nullptr) {
			error("Failed to load font " + resolved + ": " +
				SDL_GetError());
		}
		else {
			set_render_options(options);
		}
	} catch (...) {
		if (font != nullptr) TTF_CloseFont(font);
		if (ttf_initialized) release_ttf();
		throw;
	}
}

TTFont::~TTFont() {
	clear_cache();
	if (font != nullptr) {
		TTF_CloseFont(font);
		font = nullptr;
	}

	if (ttf_initialized) {
		release_ttf();
		ttf_initialized = false;
	}
}

static std::string make_text_cache_key(const std::string &text, const Color &color,
	const Color &outline_color, float outline_width) {
	std::string key = text;
	key.push_back('\0');
	key.append(reinterpret_cast<const char *>(&color), sizeof(color));
	key.append(reinterpret_cast<const char *>(&outline_color),
		sizeof(outline_color));
	key.append(reinterpret_cast<const char *>(&outline_width),
		sizeof(outline_width));
	return key;
}

std::string TTFont::resolve_font_path(
	const std::string &font_path) const {
	if (is_absolute_path(font_path) && file_exists(font_path))
		return font_path;

	std::string cwd = current_working_directory();
	std::array<std::string, 5> candidates = {
		join_path(cwd, font_path),
		join_path(join_path(cwd, "resources"), font_path),
		join_path(join_path(join_path(cwd, "resources"), "font"), font_path),
		join_path(join_path(join_path(join_path(cwd, "Toffee"),
			"engineResources"), "font"), font_path),
#ifdef ENGINE_RESOURCE_ROOT
		join_path(ENGINE_RESOURCE_ROOT, font_path),
#else
		std::string(),
#endif
	};

	for (const auto &candidate : candidates) {
		if (!candidate.empty() && file_exists(candidate))
			return candidate;
	}

	return join_path(cwd, font_path);
}

TTFont::TextMetrics TTFont::measure_text(const std::string &text) const {
	if (font == nullptr)
		return TextMetrics{};

	TTF_SetFontOutline(font, 0);
	return measure_lines(font, split_lines(text));
}

void TTFont::set_render_options(const FontRenderOptions &options) {
	render_options = options;
	clear_cache();
	if (font == nullptr)
		return;

	SDL_PropertiesID properties = TTF_GetFontProperties(font);
	if (properties == 0 ||
		!SDL_SetNumberProperty(properties,
			TTF_PROP_FONT_OUTLINE_LINE_CAP_NUMBER,
			static_cast<Sint64>(render_options.outline_cap)) ||
		!SDL_SetNumberProperty(properties,
			TTF_PROP_FONT_OUTLINE_LINE_JOIN_NUMBER,
			static_cast<Sint64>(render_options.outline_join))) {
		error(std::string("Failed to configure font outline: ") +
			SDL_GetError());
	}
	TTF_SetFontOutline(font, 0);
}

const FontRenderOptions &TTFont::get_render_options() const {
	return render_options;
}

std::unique_ptr<Texture> TTFont::create_texture(const std::string &text,
	const Color &color, const Color &outline_color,
	float outline_width) {
	if (font == nullptr)
		return nullptr;

	TTF_SetFontOutline(font, 0);
	std::vector<std::string> lines = split_lines(text);
	TextMetrics metrics = measure_lines(font, lines);
	int outline = std::max(0, static_cast<int>(std::ceil(outline_width)));
	int width = std::max(1, metrics.width + outline * 2 + 4);
	int height = std::max(1, metrics.height + outline * 2 + 4);
	int line_height = measure_line_height(font, lines);
	std::vector<unsigned char> pixels(width * height * 4, 0);
	SDL_Color fill_color = to_sdl_color(color);
	SDL_Color stroke_color = to_sdl_color(outline_color);

	for (size_t line_index = 0; line_index < lines.size(); ++line_index) {
		const std::string &line = lines[line_index];
		int line_y = static_cast<int>(line_index) * line_height;

		if (outline > 0) {
			SDL_Surface *outline_surface =
				render_line(font, line, stroke_color, outline,
					render_options.rasterization);
			blend_surface(pixels, width, height, outline_surface, 2,
				2 + line_y);
			if (outline_surface != nullptr)
				SDL_DestroySurface(outline_surface);
		}

		SDL_Surface *fill_surface = render_line(font, line, fill_color, 0,
			render_options.rasterization);
		blend_surface(pixels, width, height, fill_surface, outline + 2,
			outline + 2 + line_y);
		if (fill_surface != nullptr)
			SDL_DestroySurface(fill_surface);
	}

	TTF_SetFontOutline(font, 0);

	last_width = width;
	last_height = height;
	auto texture = std::make_unique<Texture>(width, height, pixels.data());
	const GLenum filter = render_options.texture_filtering ==
		FontTextureFiltering::Nearest ? GL_NEAREST : GL_LINEAR;
	texture->set_filter(filter, filter);
	return texture;
}

Texture *TTFont::get_cached_texture(const std::string &text,
	const Color &color, const Color &outline_color, float outline_width) {
	// Text is expensive to rasterize, so each visual variant is cached as a
	// Texture and reused by later render_font calls.
	const std::string key = make_text_cache_key(text, color, outline_color,
		outline_width);
	auto found = text_cache.find(key);
	if (found != text_cache.end()) {
		last_width = found->second->get_width();
		last_height = found->second->get_height();
		return found->second.get();
	}
	std::unique_ptr<Texture> texture = create_texture(text, color,
		outline_color, outline_width);
	if (!texture)
		return nullptr;
	Texture *result = texture.get();
	text_cache.emplace(key, std::move(texture));
	return result;
}

void TTFont::clear_cache() { text_cache.clear(); }

float TTFont::get_size() const { return size; }

int TTFont::get_last_width() const { return last_width; }

int TTFont::get_last_height() const { return last_height; }
