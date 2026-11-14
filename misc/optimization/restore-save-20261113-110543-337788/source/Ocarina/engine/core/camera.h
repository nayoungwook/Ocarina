#pragma once

#include "mathf.h"
#include <glm/glm.hpp>

enum class CameraMode {
	VIEW_2D,
	VIEW_3D,
};

class Camera {
public:
	explicit Camera(int width, int height);

	CameraMode camera_mode = CameraMode::VIEW_2D;

	Vector position;
	float zoom = 1.0f;
	float rotation = 0.0f;
	Vector rotational_vector;

	void adjust_projection(float resolution_x, float resolution_y);
	void reset_projection_to_base();
	void fit_projection_to_display(int display_width, int display_height);

	glm::mat4 get_pixel_perfect_projection(int pixel_width, int pixel_height,
		float world_units_per_pixel) const;
	glm::mat4 get_pixel_perfect_view(float world_units_per_pixel) const;
	glm::vec2 get_pixel_fraction(float world_units_per_pixel) const;

	void calculate_matrix();
	bool is_dirty() const;

	const glm::mat4 &get_view_projection();
	const glm::mat4 &get_view() const;
	const glm::mat4 &get_projection() const;
	glm::mat4 get_screen_projection() const;
	int get_width() const;
	int get_height() const;
	int get_resolution_x() const;
	int get_resolution_y() const;
	int get_base_width() const;
	int get_base_height() const;

private:

	void set_projection(float resolution_x, float resolution_y);
	glm::mat4 build_projection(float width, float height) const;

	glm::mat4 projection{ 1.0f };
	glm::mat4 view{ 1.0f };
	glm::mat4 view_projection{ 1.0f };

	Vector cached_position;
	Vector cached_rotational_vector;

	float fov;

	float cached_zoom = 0.0f;
	float cached_rotation = 0.0f;
	bool matrices_dirty = true;

	float base_width = 1.0f;
	float base_height = 1.0f;
	float resolution_width = 1.0f;
	float resolution_height = 1.0f;

};
