#pragma once

#include <GL/glew.h>
#include <string>
#include <vector>
#include <glm/glm.hpp>

class Texture {
public:
	Texture(const std::string &path);
	Texture(int width, int height, const unsigned char *pixels);
	Texture(unsigned int texture_id, int width, int height, bool owns_texture);
	~Texture();

	const unsigned int get_tex() const;

	int get_atlas_slot();

	void bind();
	void bind(unsigned int slot);

	void fetch_atlas(glm::vec4 atlas);
	void set_atlas(int slot);
	glm::vec4 get_atlas_uv(int slot) const;
	void set_filter(GLenum min_filter, GLenum mag_filter);
	int get_width() const;
	int get_height() const;

private:
	std::vector<glm::vec4> atlas_data;
	int atlas_slot = -1;
	bool owns_texture = true;

	unsigned int tex = 0;
	void load_texture();
	void upload_pixels(const unsigned char *pixels, GLenum format);
	int width, height, channels;
	std::string path;
};
