#pragma once

#include "mathf.h"
#include <color.h>

class Light {
public:

	Light(Vector position);
	Light(Vector position, Color color);
	Light(Vector position, Color color, float intensity);

	Vector position;
	Color color;
	float intensity;
};
