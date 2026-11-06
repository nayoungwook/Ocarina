#pragma once

#include "code_data.h"
#include "ir.h"

#include <set>

class CVM;

struct ObjectRelocation {
	unsigned local_id = 0, runtime_id = 0, parent_local_id = 0;
};

class IRLoader {
public:
	IRLoader(std::vector<byte> input_bytes, std::string source_name = {});
	static void read_ir_files(CVM &vm, std::vector<IRLoader> &readers);
	std::size_t reader_cnt = 0;

private:
	std::vector<byte> bytes;
	std::string source;
	std::vector<ObjectRelocation> objects;
	unsigned string_base = 0;
	std::set<std::pair<unsigned, unsigned>> functions, variables, function_code;

	[[noreturn]] void reader_error(const std::string &message) const;
	bool has_bytes(std::size_t count) const;
	byte read_byte();
	byte peek_byte() const;
	void expect_byte(byte expected);
	std::uint32_t read_u32();
	std::uint64_t read_u64();
	std::int32_t read_i32();
	unsigned read_id();
	std::string read_string();
	unsigned relocated_object_id(unsigned local_id) const;
	unsigned object_runtime_id(const CVM &vm, unsigned local_id, const std::string &name) const;
	void relocate_parent_objects(CVM &vm);
	void read_function_metadata(CVM &vm, unsigned owner, bool is_constructor);
	void read_meta_block(CVM &vm, unsigned owner);
	void read_meta(CVM &vm);
	std::vector<VMInstruction> decode_function_instructions(bool terminated,
		bool block_boundary = false);
	std::vector<VMInstruction> read_function_code();
	void read_function(CVM &vm, unsigned owner);
	void read_object(CVM &vm);
	void read_code(CVM &vm);
	void read_rodata(CVM &vm);
	void validate_strings(const CVM &vm) const;
};
