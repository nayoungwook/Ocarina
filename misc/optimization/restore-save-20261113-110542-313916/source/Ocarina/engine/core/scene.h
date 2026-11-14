#pragma once

#include "render_context.h"
#include "sprite_batch.h"
#include <vector>

class Engine;
class Renderer;

class Scene {
public:
	explicit Scene(Engine *engine);
	virtual ~Scene() = default;

	virtual void init() = 0;
	virtual void update() = 0;
	virtual void render() = 0;

	// Separate alpha keeps coverage correct for transparent composition targets.
	void flush_render_context(bool separate_alpha_blending = false);
	void add_render_context(const RenderContext &render_context);
	bool get_last_render_context(RenderContext &render_context) const;
	bool copy_last_render_context_to_framebuffer(FrameBuffer *framebuffer);
	void start_frame();
	Engine *get_engine();

protected:
	Engine *engine;

private:
	void render_queue(std::vector<RenderContext> &queue, bool needs_sort,
		bool separate_alpha_blending);
	void flush_batch(Renderer *renderer, std::vector<SpriteInstance> &batch,
		Texture *texture);
	glm::mat4 view_projection_for(const RenderContext &context) const;
	bool is_visible(const RenderContext &context) const;

	std::vector<RenderContext> render_context_queue;
	std::vector<SpriteInstance> instance_batch;
	RenderContext last_render_context = {};
	bool has_last_render_context = false;
	bool render_context_queue_sorted = true;
};

void set_scene(Scene *scene);
