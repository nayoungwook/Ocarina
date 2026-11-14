#pragma once

#include <GL/glew.h>
#include <SDL3/SDL.h>
#include <string>

class Camera;

class Display {
public:
	Display(const std::string &title, int width, int height);
	~Display();

	std::string get_title() const;
	int get_width() const;
	int get_height() const;
	int get_drawable_width() const;
	int get_drawable_height() const;

	void set_camera(Camera *camera);
	Camera *get_camera() const;
	void update_viewport(int window_width = 0, int window_height = 0);
	void apply_screen_viewport() const;
	static void apply_current_screen_viewport();
	static Display *get_current();

	void set_fullscreen();
	void set_windowed(int width, int height);
	void start();
	SDL_Window *get_window();

	int get_viewport_x() const;
	int get_viewport_y() const;
	int get_viewport_width() const;
	int get_viewport_height() const;
	int get_framebuffer_viewport_x() const;
	int get_framebuffer_viewport_y() const;
	int get_framebuffer_viewport_width() const;
	int get_framebuffer_viewport_height() const;

private:
	void calculate_viewport();

	SDL_GLContext gl_context = nullptr;
	SDL_Window *window = nullptr;
	std::string title;
	int width = 1;
	int height = 1;
	int drawable_width = 1;
	int drawable_height = 1;
	int viewport_x = 0;
	int viewport_y = 0;
	int viewport_width = 1;
	int viewport_height = 1;
	int framebuffer_viewport_x = 0;
	int framebuffer_viewport_y = 0;
	int framebuffer_viewport_width = 1;
	int framebuffer_viewport_height = 1;
	Camera *camera = nullptr;
};
