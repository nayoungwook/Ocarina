#include <ui_scroll.h>

#include <input.h>

#include <algorithm>
#include <cmath>

UIScroll::UIScroll(const Vector &position, float width, float height,
	UIScrollDirection direction)
	: position(position), width(std::max(0.0f, width)),
	  height(std::max(0.0f, height)), direction(direction) {}

void UIScroll::set_position(const Vector &value) { position = value; }

const Vector &UIScroll::get_position() const { return position; }

void UIScroll::set_size(float value_width, float value_height) {
	width = std::max(0.0f, value_width);
	height = std::max(0.0f, value_height);
	clamp_offsets();
}

float UIScroll::get_width() const { return width; }

float UIScroll::get_height() const { return height; }

void UIScroll::set_direction(UIScrollDirection value) {
	direction = value;
	clamp_offsets();
}

UIScrollDirection UIScroll::get_direction() const { return direction; }

void UIScroll::set_content_length(float value) {
	content_length = std::max(0.0f, value);
	clamp_offsets();
}

float UIScroll::get_content_length() const { return content_length; }

void UIScroll::set_wheel_step(float value) {
	wheel_step = std::max(0.0f, value);
}

void UIScroll::set_smoothing(float value) {
	smoothing = clamp_value(value, 0.0f, 1.0f);
}

void UIScroll::set_enabled(bool value) { enabled = value; }

bool UIScroll::is_enabled() const { return enabled; }

bool UIScroll::update(Input *input) {
	bool consumed = false;
	if (enabled && input != nullptr && is_scrollable() &&
		contains(input->get_ui_mouse_x(), input->get_ui_mouse_y())) {
		float wheel = input->get_mouse_wheel_y();
		if (direction == UIScrollDirection::Horizontal &&
			std::abs(input->get_mouse_wheel_x()) > std::abs(wheel)) {
			// SDL's positive X scrolls right; positive Y scrolls up.
			wheel = -input->get_mouse_wheel_x();
		}
		if (wheel != 0.0f) {
			target_offset -= wheel * wheel_step;
			target_offset = clamp_value(target_offset, 0.0f,
				get_max_offset());
			consumed = true;
		}
	}

	if (smoothing <= 0.0f || smoothing >= 1.0f) {
		offset = target_offset;
	}
	else {
		offset += (target_offset - offset) * smoothing;
		if (std::abs(target_offset - offset) < 0.01f)
			offset = target_offset;
	}
	clamp_offsets();
	return consumed;
}

bool UIScroll::update(Input *input, int item_count, float item_extent,
	float item_spacing) {
	set_content_length(item_content_length(item_count, item_extent,
		item_spacing));
	return update(input);
}

void UIScroll::set_offset(float value) {
	offset = value;
	target_offset = value;
	clamp_offsets();
}

void UIScroll::scroll_by(float distance) {
	target_offset += distance;
	target_offset = clamp_value(target_offset, 0.0f, get_max_offset());
}

void UIScroll::scroll_to_start() { target_offset = 0.0f; }

void UIScroll::scroll_to_end() { target_offset = get_max_offset(); }

float UIScroll::get_offset() const { return offset; }

float UIScroll::get_target_offset() const { return target_offset; }

float UIScroll::get_max_offset() const {
	return std::max(0.0f, content_length - viewport_length());
}

float UIScroll::get_normalized_offset() const {
	const float max_offset = get_max_offset();
	return max_offset <= 0.0f ? 0.0f : offset / max_offset;
}

bool UIScroll::is_scrollable() const { return get_max_offset() > 0.0f; }

bool UIScroll::contains(float x, float y) const {
	return std::abs(x - position.x) <= width * 0.5f &&
		std::abs(y - position.y) <= height * 0.5f;
}

bool UIScroll::is_item_visible(const Vector &item_position, float item_width,
	float item_height) const {
	return std::abs(item_position.x - position.x) <=
		(width + std::max(0.0f, item_width)) * 0.5f &&
		std::abs(item_position.y - position.y) <=
		(height + std::max(0.0f, item_height)) * 0.5f;
}

bool UIScroll::is_item_fully_visible(const Vector &item_position,
	float item_width, float item_height) const {
	const float remaining_x = width - std::max(0.0f, item_width);
	const float remaining_y = height - std::max(0.0f, item_height);
	return remaining_x >= 0.0f && remaining_y >= 0.0f &&
		std::abs(item_position.x - position.x) <= remaining_x * 0.5f &&
		std::abs(item_position.y - position.y) <= remaining_y * 0.5f;
}

Vector UIScroll::get_item_position(int index, int item_count,
	float item_extent, float item_spacing) const {
	if (item_count <= 0)
		return position;

	const float extent = std::max(0.0f, item_extent);
	const float spacing = std::max(0.0f, item_spacing);
	const float pitch = extent + spacing;
	const float layout_length = item_content_length(item_count, extent,
		spacing);

	if (direction == UIScrollDirection::Horizontal) {
		const float start_x = layout_length <= width ?
			position.x - layout_length * 0.5f + extent * 0.5f :
			position.x - width * 0.5f + extent * 0.5f;
		return Vector(start_x + index * pitch - offset, position.y,
			position.z);
	}

	const float start_y = layout_length <= height ?
		position.y + layout_length * 0.5f - extent * 0.5f :
		position.y + height * 0.5f - extent * 0.5f;

	return Vector(position.x, start_y - index * pitch + offset, position.z);
}

float UIScroll::viewport_length() const {
	return direction == UIScrollDirection::Horizontal ? width : height;
}

float UIScroll::item_content_length(int item_count, float item_extent,
	float item_spacing) const {
	if (item_count <= 0)
		return 0.0f;
	const float extent = std::max(0.0f, item_extent);
	const float spacing = std::max(0.0f, item_spacing);
	return item_count * extent + (item_count - 1) * spacing;
}

void UIScroll::clamp_offsets() {
	const float max_offset = get_max_offset();
	target_offset = clamp_value(target_offset, 0.0f, max_offset);
	offset = clamp_value(offset, 0.0f, max_offset);
}
