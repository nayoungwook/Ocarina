#include "vm_memory.h"
#include "ovm_panic.h"

#include <iostream>
#include <stdexcept>

static void require(bool condition, const char *message) {
	if (!condition)
		throw std::logic_error(message);
}

template<class F> static void rejects(F operation) {
	try { operation(); }
	catch (const OVMPanic &) { return; }
	throw std::logic_error("Invalid hierarchy operation was accepted.");
}

int main() {
	for (unsigned opcode = 0x96; opcode <= 0x9b; ++opcode)
		require(vm_instruction_operand_count(static_cast<std::uint8_t>(opcode)) < 0,
			"Removed collection opcode is still accepted.");
	VMHierarchy hierarchy;
	VMHierarchyObject object;
	object.object_id = 7;
	object.data.values = {42};
	const unsigned a = hierarchy.store(object);
	const unsigned b = hierarchy.store(object);
	const unsigned c = hierarchy.store(object);
	require(hierarchy.at(0).children == std::vector<unsigned>{a, b, c}, "Root insertion order.");
	hierarchy.add_child(a, b);
	require(hierarchy.at(0).children == std::vector<unsigned>{a, c}, "Root transfer.");
	require(hierarchy.at(b).parent == a, "Parent link.");
	hierarchy.add_child(c, a);
	hierarchy.add_child(c, a);
	require(hierarchy.at(c).children == std::vector<unsigned>{a}, "Duplicate child.");
	require(hierarchy.at(a).children == std::vector<unsigned>{b}, "Subtree preservation.");
	hierarchy.add_child(c, b);
	require(hierarchy.at(c).children == std::vector<unsigned>{a, b}, "Reparent append order.");
	require(hierarchy.at(a).children.empty() && hierarchy.at(b).parent == c, "Old parent removal.");
	rejects([&] { hierarchy.add_child(b, c); });
	rejects([&] { hierarchy.add_child(a, a); });
	rejects([&] { hierarchy.add_child(0, b); });
	rejects([&] { hierarchy.add_child(a, 0); });
	rejects([&] { hierarchy.add_child(a, 10000); });
	require(hierarchy.at(c).children == std::vector<unsigned>{a, b}, "Failure changed hierarchy.");
	VMHierarchyObject *stable = &hierarchy.at(a);
	for (unsigned i = 0; i < 1024; ++i)
		hierarchy.store(object);
	require(&hierarchy.at(a) == stable && stable->object_id == 7 && stable->data.values[0] == 42,
		"Growing hierarchy moved an object or changed its payload.");
	require(hierarchy.at(0).children.size() == 1025 && hierarchy.size() == 1028,
		"Stored objects were lost.");
	hierarchy.reset();
	require(hierarchy.size() == 1 && hierarchy.at(0).children.empty(), "Reset root.");
	std::cout << "PASS: hierarchy roots, child order, reparenting, uniqueness, cycles, stable objects and reset\n";
}
