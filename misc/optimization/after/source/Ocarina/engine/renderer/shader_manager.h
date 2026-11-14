#pragma once

#include <iostream>
#include <unordered_map>

class Shader;

class ShaderManager {
      public:
        ShaderManager() = default;
        ~ShaderManager();
        ShaderManager(const ShaderManager &) = delete;
        ShaderManager &operator=(const ShaderManager &) = delete;
        void fetch_shader(const std::string &name, Shader *shader);
        Shader *get_shader(const std::string &name);

      private:
        std::unordered_map<std::string, Shader *> shaders;
};
