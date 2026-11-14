#include <light.h>

Light::Light(Vector position) : Light(position, Color(1, 1, 1), 10.0f) {

}

Light::Light(Vector position, Color color) : Light(position, color, 10.0f) {

}

Light::Light(Vector position, Color color, float intensity) : position(position), color(color), intensity(intensity) {

}
