#include "light_system.h"
#include "shader.h"

#include <string>

Light *LightSystem::add(const Light &light) {
	for (auto &slot : lights) {
		if (!slot) {
			slot = std::make_unique<Light>(light);
			return slot.get();
		}
	}
	return nullptr;
}

bool LightSystem::remove(const Light *light) {
	if (light == nullptr)
		return false;
	for (auto &slot : lights) {
		if (slot.get() == light) {
			slot.reset();
			return true;
		}
	}
	return false;
}

void LightSystem::clear() {
	for (auto &slot : lights)
		slot.reset();
}

int LightSystem::get_count() const {
	int count = 0;
	for (const auto &slot : lights) {
		if (slot)
			++count;
	}
	return count;
}

void LightSystem::upload(Shader *shader) const {
	if (shader == nullptr)
		return;

	int index = 0;
	for (const auto &slot : lights) {
		if (!slot)
			continue;
		const std::string prefix = "uLights[" + std::to_string(index) + "].";
		shader->upload_vec3(prefix + "position",
			glm::vec3(slot->position.x, slot->position.y, slot->position.z));
		shader->upload_vec3(prefix + "color",
			glm::vec3(slot->color.r, slot->color.g, slot->color.b));
		shader->upload_float(prefix + "intensity", slot->intensity);
		++index;
	}
	// Also upload zero so removed lights cannot remain active in the shader.
	shader->upload_int("uLightCount", index);
}
