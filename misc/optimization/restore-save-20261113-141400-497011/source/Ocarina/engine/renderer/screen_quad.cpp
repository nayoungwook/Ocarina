#include "screen_quad.h"

#include "display.h"
#include "framebuffer.h"

#include <GL/glew.h>

unsigned ScreenQuad::vao = 0;
unsigned ScreenQuad::vbo = 0;
unsigned ScreenQuad::ebo = 0;

void ScreenQuad::init() {
	// Full-screen quad used by post-processing and final composition shaders.
	// Vertices are already in clip space, so no camera matrix is needed.
	if (vao != 0)
		return;

	float vertices[] = {
		-1.0f, -1.0f, 0.0f, 0.0f,
		 1.0f, -1.0f, 1.0f, 0.0f,
		 1.0f,  1.0f, 1.0f, 1.0f,
		-1.0f,  1.0f, 0.0f, 1.0f,
	};
	unsigned indices[] = { 0, 1, 2, 2, 3, 0 };

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
		GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
		(void *)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
		(void *)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glBindVertexArray(0);
}

void ScreenQuad::render(FrameBuffer *framebuffer) {
	// Bind the viewport that matches the destination before drawing the shared
	// quad. The caller is responsible for binding the shader and textures.
	init();
	if (framebuffer != nullptr)
		framebuffer->apply_viewport();
	else
		Display::apply_current_screen_viewport();

	glBindVertexArray(vao);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void ScreenQuad::render_current_viewport() {
	init();
	glBindVertexArray(vao);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void ScreenQuad::dispose() {
	if (ebo != 0)
		glDeleteBuffers(1, &ebo);
	if (vbo != 0)
		glDeleteBuffers(1, &vbo);
	if (vao != 0)
		glDeleteVertexArrays(1, &vao);
	ebo = 0;
	vbo = 0;
	vao = 0;
}
