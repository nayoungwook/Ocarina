#include "vm_memory.h"

#include <algorithm>
#include <limits>
#include "ovm_panic.h"

void VMHierarchy::reset() {
	objects.clear();
	objects.emplace_back();
}

unsigned VMHierarchy::store(VMHierarchyObject object) {
	if (objects.size() >= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
		throw OVMPanic("Too many hierarchy objects.");

	const unsigned index = static_cast<unsigned>(objects.size());

	auto &roots = objects.front().children;
	if (roots.size() == roots.capacity())
		roots.reserve(roots.capacity() == 0 ? 8 : roots.capacity() * 2);

	object.parent = 0;
	objects.push_back(std::move(object));
	objects.front().children.push_back(index);
	return index;
}

void VMHierarchy::add_child(unsigned parent, unsigned child) {
	if (parent == 0 || child == 0 || parent >= objects.size() || child >= objects.size())
		throw OVMPanic("Invalid or null object reference.");
	for (unsigned ancestor = parent; ancestor != 0; ancestor = objects[ancestor].parent)
		if (ancestor == child)
			throw OVMPanic("add_child would create a hierarchy cycle.");
	VMHierarchyObject &node = objects[child];
	if (node.parent == parent)
		return;
	objects[parent].children.push_back(child);
	auto &siblings = objects[node.parent].children;
	siblings.erase(std::find(siblings.begin(), siblings.end(), child));
	node.parent = parent;
}

unsigned VMHierarchy::get_child(unsigned parent, std::int32_t index) const {
	if (parent == 0 || parent >= objects.size())
		throw OVMPanic("Invalid or null object reference.");
	const auto &children = objects[parent].children;
	if (index < 0 || static_cast<std::size_t>(index) >= children.size())
		throw OVMPanic("Child index out of range.");
	return children[static_cast<std::size_t>(index)];
}

unsigned VMHierarchy::get_child_size(unsigned parent) const {
	if (parent == 0 || parent >= objects.size())
		throw OVMPanic("Invalid or null object reference.");
	const auto &children = objects[parent].children;

	return children.size();
}
