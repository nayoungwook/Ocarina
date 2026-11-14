#pragma once

#include <iostream>
#include <functional>

#include "camera.h"
#include "display.h"
#include "renderer.h"
#include "scene.h"
#include "shader.h"
#include "shader_manager.h"
#include "texture.h"
#include "input.h"
#include "sound.h"
#include "mesh.h"
#include "ttfont.h"
#include "screen_quad.h"
#include "light_system.h"

class Display;
class Camera;
class Renderer;
class ShaderManager;
class Scene;
class Input;
class FrameBuffer;
class OVM;

class Engine {
      public:
        explicit Engine(OVM *vm = nullptr) : vm(vm) {}
        ~Engine();
        Engine(const Engine &) = delete;
        Engine &operator=(const Engine &) = delete;
        void shutdown();
        Display *get_display();
        Camera *get_camera();
        Renderer *get_renderer();
        ShaderManager *get_shader_manager();
        Scene *get_current_scene();
        Input *get_input();
        FrameBuffer *get_framebuffer();

        // Engine owns a copy; nullptr means full. Remove/clear invalidates it.
        Light *add_light(const Light &light);
        bool remove_light(const Light *light);
        void clear_lights();
        int get_light_count() const;
        void upload_lights(Shader *shader) const;

        float get_width(), get_height();

        void set_framebuffer(FrameBuffer *framebuffer);
        void set_automatic_presentation(bool enabled);
        bool is_automatic_presentation() const;

        void set_scene(Scene *scene);
		void set_after_scene_update(std::function<void(Engine *)> callback);
        void start();
        // Run one frame on the SDL/main thread. False means closed or inactive.
        bool run_frame();

        void update_hierarchy();
        void render_hierarchy();

        void init_window(const std::string &title, int width, int height);
        void init_engine();

        int get_frames();
        int get_ticks();

      private:
        float width = 0, height = 0;

        int cached_frames = 0, cached_ticks = 0;

        FrameBuffer *current_framebuffer = nullptr;
        Scene *current_scene = nullptr;
		Scene *vm_render_scene = nullptr;
		std::function<void(Engine *)> after_scene_update;
        OVM *vm = nullptr;
        Display *display = nullptr;
        Camera *camera = nullptr;
        Renderer *renderer = nullptr;
        ShaderManager *shader_manager = nullptr;
        Input *input = nullptr;
        bool automatic_presentation = true;
        bool sdl_initialized = false;
        bool loop_started = false;
        Uint64 previous_frame = 0;
        double update_lag = 0.0, fps_timer = 0.0;
        int frame_count = 0, tick_count = 0;
        LightSystem lights;
};
