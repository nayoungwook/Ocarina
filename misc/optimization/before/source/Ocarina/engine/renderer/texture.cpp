#include "texture.h"
#include "error.h"
#include "util.h"

#include <string>

#define STB_IMAGE_IMPLEMENTATION
// Resource paths from util.h use UTF-8 on Windows as well.
#define STBI_WINDOWS_UTF8
#include <stb/stb_image.h>

Texture::Texture(const std::string &path) : path(path) {
	this->load_texture();
}

Texture::Texture(int width, int height, const unsigned char *pixels)
	: width(width), height(height), channels(4) {
	this->upload_pixels(pixels, GL_RGBA);
}

Texture::Texture(unsigned int texture_id, int width, int height,
	bool owns_texture)
	: width(width), height(height), channels(4), owns_texture(owns_texture),
	  tex(texture_id) {}

Texture::~Texture() {
	if (tex != 0 && owns_texture)
		glDeleteTextures(1, &tex);
}

void Texture::fetch_atlas(glm::vec4 atlas) {
	this->atlas_data.push_back(atlas);
}

void Texture::set_atlas(int slot) {
	if (this->atlas_data.size() == 0) {
		this->atlas_slot = slot;
		return;
	}

	if (this->atlas_slot == slot)
		return;

	if (this->atlas_data.size() <= slot || slot < 0) {
		error("Atlas out of index MAX : " +
			std::to_string(this->atlas_data.size()) + " , " +
			std::to_string(slot));
	}

	this->atlas_slot = slot;
}

glm::vec4 Texture::get_atlas_uv(int slot) const {
	if (atlas_data.empty() || slot < 0)
		return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);

	if (slot >= static_cast<int>(atlas_data.size()))
		return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);

	glm::vec4 atlas = atlas_data[slot];
	float epsilon = 0.001f;
	float x = (atlas.x + epsilon) / width;
	float y = (atlas.y + epsilon) / height;
	float max_x = (atlas.x + atlas.z - epsilon) / width;
	float max_y = (atlas.y + atlas.w - epsilon) / height;
	return glm::vec4(x, y, max_x, max_y);
}

void Texture::load_texture() {
	// stb_image decodes into CPU memory; upload_pixels transfers it to GL and
	// the CPU copy is released immediately after.
	std::string full_path = resolve_resource_path(path);

	unsigned char *data =
		stbi_load(full_path.c_str(), &width, &height, &channels, 0);

	if (!data) {
		error("Failed to load texture " + full_path);
		return;
	}

	GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
	upload_pixels(data, format);

	stbi_image_free(data);
}

void Texture::upload_pixels(const unsigned char *pixels, GLenum format) {
	// Atlas and framebuffer sampling expect pixel-art friendly nearest filters
	// and clamped edges unless a higher-level shader decides otherwise.
	if (tex == 0)
		glGenTextures(1, &this->tex);
	glBindTexture(GL_TEXTURE_2D, this->tex);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	GLenum internal_format = (format == GL_RGBA) ? GL_RGBA8 : GL_RGB8;
	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format,
		GL_UNSIGNED_BYTE, pixels);
}

void Texture::bind(unsigned int slot) {
	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(GL_TEXTURE_2D, this->tex);
}

void Texture::bind() {
	bind(0);
}

int Texture::get_atlas_slot() {
	return this->atlas_slot;
}

void Texture::set_filter(GLenum min_filter, GLenum mag_filter) {
	bind(0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
}

int Texture::get_width() const { return width; }

int Texture::get_height() const { return height; }

const unsigned int Texture::get_tex() const { return tex; }

