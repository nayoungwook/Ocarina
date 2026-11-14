#pragma once

#include "code_data.h"
#include "ir.h"

#include <set>

class OVM;

class IRLoader {
public:
	IRLoader(std::vector<byte> input_bytes, std::string source_name = {});
	static std::vector<IRLoader> read_files(const std::unordered_map<std::string, std::vector<byte>> &files);
	static void read_ir_files(OVM &vm, std::vector<IRLoader> &readers);
	const std::string &get_source() const { return source; }
	const std::vector<byte> &get_bytes() const { return bytes; }
	std::size_t reader_cnt = 0;

private:
	std::vector<byte> bytes;
	std::string source;
	std::set<unsigned> objects;
	std::set<std::pair<unsigned, unsigned>> functions, variables, function_code;
	std::vector<std::string> read_imports();
	static void read_file_tree(const std::string &path,
		const std::map<std::string, const std::vector<byte> *> &files,
		std::set<std::string> &seen, std::vector<IRLoader> &readers);

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
	void read_function_metadata(OVM &vm, unsigned owner, bool is_constructor);
	void read_meta_block(OVM &vm, unsigned owner);
	void read_meta(OVM &vm);
	std::vector<VMInstruction> decode_function_instructions(const OVM &vm, bool terminated,
		bool block_boundary = false);
	std::vector<VMInstruction> read_function_code(const OVM &vm);
	void read_function(OVM &vm, unsigned owner);
	void specialize_storage(OVM &vm, VMFunctionData &function, unsigned owner);
	void read_object(OVM &vm);
	void read_code(OVM &vm);
	void read_rodata(OVM &vm);
	void validate_strings(const OVM &vm) const;
};
