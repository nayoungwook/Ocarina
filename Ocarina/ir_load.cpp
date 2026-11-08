#include "ir_load.h"
#include "vm.h"
#include "vm_numeric.h"

#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>
#include <utility>

IRLoader::IRLoader(std::vector<byte> input_bytes, std::string source_name)
	: bytes(std::move(input_bytes)), source(std::move(source_name)) {
}

void IRLoader::reader_error(const std::string &message) const {
	throw std::runtime_error(source + ": IR load error at byte " + std::to_string(reader_cnt) +
							 ": " + message);
}

bool IRLoader::has_bytes(std::size_t count) const {
	return reader_cnt <= bytes.size() && count <= bytes.size() - reader_cnt;
}

byte IRLoader::peek_byte() const {
	if (!has_bytes(1))
		reader_error("unexpected end of IR");

	return bytes[reader_cnt];
}

byte IRLoader::read_byte() {
	byte value = peek_byte();
	++reader_cnt;

	return value;
}

void IRLoader::expect_byte(byte expected) {
	if (peek_byte() != expected)
		reader_error("expected section/block marker " + std::to_string(expected));

	read_byte();
}

std::uint32_t IRLoader::read_u32() {
	if (!has_bytes(4))
		reader_error("truncated 32-bit operand");

	std::uint32_t result = 0;

	for (unsigned i = 0; i < 4; ++i)
		result |= static_cast<std::uint32_t>(read_byte()) << (i * 8);

	return result;
}

std::uint64_t IRLoader::read_u64() {
	if (!has_bytes(8))
		reader_error("truncated 64-bit operand");

	std::uint64_t result = 0;

	for (unsigned i = 0; i < 8; ++i)
		result |= static_cast<std::uint64_t>(read_byte()) << (i * 8);

	return result;
}

std::int32_t IRLoader::read_i32() {
	return std::bit_cast<std::int32_t>(read_u32());
}

unsigned IRLoader::read_id() {
	std::int32_t id = read_i32();

	if (id < 0)
		reader_error("negative id, size or count");

	return static_cast<unsigned>(id);
}

std::string IRLoader::read_string() {
	auto begin = bytes.begin() + reader_cnt;
	auto end = std::find(begin, bytes.end(), 0);

	if (end == bytes.end())
		reader_error("unterminated string");

	std::string value(begin, end);
	reader_cnt += value.size() + 1;

	return value;
}

unsigned IRLoader::relocated_object_id(unsigned local_id) const {
	for (const ObjectRelocation &entry : objects)
		if (entry.local_id == local_id)
			return entry.runtime_id;

	return 0;
}

unsigned
IRLoader::object_runtime_id(const CVM &vm, unsigned local_id, const std::string &name) const {
	for (const auto &[id, data] : vm.object_data)
		if (data.name == name)
			return id;

	if (!vm.object_data.contains(local_id))
		return local_id;

	unsigned next_id = vm.object_data.rbegin()->first;

	if (next_id >= static_cast<unsigned>(INT32_MAX))
		reader_error("object id overflow");

	return next_id + 1;
}

void IRLoader::relocate_parent_objects(CVM &vm) {
	for (const ObjectRelocation &entry : objects) {
		unsigned parent_id = relocated_object_id(entry.parent_local_id);

		if (entry.parent_local_id != 0 && parent_id == 0)
			reader_error("unresolved parent object id");

		VMObjectData &data = vm.object_data.at(entry.runtime_id);

		if (data.parent_resolved && data.parent_id != parent_id)
			reader_error("conflicting object parent");

		data.parent_id = parent_id;
		data.parent_resolved = true;
	}
}

