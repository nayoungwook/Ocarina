#include "error.h"
#include "../../ovm_panic.h"

void error(const std::string &msg) {
	throw OVMPanic("Tonic: " + msg);
}
