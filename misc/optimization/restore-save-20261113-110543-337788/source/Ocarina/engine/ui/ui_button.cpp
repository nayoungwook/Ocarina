#include <ui_button.h>

#include <input.h>
#include <renderer.h>
#include <shader.h>
#include <texture.h>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <utility>

UIButton::UIButton(Texture *texture, const Vector &position, float width,
	float height)
	: texture(texture), position(position), width(std::max(0.0f, width)),
	  height(std::max(0.0f, height)), render_width(std::max(0.0f, width)),
	  render_height(std::max(0.0f, height)) {}

void UIButton::set_texture(Texture *value) { texture = value; }

void UIButton::set_position(const Vector &value) { position = value; }

const Vector &UIButton::get_position() const { return position; }

void UIButton::set_size(float value_width, float value_height) {
	width = std::max(0.0f, value_width);
	height = std::max(0.0f, value_height);
}

float UIButton::get_width() const { return width; }

float UIButton::get_height() const { return height; }

void UIButton::set_render_size(float value_width, float value_height) {
	render_width = std::max(0.0f, value_width);
	render_height = std::max(0.0f, value_height);
}

void UIButton::set_atlas_indices(int normal, int hovered_value,
	int pressed_value, int disabled_value) {
	normal_atlas = normal;
	hovered_atlas = hovered_value;
	pressed_atlas = pressed_value;
	disabled_atlas = disabled_value;
}

void UIButton::set_atlas_index(int normal) {
	normal_atlas = normal;
	hovered_atlas = use_hover_atlas && normal >= 0 ? normal + 1 : normal;
	pressed_atlas = hovered_atlas;
	disabled_atlas = normal;
}

void UIButton::set_use_hover_atlas(bool enabled) {
	use_hover_atlas = enabled;
	hovered_atlas = enabled && normal_atlas >= 0 ? normal_atlas + 1 :
		normal_atlas;
	pressed_atlas = hovered_atlas;
}

void UIButton::set_tints(const Color &normal, const Color &hovered_value,
	const Color &pressed_value, const Color &disabled_value) {
	normal_tint = normal;
	hovered_tint = hovered_value;
	pressed_tint = pressed_value;
	disabled_tint = disabled_value;
}

void UIButton::set_mouse_button(std::uint8_t button) {
	mouse_button = button;
}

void UIButton::set_interaction_enabled(bool enabled) {
	interaction_enabled = enabled;
	if (!enabled)
		cancel_interaction();
}

bool UIButton::is_interaction_enabled() const {
	return interaction_enabled;
}

void UIButton::set_visible(bool value) {
	visible = value;
	if (!value)
		cancel_interaction();
}

bool UIButton::is_visible() const { return visible; }

void UIButton::set_on_press(std::function<void()> callback) {
	on_press = std::move(callback);
}

void UIButton::set_on_release(std::function<void()> callback) {
	on_release = std::move(callback);
}

void UIButton::set_on_click(std::function<void()> callback) {
	on_click = std::move(callback);
}

void UIButton::update(Input *input) {
	clicked = false;
	if (input == nullptr || !visible || !interaction_enabled) {
		cancel_interaction();
		return;
	}

	hovered = contains(input->get_ui_mouse_x(), input->get_ui_mouse_y());
	const Uint8 button = static_cast<Uint8>(mouse_button);

	if (hovered && input->is_mouse_pressed(button)) {
		pressed = true;
		if (on_press)
			on_press();
	}

	if (pressed && input->is_mouse_released(button)) {
		pressed = false;
		if (on_release)
			on_release();
		if (hovered) {
			clicked = true;
			if (on_click)
				on_click();
		}
	}
	else if (pressed && !input->is_mouse_down(button)) {
		// This also recovers if focus was lost before a release event arrived.
		pressed = false;
	}
}

void UIButton::render(Renderer *renderer) const {
	if (renderer == nullptr || texture == nullptr || !visible)
		return;

	Shader *shader = Shader::get_current();
	if (shader == nullptr)
		shader = renderer->get_shader();
	if (shader == nullptr)
		return;

	const UIButtonState state = get_state();
	renderer->render_texture(texture, shader, position, render_width,
		render_height, 0.0f, tint_for_state(state), atlas_for_state(state),
		true);
}

bool UIButton::contains(float x, float y) const {
	return visible && std::abs(x - position.x) <= width * 0.5f &&
		std::abs(y - position.y) <= height * 0.5f;
}

bool UIButton::is_hovered() const { return hovered; }

bool UIButton::is_pressed() const { return pressed; }

bool UIButton::was_clicked() const { return clicked; }

UIButtonState UIButton::get_state() const {
	if (!interaction_enabled)
		return UIButtonState::Disabled;
	if (pressed && hovered)
		return UIButtonState::Pressed;
	if (hovered)
		return UIButtonState::Hovered;

	return UIButtonState::Normal;
}

int UIButton::atlas_for_state(UIButtonState state) const {
	int state_atlas = normal_atlas;
	switch (state) {
	case UIButtonState::Hovered:
		state_atlas = hovered_atlas;
		break;
	case UIButtonState::Pressed:
		state_atlas = pressed_atlas;
		break;
	case UIButtonState::Disabled:
		state_atlas = disabled_atlas;
		break;
	case UIButtonState::Normal:
		break;
	}
	return state_atlas < 0 ? normal_atlas : state_atlas;
}

Color UIButton::tint_for_state(UIButtonState state) const {
	switch (state) {
	case UIButtonState::Hovered:
		return hovered_tint;
	case UIButtonState::Pressed:
		return pressed_tint;
	case UIButtonState::Disabled:
		return disabled_tint;
	case UIButtonState::Normal:
	default:
		return normal_tint;
	}
}

void UIButton::cancel_interaction() {
	hovered = false;
	pressed = false;
	clicked = false;
}
