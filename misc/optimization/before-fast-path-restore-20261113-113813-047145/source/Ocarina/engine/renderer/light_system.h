#pragma once

#include "light.h"

#include <array>
#include <memory>

class Shader;

class LightSystem {
public:
	// Matches MAX_LIGHTS in model.frag.
	static constexpr int MAX_LIGHTS = 32;

	// Owns a copy. The pointer stays valid until remove() or clear().
	// Returns nullptr when all slots are occupied.
	Light *add(const Light &light);
	bool remove(const Light *light);
	void clear();
	int get_count() const;
	void upload(Shader *shader) const;

private:
	std::array<std::unique_ptr<Light>, MAX_LIGHTS> lights;
};
