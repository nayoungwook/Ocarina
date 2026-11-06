#include "error.h"
#include <stdexcept>

void error(const std::string &msg) {
	throw std::runtime_error("Tonic: " + msg);
}
