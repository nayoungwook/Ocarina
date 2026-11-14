#pragma once

#include <glm/glm.hpp>

class Camera;
class FrameBuffer;

template <typename T>
T clamp_value(const T &value, const T &minimum, const T &maximum) {
	return value < minimum ? minimum : (value > maximum ? maximum : value);
}

class Vector {
      private:
      public:
        float x, y, z;

        Vector(float x, float y, float z);
        Vector(float x, float y);
        Vector();

        Vector clone_vector() const;
        Vector normalized() const;
        void set_transform(float x, float y);
        void set_transform(float x, float y, float z);
        void translate(float delta_x, float delta_y);
        void translate(float delta_x, float delta_y, float delta_z);

        Vector operator+(const float other);
        Vector operator-(const float other);
        Vector operator*(const float other);
        Vector operator/(const float other);
        Vector operator%(const int other);
        Vector operator+(const Vector other);
        Vector operator-(const Vector other);
        Vector operator/(const Vector other);
        Vector operator*(const Vector other);
        Vector operator%(const Vector other);
        Vector &operator+=(const Vector &other);
        Vector &operator-=(const Vector &other);
        Vector normalize();
        float dist(const Vector other) const;
        float dist2d(const Vector other) const;
};

Vector world_to_screen(Camera *camera, const Vector &worldPos);
Vector screen_to_world(Camera *camera, const Vector &screenPos);
Vector world_to_screen(Camera *camera, const FrameBuffer *framebuffer,
	const Vector &worldPos);
Vector screen_to_world(Camera *camera, const FrameBuffer *framebuffer,
	const Vector &screenPos);
Vector world_to_ui_position(Camera *camera, const Vector &world_position);
Vector world_to_ui_position(Camera *camera, const FrameBuffer *framebuffer,
	const Vector &world_position);
float world_to_ui_size_x(Camera *camera, const Vector &world_position,
	float world_width);
float world_to_ui_size_y(Camera *camera, const Vector &world_position,
	float world_height);
float get_distance(const Vector &position, const Vector &position2);
float get_distance_squared(const Vector &position, const Vector &position2);
float get_angle(const Vector &position, const Vector &position2);
Vector get_velocity_towards(float move_speed, const Vector &position,
	const Vector &position2);
float dot(const Vector &v1, const Vector &v2);
