#include "mathf.h"
#include "camera.h"
#include "framebuffer.h"
#include "display.h"

#include <algorithm>
#include <cmath>

// Lightweight math primitives used by the engine-facing API. Rendering code
// converts these values to GLM only where matrices or GPU data are built.
Vector::Vector(float x, float y, float z) : x(x), y(y), z(z) {};
Vector::Vector(float x, float y) : x(x), y(y), z(0) {};
Vector::Vector() : x(0), y(0), z(0) {};

Vector Vector::clone_vector() const { return Vector(x, y, z); }

Vector Vector::normalized() const {
        float length = (float)sqrt(x * x + y * y + z * z);
        if (length == 0)
                return Vector(0, 0, 0);
        return Vector(x / length, y / length, z / length);
}

void Vector::set_transform(float x, float y) {
        this->x = x;
        this->y = y;
}
void Vector::set_transform(float x, float y, float z) {
        this->x = x;
        this->y = y;
        this->z = z;
}
void Vector::translate(float delta_x, float delta_y) {
        x += delta_x;
        y += delta_y;
}
void Vector::translate(float delta_x, float delta_y, float delta_z) {
        x += delta_x;
        y += delta_y;
        z += delta_z;
}

Vector Vector::operator+(const float other) {
        return Vector(x + other, y + other, z + other);
}

Vector Vector::operator-(const float other) {
        return Vector(x - other, y - other, z - other);
}

Vector Vector::operator/(const float other) {
        return Vector(x / other, y / other, z / other);
}

Vector Vector::operator*(const float other) {
        return Vector(x * other, y * other, z * other);
}

Vector Vector::operator%(const int other) {
        return Vector((int)x % other, (int)y % other, (int)z % other);
}

Vector Vector::operator+(const Vector other) {
        return Vector(x + other.x, y + other.y, z + other.z);
}

Vector Vector::operator-(const Vector other) {
        return Vector(x - other.x, y - other.y, z - other.z);
}

Vector Vector::operator/(const Vector other) {
        return Vector(x / other.x, y / other.y, z / other.z);
}

Vector Vector::operator*(const Vector other) {
        return Vector(x * other.x, y * other.y, z * other.z);
}

Vector Vector::operator%(const Vector other) {
        return Vector((int)x % (int)other.x, (int)y % (int)other.y,
                      (int)z % (int)other.z);
}

Vector &Vector::operator+=(const Vector &other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
}

Vector &Vector::operator-=(const Vector &other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
}

Vector Vector::normalize() {
        return normalized();
}

float Vector::dist2d(const Vector other) const {
        return sqrt((x - other.x) * (x - other.x) +
                    (y - other.y) * (y - other.y));
}

float Vector::dist(const Vector other) const {
        return sqrt((x - other.x) * (x - other.x) +
                    (y - other.y) * (y - other.y) +
                    (z - other.z) * (z - other.z));
}

Vector world_to_screen(Camera *camera, const Vector &world_pos) {
	// Project a world-space point into the camera's logical screen coordinate
	// system, applying the same zoom and rotation used for rendering.
	if (camera == nullptr)
		return world_pos;
	Display *display = Display::get_current();
	const glm::vec4 clip = camera->get_view_projection() *
		glm::vec4(world_pos.x, world_pos.y, world_pos.z, 1.0f);
	const glm::vec4 ndc = clip / clip.w;
	const float width = display == nullptr ? camera->get_width() :
		static_cast<float>(display->get_width());
	const float height = display == nullptr ? camera->get_height() :
		static_cast<float>(display->get_height());
	return Vector(ndc.x * 0.5f * width, ndc.y * 0.5f * height,
		world_pos.z);
}

Vector screen_to_world(Camera *camera, const Vector &screen_pos) {
	// Mouse/UI input arrives in screen-space pixels. Undo projection, zoom,
	// rotation, and camera translation to get the corresponding world point.
	if (camera == nullptr)
		return screen_pos;
	Display *display = Display::get_current();
	const float width = display == nullptr ? camera->get_width() :
		static_cast<float>(display->get_width());
	const float height = display == nullptr ? camera->get_height() :
		static_cast<float>(display->get_height());
	const glm::vec4 clip(2.0f * screen_pos.x / width,
		2.0f * screen_pos.y / height, 0.0f, 1.0f);
	const glm::vec4 world = glm::inverse(camera->get_view_projection()) * clip;
	return Vector(world.x / world.w, world.y / world.w, screen_pos.z);
}

Vector world_to_screen(Camera *camera, const FrameBuffer *framebuffer,
	const Vector &world_pos) {
	if (camera == nullptr || framebuffer == nullptr ||
		!framebuffer->is_pixel_perfect())
		return world_to_screen(camera, world_pos);

	Display *display = Display::get_current();
	const float display_width = display == nullptr ?
		static_cast<float>(camera->get_width()) : static_cast<float>(display->get_width());
	const float display_height = display == nullptr ?
		static_cast<float>(camera->get_height()) : static_cast<float>(display->get_height());
	const float zoom = std::max(0.0001f, std::abs(camera->zoom));
	const float direction = camera->zoom < 0.0f ? -1.0f : 1.0f;
	const float cosine = std::cos(camera->rotation);
	const float sine = std::sin(camera->rotation);
	const float dx = world_pos.x - camera->position.x;
	const float dy = world_pos.y - camera->position.y;
	// world-units-per-pixel cancels with the resolve source scale, so output
	// coordinates retain the camera's normal world-to-screen scale.
	const float output_x = (cosine * dx - sine * dy) * zoom * direction;
	const float output_y = (sine * dx + cosine * dy) * zoom * direction;
	return Vector(output_x * display_width / framebuffer->get_pixel_view_width(),
		output_y * display_height / framebuffer->get_pixel_view_height(), world_pos.z);
}

