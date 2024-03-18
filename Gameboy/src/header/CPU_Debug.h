#pragma once

class CPU;
class Cartridge;

class CPU_DEBUG
{
public:
	struct Opcodes
	{
		int operands;
		const char* name;
	};

	static uint64_t s_cycles;

	static std::string GetDebugInfo(const CPU* sm83);
	static std::string GetDebugInfo(const Cartridge* cart);

	static void ImGui_PPU_Registers(const CPU* sm83);
	static void ImGui_Registers(const CPU* sm83);

private:
	static std::string GetOpcodeDebugInfo(const CPU* sm83);
};