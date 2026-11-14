#pragma once

#include "../core/engine.h"

#include <map>
#include <set>

class TonicContext {
public:
	TonicContext();
	~TonicContext();
	TonicContext(const TonicContext &) = delete;
	TonicContext &operator=(const TonicContext &) = delete;

	void init_engine(const std::string &title, int width, int height);
	void shutdown();
	bool is_running() const;
	void quit();
	void begin_frame();
	void end_frame();
	void flush_frame();
	float delta_time() const;
	void clear(float r, float g, float b, float a);
	void set_color(float r, float g, float b, float a);
	void draw_rect(float x, float y, float width, float height, float rotation = 0, int z_order = 0);
	void draw_oval(float x, float y, float width, float height, float rotation = 0, int z_order = 0);
	void set_camera(float x, float y, float zoom, float rotation);
	bool key_down(const std::string &key);
	bool key_pressed(const std::string &key);
	bool key_released(const std::string &key);
	bool mouse_down(int button);
	float mouse_x();
	float mouse_y();
	int load_texture(const std::string &path);
	void unload_texture(int handle);
	void draw_texture(int handle, float x, float y, float width, float height, float rotation = 0, int z_order = 0);
	int load_font(const std::string &path, float size);
	void unload_font(int handle);
	void draw_text(int handle, const std::string &text, float x, float y, int z_order = 0);
	int load_sound(const std::string &path);
	void unload_sound(int handle);
	void play_sound(int handle);
	void stop_sound(int handle);
	void set_sound_volume(int handle, float volume);

private:
	class TonicScene : public Scene {
	public:
		explicit TonicScene(Engine &engine) : Scene(&engine) {}
		void init() override {}
		void update() override {}
		void render() override {}
	};

	Engine engine;
	TonicScene scene;
	bool initialized = false;
	bool running = false;
	bool in_frame = false;
	float elapsed = 1.0f / 60.0f;
	Uint64 previous = 0;
	static bool active_session;
	int next_handle = 1;
	std::map<int, Texture> textures;
	std::map<int, TTFont> fonts;
	std::map<int, Sound> sounds;
	std::set<int> pending_textures, pending_fonts, pending_sounds;

	void require_engine() const;
	void require_frame() const;
	void release_pending();
	int new_handle();
	static SDL_Scancode scancode(const std::string &key);
	static void check_path(const std::string &path);
};