Vector screen_to_world(Camera *camera, const FrameBuffer *framebuffer,
	const Vector &screen_pos) {
	if (camera == nullptr || framebuffer == nullptr ||
		!framebuffer->is_pixel_perfect())
		return screen_to_world(camera, screen_pos);

	Display *display = Display::get_current();
	const float display_width = display == nullptr ?
		static_cast<float>(camera->get_width()) : static_cast<float>(display->get_width());
	const float display_height = display == nullptr ?
		static_cast<float>(camera->get_height()) : static_cast<float>(display->get_height());
	const float zoom = std::max(0.0001f, std::abs(camera->zoom));
	const float direction = camera->zoom < 0.0f ? -1.0f : 1.0f;
	const float output_x = screen_pos.x * framebuffer->get_pixel_view_width() / display_width;
	const float output_y = screen_pos.y * framebuffer->get_pixel_view_height() / display_height;
	const float cosine = std::cos(camera->rotation);
	const float sine = std::sin(camera->rotation);
	// This is the inverse of the world-to-screen transform above. The source
	// texel scale cancels with the FrameBuffer's world-units-per-pixel value.
	const float world_x = (cosine * output_x + sine * output_y) / zoom * direction;
	const float world_y = (-sine * output_x + cosine * output_y) / zoom * direction;
	return Vector(camera->position.x + world_x,
		camera->position.y + world_y, screen_pos.z);
}

Vector world_to_ui_position(Camera *camera, const Vector &world_position) {
	Display *display = Display::get_current();
	if (display == nullptr || display->get_viewport_width() == 0 ||
		display->get_viewport_height() == 0)
		return Vector(0.0f, 0.0f, world_position.z);
	const Vector screen = world_to_screen(camera, world_position);
	const float window_x = screen.x + display->get_width() * 0.5f;
	const float window_y = screen.y + display->get_height() * 0.5f;
	const float ui_x = (window_x - display->get_viewport_x()) /
		display->get_viewport_width() - 0.5f;
	const float ui_y = (window_y - display->get_viewport_y()) /
		display->get_viewport_height() - 0.5f;
	return Vector(ui_x * camera->get_resolution_x(),
		ui_y * camera->get_resolution_y(), world_position.z);
}

Vector world_to_ui_position(Camera *camera, const FrameBuffer *framebuffer,
	const Vector &world_position) {
	if (camera == nullptr || framebuffer == nullptr)
		return world_to_ui_position(camera, world_position);

	// world_to_screen(pixel-perfect) returns centered window coordinates so it
	// can be inverted by screen_to_world. Convert those physical screen units
	// to the logical CAMERA units used by a regular UI framebuffer.
	Display *display = Display::get_current();
	const float screen_width = display == nullptr ?
		static_cast<float>(camera->get_width()) :
		static_cast<float>(std::max(1, display->get_width()));
	const float screen_height = display == nullptr ?
		static_cast<float>(camera->get_height()) :
		static_cast<float>(std::max(1, display->get_height()));
	const Vector screen = world_to_screen(camera, framebuffer, world_position);
	return Vector(screen.x * camera->get_resolution_x() / screen_width,
		screen.y * camera->get_resolution_y() / screen_height,
		world_position.z);
}

float world_to_ui_size_x(Camera *camera, const Vector &position, float width) {
	const Vector start = world_to_ui_position(camera, position);
	const Vector end = world_to_ui_position(camera,
		Vector(position.x + width, position.y, position.z));
	return std::abs(end.x - start.x);
}

float world_to_ui_size_y(Camera *camera, const Vector &position, float height) {
	const Vector start = world_to_ui_position(camera, position);
	const Vector end = world_to_ui_position(camera,
		Vector(position.x, position.y + height, position.z));
	return std::abs(end.y - start.y);
}

float get_distance(const Vector &position, const Vector &position2) {
	// Toffee's Mathf distance helpers intentionally operate in 2D.  The z
	// component is a render-order value and must not affect gameplay distance.
	return position.dist2d(position2);
}

float get_distance_squared(const Vector &position, const Vector &position2) {
	float x = position.x - position2.x;
	float y = position.y - position2.y;
	return x * x + y * y;
}

float get_angle(const Vector &position, const Vector &position2) {
	return -std::atan2(position2.y - position.y,
		position2.x - position.x);
}

Vector get_velocity_towards(float move_speed, const Vector &position,
	const Vector &position2) {
	float angle = get_angle(position, position2);
	return Vector(std::cos(angle) * move_speed,
		std::sin(angle) * move_speed);
}

float dot(const Vector &v1, const Vector &v2) {
        return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}
