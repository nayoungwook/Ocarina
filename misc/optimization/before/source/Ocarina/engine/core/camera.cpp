#include <camera.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(int width, int height) {
	position = Vector(0.0f, 0.0f, 0.0f);
	fov = 45.0f;
	adjust_projection(static_cast<float>(std::max(1, width)),
		static_cast<float>(std::max(1, height)));
	calculate_matrix();
	rotational_vector = Vector(0.0f, 0.0f, 0.0f);
}

glm::mat4 Camera::build_projection(float width, float height) const {
	if (camera_mode == CameraMode::VIEW_2D)
		return glm::ortho(width * -0.5f, width * 0.5f,
			height * -0.5f, height * 0.5f, -100.0f, 100.0f);

	return glm::perspective(glm::radians(fov), (float)(width / height), 0.1f, 100.0f);
}

void Camera::set_projection(float width, float height) {
	resolution_width = std::max(1.0f, width);
	resolution_height = std::max(1.0f, height);
	projection = build_projection(resolution_width, resolution_height);
	matrices_dirty = true;
}

void Camera::adjust_projection(float width, float height) {
	if (width <= 0.0f || height <= 0.0f)
		throw std::invalid_argument("Camera resolution must be positive");
	base_width = width;
	base_height = height;
	set_projection(width, height);
}

void Camera::reset_projection_to_base() {
	set_projection(base_width, base_height);
}

void Camera::fit_projection_to_display(int display_width, int display_height) {
	if (display_width <= 0 || display_height <= 0)
		return;

	const float aspect = static_cast<float>(display_width) / display_height;
	const float base_aspect = base_width / base_height;

	if (aspect > base_aspect)
		set_projection(base_height * aspect, base_height);
	else
		set_projection(base_width, base_width / aspect);
}

glm::mat4 Camera::get_pixel_perfect_projection(int pixel_width,
	int pixel_height, float units) const {
	units = std::max(0.0001f, units);
	return build_projection(std::max(1, pixel_width) * units,
		std::max(1, pixel_height) * units);
}

glm::mat4 Camera::get_pixel_perfect_view(float units) const {
	units = std::max(0.0001f, units);
	const float x = std::floor(position.x / units) * units;
	const float y = std::floor(position.y / units) * units;
	return glm::translate(glm::mat4(1.0f), glm::vec3(-x, -y, 0.0f));
}

glm::vec2 Camera::get_pixel_fraction(float units) const {
	units = std::max(0.0001f, units);
	return glm::vec2(position.x / units - std::floor(position.x / units),
		position.y / units - std::floor(position.y / units));
}

bool Camera::is_dirty() const {
	bool position_dirty = (cached_position.x != position.x ||
		cached_position.y != position.y || cached_position.z != position.z);
	bool rotational_dirty = (
		cached_rotational_vector.x != rotational_vector.x ||
		cached_rotational_vector.y != rotational_vector.y ||
		cached_rotational_vector.z != rotational_vector.z);

	return matrices_dirty || position_dirty || cached_zoom != zoom ||
		cached_rotation != rotation || rotational_dirty;
}

void Camera::calculate_matrix() {
	if (!is_dirty())
		return;

	view = glm::mat4(1.0f);

	view = glm::scale(view, glm::vec3(zoom, zoom, 1.0f));

	view = glm::rotate(view, rotation, glm::vec3(0.0f, 0.0f, 1.0f));
	view = glm::rotate(view, rotational_vector.x, glm::vec3(1.0f, 0.0f, 0.0f));
	view = glm::rotate(view, rotational_vector.y, glm::vec3(0.0f, 1.0f, 0.0f));
	view = glm::rotate(view, rotational_vector.z, glm::vec3(0.0f, 0.0f, 1.0f));

	view = glm::translate(view,
		glm::vec3(-position.x, -position.y, 0.0f));

	view_projection = projection * view;

	cached_position = position;
	cached_zoom = zoom;
	cached_rotation = rotation;
	matrices_dirty = false;
}

const glm::mat4 &Camera::get_view_projection() {
	calculate_matrix();
	return view_projection;
}
const glm::mat4 &Camera::get_view() const { return view; }
const glm::mat4 &Camera::get_projection() const { return projection; }
glm::mat4 Camera::get_screen_projection() const { return projection; }

int Camera::get_width() const { return static_cast<int>(std::round(resolution_width)); }
int Camera::get_height() const { return static_cast<int>(std::round(resolution_height)); }
int Camera::get_resolution_x() const { return get_width(); }
int Camera::get_resolution_y() const { return get_height(); }
int Camera::get_base_width() const { return static_cast<int>(std::round(base_width)); }
int Camera::get_base_height() const { return static_cast<int>(std::round(base_height)); }