void IRLoader::read_function_metadata(CVM &vm, unsigned owner, bool is_constructor) {
	VMFunctionData data;
	data.id = read_id();
	data.name = read_string();

	if (!is_constructor)
		data.return_type = read_string();

	data.is_constructor = is_constructor;
	data.argument_count = read_id();
	if (data.argument_count > bytes.size() - reader_cnt)
		reader_error("invalid argument count");

	for (unsigned i = 0; i < data.argument_count; ++i)
		data.argument_types.push_back(read_string());

	data.stack_size = read_id();
	const unsigned local_count = read_id();
	if (local_count > (bytes.size() - reader_cnt) / 5)
		reader_error("invalid local variable count");

	unsigned end = 0;
	for (unsigned i = 0; i < local_count; ++i) {
		const unsigned offset = read_id();
		const std::string type_name = read_string();
		const VMOPType type = vm_operand_type(type_name);
		const unsigned size = get_operand_size(type);
		if (type_name.empty() || size == 0 || offset < end || offset > data.stack_size ||
			size > data.stack_size - offset || !data.local_types.emplace(offset, type).second)
			reader_error("invalid local variable layout");
		end = offset + size;
	}

	unsigned offset = 0;
	for (const std::string &type_name : data.argument_types) {
		const VMOPType type = vm_operand_type(type_name);
		if (type_name.empty() || !data.local_types.contains(offset) ||
			data.local_types.at(offset) != type)
			reader_error("argument layout does not match local variables");
		const unsigned size = get_operand_size(type);
		data.argument_sizes.push_back(size);
		offset += size;
	}

	if (!functions.emplace(owner, data.id).second)
		reader_error("duplicate function metadata");

	auto &table = owner == 0 ? vm.function_data : vm.object_data.at(owner).function_data;
	auto found = table.find(data.id);

	if (found != table.end()) {
		const VMFunctionData &old = found->second;

		if (owner == 0 || old.name != data.name || old.return_type != data.return_type ||
			old.argument_types != data.argument_types || old.local_types != data.local_types ||
			old.stack_size != data.stack_size || old.is_constructor != is_constructor)
			reader_error("conflicting function metadata");
	} else
		table.emplace(data.id, data);

	if (owner == 0 && data.name == "main") {
		if (vm.main_func_id != std::numeric_limits<unsigned>::max())
			reader_error("multiple main functions");

		vm.main_func_id = data.id;
	}
}

void IRLoader::read_meta_block(CVM &vm, unsigned owner) {
	byte kind = read_byte();

	if (kind == META_FUNC || kind == META_CONSTRUCTOR) {
		read_function_metadata(vm, owner, kind == META_CONSTRUCTOR);

		return;
	}

	if (kind == META_OBJECT) {
		if (owner != 0)
			reader_error("nested object metadata");

		unsigned id = read_id();
		std::string name = read_string();
		const unsigned parent_id = read_id();
		const unsigned size = read_id();

		if (id == 0 || relocated_object_id(id) != 0)
			reader_error("invalid or duplicate object id");

		unsigned runtime_id = object_runtime_id(vm, id, name);

		for (const auto &entry : objects)
			if (entry.runtime_id == runtime_id)
				reader_error("duplicate object name");

		objects.push_back({id, runtime_id, parent_id});

		if (vm.object_data.contains(runtime_id)) {
			if (vm.object_data.at(runtime_id).size != size)
				reader_error("conflicting object size");
		} else {
			VMObjectData data;
			data.id = runtime_id;
			data.name = name;
			data.size = size;
			data.initializer.name = "<initializer>";
			vm.object_data.emplace(runtime_id, std::move(data));
		}

		while (peek_byte() != META_TERM)
			read_meta_block(vm, runtime_id);

		expect_byte(META_TERM);

		return;
	}

	if (kind == META_VAR) {
		VMVariableData data;
		data.id = read_id();
		data.name = read_string();
		data.type = read_string();
		data.operand_type = vm_operand_type(data.type);
		data.offset = read_id();
		data.size = read_id();
		if (data.type.empty() || data.size == 0 || data.size != get_operand_size(data.operand_type))
			reader_error("invalid variable metadata");

		if (owner != 0) {
			const unsigned size = vm.object_data.at(owner).size;

			if (data.offset > size || data.size > size - data.offset)
				reader_error("member exceeds object size");
		}

		if (!variables.emplace(owner, data.id).second)
			reader_error("duplicate variable metadata");

		auto &table = owner == 0 ? vm.variable_data : vm.object_data.at(owner).variable_data;
		auto found = table.find(data.id);

		if (found != table.end()) {
			const VMVariableData &old = found->second;

			if (owner == 0 || old.name != data.name || old.type != data.type ||
				old.offset != data.offset || old.size != data.size)
				reader_error("conflicting variable metadata");
		} else
			table.emplace(data.id, std::move(data));

		return;
	}

	reader_error("unknown metadata block");
}

void IRLoader::read_meta(CVM &vm) {
	expect_byte(META_BEGIN);

	while (peek_byte() != META_END)
		read_meta_block(vm, 0);

	expect_byte(META_END);
}

