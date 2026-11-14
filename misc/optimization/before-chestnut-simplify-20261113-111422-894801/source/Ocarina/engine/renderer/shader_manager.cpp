#include "shader_manager.h"

#include "error.h"
#include "shader.h"

ShaderManager::~ShaderManager() {
        for (const auto &entry : shaders) delete entry.second;
}

void ShaderManager::fetch_shader(const std::string &name, Shader *shader) {
        // Store named shaders once during startup so scenes can ask for common
        // programs without owning their construction order.
        this->shaders.insert(std::make_pair(name, shader));
}

Shader *ShaderManager::get_shader(const std::string &name) {
        // Missing shaders are fatal because render commands generally cannot
        // recover with a sensible visual fallback.
        Shader *result = nullptr;
        std::unordered_map<std::string, Shader *>::iterator shader_it =
            this->shaders.find(name);

        if (shader_it == this->shaders.end()) {
                error("Failed to find shader " + name);
        }

        return shader_it->second;
}
