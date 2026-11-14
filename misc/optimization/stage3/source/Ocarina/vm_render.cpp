#include "vm.h"

void OVM::render_texture(const VMOperand &texture, const Vector &position,
	float width, float height, float rotation) {
	if (texture.op_type == VMOPType::OPRND_NULL || width == 0.0f || height == 0.0f)
		return;
	if (texture.op_type != VMOPType::OPRND_TEXTURE)
		throw OVMPanic("Rendering requires a texture value.");
	const std::string &path = vm_string_pool.str_pool.at(static_cast<unsigned>(texture.val.i32));
	const auto found = texture_pool.find(path);
	if (found == texture_pool.end() || found->second == nullptr)
		throw OVMPanic("Texture is not loaded: " + path);
	if (engine.get_renderer() == nullptr || engine.get_current_scene() == nullptr)
		throw OVMPanic("Rendering requires an open window.");
	engine.get_renderer()->render_image(found->second, position, width, height, rotation);
}

void OVM::exec_render(const VMInstruction &instruction, unsigned hierarchy_index) {
	if (hierarchy_index == 0 || hierarchy_index >= hierarchy.size())
		throw OVMPanic("OP_RENDER requires an object receiver.");
	const VMHierarchyObject &object = hierarchy.at(hierarchy_index);
	if (object_data.at(object.object_id).is_scene)
		throw OVMPanic("OP_RENDER cannot render a scene.");
	const auto &offsets = instruction.operands.u32;
	const VMOperand texture = object.data.load(offsets[0], 8);
	if (texture.op_type == VMOPType::OPRND_NULL)
		return;
	const VMOperand position = object.data.load(offsets[1], 12);
	const float width = object.data.load(offsets[2], 4).val.f32;
	const float height = object.data.load(offsets[3], 4).val.f32;
	const float rotation = object.data.load(offsets[4], 4).val.f32;
	render_texture(texture, Vector(position.val.v[0], position.val.v[1], position.val.v[2]),
		width, height, rotation);
}