std::vector<VMInstruction> IRLoader::decode_function_instructions(bool terminated,
																  bool block_boundary) {
	std::map<unsigned, unsigned> label_targets;
	std::vector<VMInstruction> instructions;

	while (has_bytes(1)) {
		byte opcode = peek_byte();

		if (terminated && opcode == CODE_TERM)
			break;

		if (block_boundary && (opcode == CODE_FUNC || opcode == CODE_OBJECT || opcode == CODE_END))
			break;

		read_byte();

		if (instructions.size() >= static_cast<std::size_t>(INT32_MAX))
			reader_error("too many instructions");

		if (opcode == OP_LABEL) {
			unsigned label_id = read_id();

			if (label_id >= VM_LABEL_CAPACITY ||
				!label_targets.emplace(label_id, static_cast<unsigned>(instructions.size())).second)
				reader_error("invalid or duplicate label id");

			continue;
		}

		VMInstruction instruction;
		instruction.opcode = opcode;
		const int operand_count = vm_instruction_operand_count(opcode);

		if (operand_count < 0)
			reader_error("unknown instruction opcode " + std::to_string(opcode));

		if (opcode == OP_EXPR_OP) {
			instruction.operands.u8 = read_byte();

			if (instruction.operands.u8 < OP_ADD || instruction.operands.u8 > OP_MOD)
				reader_error("unknown expression opcode");
		} else if (opcode == OP_LDC_F4)
			instruction.operands.f32 = std::bit_cast<float>(read_u32());
		else if (opcode == OP_LDC_F8)
			instruction.operands.f64 = std::bit_cast<double>(read_u64());
		else {
			for (int i = 0; i < operand_count; ++i) {
				instruction.operands.i32[i] = read_i32();
				instruction.operands.u32[i] =
					std::bit_cast<std::uint32_t>(instruction.operands.i32[i]);
			}
		}

		if (opcode == OP_ADD_CHILD && instruction.operands.u32[0] > 1)
			reader_error("invalid add_child receiver");

		if (opcode == OP_CONVERT &&
			(instruction.operands.u32[0] < static_cast<unsigned>(VMOPType::OPRND_INT32) ||
			 instruction.operands.u32[0] > static_cast<unsigned>(VMOPType::OPRND_VEC)))
			reader_error("invalid conversion type");

		if (opcode == OP_NEW_OBJECT || opcode == OP_CALL_SUPER) {
			unsigned id = instruction.operands.u32[0];
			unsigned relocated = relocated_object_id(id);

			if (instruction.operands.i32[0] <= 0 || relocated == 0)
				reader_error("unresolved instruction object id");

			instruction.operands.i32[0] = static_cast<std::int32_t>(relocated);
			instruction.operands.u32[0] = relocated;
		}

		if (opcode == OP_LOAD_STR) {
			if (instruction.operands.i32[0] < 0 || string_base > static_cast<unsigned>(INT32_MAX) ||
				instruction.operands.u32[0] > static_cast<unsigned>(INT32_MAX) - string_base)
				reader_error("invalid string reference");

			instruction.operands.u32[0] += string_base;
			instruction.operands.i32[0] = static_cast<std::int32_t>(instruction.operands.u32[0]);
		}

		instructions.push_back(instruction);
	}

	for (VMInstruction &instruction : instructions) {
		if (instruction.opcode != OP_GOTO && instruction.opcode != OP_JE &&
			instruction.opcode != OP_JNE)
			continue;

		unsigned label_id = instruction.operands.u32[0];

		if (!label_targets.contains(label_id))
			reader_error("unresolved branch label");

		instruction.operands.u32[0] = label_targets.at(label_id);
		instruction.operands.i32[0] = static_cast<std::int32_t>(label_targets.at(label_id));
	}

	return instructions;
}

std::vector<VMInstruction> IRLoader::read_function_code() {
	auto instructions = decode_function_instructions(true);
	expect_byte(CODE_TERM);
	return instructions;
}

void IRLoader::read_function(CVM &vm, unsigned owner) {
	unsigned id = read_id();
	auto &table = owner == 0 ? vm.function_data : vm.object_data.at(owner).function_data;

	if (!table.contains(id))
		reader_error("function code has no matching metadata");

	if (!function_code.emplace(owner, id).second)
		reader_error("duplicate function code");

	VMFunctionData &data = table.at(id);
	data.instructions = read_function_code();
	data.code_loaded = true;
}

static void append_instructions(VMFunctionData &data, std::vector<VMInstruction> instructions) {
	const std::size_t base = data.instructions.size();

	if (instructions.size() > static_cast<std::size_t>(INT32_MAX) - base)
		throw std::runtime_error("Initializer is too large.");

	for (VMInstruction &instruction : instructions) {
		if (instruction.opcode == OP_GOTO || instruction.opcode == OP_JE ||
			instruction.opcode == OP_JNE) {
			instruction.operands.u32[0] += static_cast<unsigned>(base);
			instruction.operands.i32[0] = static_cast<std::int32_t>(instruction.operands.u32[0]);
		}

		data.instructions.push_back(instruction);
	}
	data.code_loaded = true;
}

