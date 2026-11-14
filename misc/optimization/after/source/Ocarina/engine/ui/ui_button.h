#pragma once

#include <color.h>
#include <mathf.h>

#include <cstdint>
#include <functional>

class Input;
class Renderer;
class Texture;

enum class UIButtonState {
	Normal,
	Hovered,
	Pressed,
	Disabled,
};

// A small, renderer-independent UI control. Input and rendering are supplied
// by the caller so the button can be reused by any Scene without owning an
// Engine or a game-specific manager.
class UIButton {
public:
	UIButton(Texture *texture, const Vector &position, float width,
		float height);

	void set_texture(Texture *texture);
	void set_position(const Vector &position);
	const Vector &get_position() const;
	void set_size(float width, float height);
	float get_width() const;
	float get_height() const;
	void set_render_size(float width, float height);

	// State atlas slots are optional. A negative state slot falls back to the
	// normal slot; a negative normal slot draws the entire texture.
	void set_atlas_indices(int normal, int hovered = -1, int pressed = -1,
		int disabled = -1);
	void set_atlas_index(int normal);
	void set_use_hover_atlas(bool enabled);
	void set_tints(const Color &normal, const Color &hovered,
		const Color &pressed, const Color &disabled);

	void set_mouse_button(std::uint8_t button);
	void set_interaction_enabled(bool enabled);
	bool is_interaction_enabled() const;
	void set_visible(bool visible);
	bool is_visible() const;

	void set_on_press(std::function<void()> callback);
	void set_on_release(std::function<void()> callback);
	void set_on_click(std::function<void()> callback);

	void update(Input *input);
	void render(Renderer *renderer) const;

	bool contains(float x, float y) const;
	bool is_hovered() const;
	bool is_pressed() const;
	bool was_clicked() const;
	UIButtonState get_state() const;

private:
	int atlas_for_state(UIButtonState state) const;
	Color tint_for_state(UIButtonState state) const;
	void cancel_interaction();

	Texture *texture = nullptr;
	Vector position;
	float width = 0.0f;
	float height = 0.0f;
	float render_width = 0.0f;
	float render_height = 0.0f;

	int normal_atlas = -1;
	int hovered_atlas = -1;
	int pressed_atlas = -1;
	int disabled_atlas = -1;
	bool use_hover_atlas = true;

	Color normal_tint = Color::white();
	Color hovered_tint = Color::white();
	Color pressed_tint = Color::white();
	Color disabled_tint = Color(0.5f, 0.5f, 0.5f, 0.75f);

	std::uint8_t mouse_button = 1;
	bool interaction_enabled = true;
	bool visible = true;
	bool hovered = false;
	bool pressed = false;
	bool clicked = false;

	std::function<void()> on_press;
	std::function<void()> on_release;
	std::function<void()> on_click;
};
