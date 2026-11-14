#pragma once

#include <mathf.h>

class Input;

enum class UIScrollDirection {
	Horizontal,
	Vertical,
};

// Logic-only scroll viewport. Input is supplied by the caller and rendering
// stays under the Scene's control, so it can drive any kind of content.
class UIScroll {
public:
	UIScroll(const Vector &position, float width, float height,
		UIScrollDirection direction = UIScrollDirection::Vertical);

	void set_position(const Vector &position);
	const Vector &get_position() const;
	void set_size(float width, float height);
	float get_width() const;
	float get_height() const;
	void set_direction(UIScrollDirection direction);
	UIScrollDirection get_direction() const;

	void set_content_length(float content_length);
	float get_content_length() const;
	void set_wheel_step(float step);
	void set_smoothing(float smoothing);
	void set_enabled(bool enabled);
	bool is_enabled() const;

	// True means this viewport handled a wheel delta this update.
	bool update(Input *input);
	bool update(Input *input, int item_count, float item_extent,
		float item_spacing = 0.0f);

	void set_offset(float offset);
	void scroll_by(float distance);
	void scroll_to_start();
	void scroll_to_end();
	float get_offset() const;
	float get_target_offset() const;
	float get_max_offset() const;
	float get_normalized_offset() const;
	bool is_scrollable() const;

	bool contains(float x, float y) const;
	bool is_item_visible(const Vector &item_position, float item_width,
		float item_height) const;
	bool is_item_fully_visible(const Vector &item_position, float item_width,
		float item_height) const;

	Vector get_item_position(int index, int item_count, float item_extent,
		float item_spacing = 0.0f) const;

private:
	float viewport_length() const;
	float item_content_length(int item_count, float item_extent,
		float item_spacing) const;
	void clamp_offsets();

	Vector position;
	float width = 0.0f;
	float height = 0.0f;
	float content_length = 0.0f;
	float wheel_step = 48.0f;
	float smoothing = 0.2f;
	float offset = 0.0f;
	float target_offset = 0.0f;
	bool enabled = true;
	UIScrollDirection direction = UIScrollDirection::Vertical;
};