void IRLoader::read_object(CVM &vm) {
	unsigned owner = relocated_object_id(read_id());

	if (owner == 0)
		reader_error("object code has no matching metadata");

	while (peek_byte() != CODE_TERM) {
		byte kind = peek_byte();

		if (kind == CODE_FUNC) {
			read_byte();
			read_function(vm, owner);
		} else if (kind != CODE_OBJECT && kind != CODE_END)
			append_instructions(vm.object_data.at(owner).initializer,
								decode_function_instructions(true, true));
		else
			reader_error("unknown object code block");
	}

	expect_byte(CODE_TERM);
}

void IRLoader::read_code(CVM &vm) {
	expect_byte(CODE_BEGIN);

	while (peek_byte() != CODE_END) {
		byte kind = peek_byte();

		if (kind == CODE_OBJECT) {
			read_byte();
			read_object(vm);
		} else if (kind == CODE_FUNC) {
			read_byte();
			read_function(vm, 0);
		} else
			append_instructions(vm.initializer, decode_function_instructions(false, true));
	}

	expect_byte(CODE_END);
}

static std::string decode_string_literal(const std::string &literal) {
	if (literal.size() < 2 || literal.front() != '"' || literal.back() != '"')
		throw std::runtime_error("Invalid string literal in rodata.");

	std::string value;
	for (std::size_t i = 1; i + 1 < literal.size(); ++i) {
		char ch = literal[i];
		if (ch == '\\') {
			if (++i + 1 >= literal.size())
				throw std::runtime_error("Invalid string literal in rodata.");
			switch (literal[i]) {
			case 'n':
				ch = '\n';
				break;
			case 'r':
				ch = '\r';
				break;
			case 't':
				ch = '\t';
				break;
			case '0':
				ch = '\0';
				break;
			case '\\':
				ch = '\\';
				break;
			case '\'':
				ch = '\'';
				break;
			case '"':
				ch = '"';
				break;
			default:
				throw std::runtime_error("Invalid string escape in rodata.");
			}
		}
		value += ch;
	}
	return value;
}

void IRLoader::read_rodata(CVM &vm) {
	expect_byte(RODATA_BEGIN);
	std::set<unsigned> ids;

	while (peek_byte() != RODATA_END) {
		expect_byte(RODATA_STR);
		unsigned id = read_id();

		if (!ids.insert(id).second || string_base > static_cast<unsigned>(INT32_MAX) ||
			id > static_cast<unsigned>(INT32_MAX) - string_base)
			reader_error("invalid string id");

		std::string value = read_string();
		value = decode_string_literal(value);
		id += string_base;

		if (!vm.vm_string_pool.str_pool.emplace(id, std::move(value)).second)
			reader_error("duplicate string id");

		vm.vm_string_pool.size = std::max(vm.vm_string_pool.size, id + 1);
	}

	expect_byte(RODATA_END);

	if (reader_cnt != bytes.size())
		reader_error("unexpected data after rodata");
}

static bool valid_strings(const VMFunctionData &data, const VMStringPool &pool) {
	for (const VMInstruction &instruction : data.instructions)
		if (instruction.opcode == OP_LOAD_STR &&
			!pool.str_pool.contains(instruction.operands.u32[0]))
			return false;

	return true;
}

void IRLoader::validate_strings(const CVM &vm) const {
	if (!valid_strings(vm.initializer, vm.vm_string_pool))
		reader_error("undefined string id");

	for (const auto &[id, data] : vm.function_data)
		if (!valid_strings(data, vm.vm_string_pool))
			reader_error("undefined string id");

	for (const auto &[id, object] : vm.object_data) {
		if (!valid_strings(object.initializer, vm.vm_string_pool))
			reader_error("undefined string id");

		for (const auto &[function_id, data] : object.function_data)
			if (!valid_strings(data, vm.vm_string_pool))
				reader_error("undefined string id");
	}
}

void IRLoader::read_ir_files(CVM &vm, std::vector<IRLoader> &readers) {
	CVM loaded;
	loaded.initializer.name = "<global initializer>";

	for (IRLoader &reader : readers) {

		reader.reader_cnt = 0;
		reader.objects.clear();
		reader.functions.clear();
		reader.variables.clear();
		reader.function_code.clear();
		reader.read_meta(loaded);
	}

	for (IRLoader &reader : readers)
		reader.relocate_parent_objects(loaded);

	for (IRLoader &reader : readers) {
		reader.string_base = loaded.vm_string_pool.size;
		reader.read_code(loaded);
		reader.read_rodata(loaded);
		reader.validate_strings(loaded);
	}

	vm.object_data = std::move(loaded.object_data);
	vm.function_data = std::move(loaded.function_data);
	vm.variable_data = std::move(loaded.variable_data);
	vm.initializer = std::move(loaded.initializer);
	vm.vm_string_pool = std::move(loaded.vm_string_pool);
	vm.main_func_id = loaded.main_func_id;
}
