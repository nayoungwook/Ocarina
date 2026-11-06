#pragma once

#include <GL/glew.h>

#include <vector>

class Mesh {
      public:
        Mesh(const float *vertices, int vertex_count, const unsigned *indices,
             int index_count, int vertex_float_count = 5);
        Mesh(const std::vector<float> &vertices,
             const std::vector<unsigned> &indices, int vertex_float_count = 5);
        ~Mesh();

        unsigned get_vao() const;
        int get_index_count() const;
        GLenum get_draw_mode() const;
        void set_draw_mode(GLenum draw_mode);

      private:
        void init(const float *vertices, int vertex_count,
                  const unsigned *indices, int index_count);

        unsigned vao = 0;
        unsigned vbo = 0;
        unsigned ebo = 0;
        int index_count = 0;
        int vertex_float_count = 5;
        GLenum draw_mode = GL_TRIANGLES;
};
