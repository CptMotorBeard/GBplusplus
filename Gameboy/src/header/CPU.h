#pragma once

#include "Joypad.h"
#include "PPU.h"

class Cartridge;
class PPU;

struct CPU_Register
{
	union
	{
		struct
		{
			BYTE lo;
			BYTE hi;
		};
		WORD pair;
	};

	CPU_Register(WORD initialValue);
	CPU_Register(BYTE initialLow, BYTE initialHigh);

	void operator= (WORD value) { pair = value; }
	operator WORD& () { return pair; }
};

struct DMATransfer
{
	DMATransfer()
	{
		from = 0;
		currentIndex = 0;
		active = false;
	}

	WORD from;
	WORD currentIndex;
	bool active;
};

class CPU
{
public:
	CPU(sf::RenderTarget* screen);
	~CPU();

	void AddCartridge(Cartridge* cart);
	void PowerOn();
	void WriteJoypad(const Joypad& joypad);
	void CPU_Step();

	void DumpGPU(sf::RenderTarget& renderWindow);

	static constexpr BYTE INTERRUPT_VBLANK = BIT_0;
	static constexpr BYTE INTERRUPT_LCD = BIT_1;
	static constexpr BYTE INTERRUPT_TIMER = BIT_2;
	static constexpr BYTE INTERRUPT_SERIAL = BIT_3;
	static constexpr BYTE INTERRUPT_JOYPAD = BIT_4;

	bool IsInterruptEnabled(BYTE interrupt) const;
	void RequestInterrupt(BYTE interrupt);
	void ServiceInterrupts();

	BYTE Read(WORD address) const;

	inline const bool IsRunning() { return m_isRunning; }
	inline unsigned long GetTotalClockCycles() { return m_totalClockCycles; }
	inline void ResetTotalClockCycles() { m_totalClockCycles = 0; }
	inline void Draw() { m_ppu.DrawToScreen(); }

private:
	bool m_isRunning;
	PPU m_ppu;

	///////////////// Registers /////////////////
private:
	static constexpr BYTE FLAG_Z = (1 << 7);	// Zero flag
	static constexpr BYTE FLAG_N = (1 << 6);	// BCD, Negative flag
	static constexpr BYTE FLAG_H = (1 << 5);	// BCD, half carry flag
	static constexpr BYTE FLAG_C = (1 << 4);	// carry flag

	inline void SetFlagIf(BYTE flag, bool condition);
	inline void SetZFlag(BYTE valueToCheck);
	inline void SetFlag(BYTE flag);
	inline void ClearFlag(BYTE flag);
	inline void FlipFlag(BYTE flag);
	inline bool IsFlagSet(BYTE flag) const
	{
		return (AF.lo & flag);
	};

	enum Registers
	{
		REGISTER_AF,
		REGISTER_BC,
		REGISTER_DE,
		REGISTER_HL,
		REGISTER_SP,
		REGISTER_PC,
		REGISTER_COUNT
	};

	union
	{
		CPU_Register registers[REGISTER_COUNT];

		struct
		{
			CPU_Register AF;		// Register AF, accumulator & flags
			CPU_Register BC;		// Register BC
			CPU_Register DE;		// Register DE
			CPU_Register HL;		// Register HL

			CPU_Register SP;		// Stack Pointer
			CPU_Register PC;		// Program Counter
		};
	};	

#define REGISTER_A AF.hi
#define REGISTER_B BC.hi
#define REGISTER_C BC.lo
#define REGISTER_D DE.hi
#define REGISTER_E DE.lo
#define REGISTER_H HL.hi
#define REGISTER_L HL.lo

	BYTE GetValueBasedOnOpCode(BYTE opcode);
	void SetValueBasedOnOpCode(BYTE opcode, BYTE value);

	///////////////// CPU Clock /////////////////

	void SpinCycle(int numMachineCycles = 1);
	void FlushClockCycles();

	int m_clockCycles;
	unsigned int m_totalClockCycles;

	DMATransfer m_dmaTransferProgress;

	///////////////// Timers /////////////////
private:
	static constexpr WORD DIVIDER_REGISTER = 0x0004;
	static constexpr WORD TIMER_COUNTER = 0x0005;
	static constexpr WORD TIMER_MODULO = 0x0006;
	static constexpr WORD TIMER_CONTROL = 0x0007;

	static constexpr int CLOCKSPEED  = 4194304;

	static constexpr int FREQUENCY_0 = 4096;
	static constexpr int FREQUENCY_1 = 262144;
	static constexpr int FREQUENCY_2 = 65536;
	static constexpr int FREQUENCY_3 = 16382;

	void TimerStep(int cycles);
	BYTE GetFrequency() const;
	void SetFrequency();

	BYTE m_dividerCounter;
	int m_timerCounter;

	///////////////// Interrupts /////////////////
private:
	bool m_interruptMasterEnableFlag;
	BYTE m_interruptMasterTimer;

	///////////////// Memory /////////////////
private:
	// The gameboy memory map looks like :
	// 0000		3FFF	16 KiB ROM bank 00	From cartridge, usually a fixed bank
	// 4000		7FFF	16 KiB ROM Bank 01–NN	From cartridge, switchable bank via mapper(if any)
	// 8000		9FFF	8 KiB Video RAM(VRAM)	In CGB mode, switchable bank 0 / 1
	// A000		BFFF	8 KiB External RAM	From cartridge, switchable bank if any
	// C000		CFFF	4 KiB Work RAM(WRAM)
	// D000		DFFF	4 KiB Work RAM(WRAM)	In CGB mode, switchable bank 1–7
	// E000		FDFF	Echo RAM(mirror of C000–DDFF)	Nintendo says use of this area is prohibited.
	// FE00		FE9F	Object attribute memory(OAM)
	// FEA0		FEFF	Not Usable	Nintendo says use of this area is prohibited.
	// FF00		FF7F	I / O Registers
	// FF80		FFFE	High RAM(HRAM)
	// FFFF		FFFF	Interrupt Enable register (IE)

	// The CPU has 8 KiB of workable RAM
	static constexpr int CPU_RAM = 0x2000;
	// 160 bytes of object attribute memory
	static constexpr int OAM = 0xA0;
	// 128 bytes for I / O registers
	static constexpr int IO = 0x80;
	// 127 bytes for HRAM
	static constexpr int HRAM = 0x7F;

	Cartridge* m_cartridge;
	BYTE CycleRead(WORD address);
	BYTE CycleRead_PC();
	inline WORD CycleReadWord_PC();
	void Write(WORD address, BYTE data);
	void CycleWrite(WORD address, BYTE data);

	void PushStack(WORD data);
	WORD PopStack();

	BYTE* m_internalRAM;
	BYTE* m_oam;
	BYTE* m_io;
	BYTE* m_hram;
	BYTE m_interruptEnableRegister;

	bool m_isStopped;
	bool m_isHalted;

	///////////////// Opcode Helpers /////////////////
private:
	void ADD(WORD value, BYTE carry = 0);
	void ADC(WORD value);
	void AND(BYTE value);
	void CP(BYTE value);
	void DEC(BYTE& value);
	void INC(BYTE& value);
	void OR(BYTE value);
	void SBC(WORD value);
	void SUB(WORD value, BYTE carry = 0);
	void XOR(BYTE value);

	void ADD_16(WORD value);

	///////////////// Opcodes /////////////////
private:
	// Information and grouping from here:
	// https://rgbds.gbdev.io/docs/v0.7.0/gbz80.7

	typedef void (CPU::* func_opcode)(BYTE);
	func_opcode m_opcodes[256];
	void SetupOpcodes();

#pragma region 8-bit Arithmetic and Logic Instructions

	void ADC_R	(BYTE opcode);
	void ADC_n8	(BYTE opcode);
	void ADD_R	(BYTE opcode);
	void ADD_n8	(BYTE opcode);
	void AND_R	(BYTE opcode);
	void AND_n8	(BYTE opcode);
	void CP_R	(BYTE opcode);
	void CP_n8	(BYTE opcode);
	void DEC_R	(BYTE opcode);
	void INC_R	(BYTE opcode);
	
	void OR_R	(BYTE opcode);
	void OR_n8	(BYTE opcode);

	void SBC_R	(BYTE opcode);
	void SBC_n8	(BYTE opcode);

	void SUB_R	(BYTE opcode);
	void SUB_n8	(BYTE opcode);

	void XOR_R	(BYTE opcode);	
	void XOR_n8	(BYTE opcode);

#pragma endregion

#pragma region 16-bit Arithmetic Instructions

	void ADD_HL_RR	(BYTE opcode);
	void DEC_RR		(BYTE opcode);
	void INC_RR		(BYTE opcode);

#pragma endregion

#pragma region Bit and Shift Operations

	void BIT_OP	(BYTE opcode);
	void RES	(BYTE opcode);
	void SET	(BYTE opcode);
	void SWAP	(BYTE opcode);
	void RL		(BYTE opcode);
	void RLC	(BYTE opcode);
	void RR		(BYTE opcode);
	void RRC	(BYTE opcode);
	void SLA	(BYTE opcode);
	void SRA	(BYTE opcode);
	void SRL	(BYTE opcode);

	void RLA	(BYTE opcode);
	void RLCA	(BYTE opcode);
	void RRA	(BYTE opcode);
	void RRCA	(BYTE opcode);

#pragma endregion

#pragma region Load Instructions

	void LD_R_R		(BYTE opcode);
	void LD_R_n8	(BYTE opcode);

	void LD_RR_n16	(BYTE opcode);

	void LD_BC_A	(BYTE opcode);
	void LD_DE_A	(BYTE opcode);
	void LD_A_BC	(BYTE opcode);
	void LD_A_DE	(BYTE opcode);
	void LDI_HL_A	(BYTE opcode);
	void LDD_HL_A	(BYTE opcode);
	void LDI_A_HL	(BYTE opcode);
	void LDD_A_HL	(BYTE opcode);

	void LD_n16_A		(BYTE opcode);
	void LD_FF00_n8_A	(BYTE opcode);
	void LD_FF00_C_A	(BYTE opcode);
	void LD_A_n16		(BYTE opcode);
	void LD_A_FF00_n8	(BYTE opcode);
	void LD_A_FF00_C	(BYTE opcode);

#pragma endregion

#pragma region Jumps and Subroutines

	bool CheckCondition(BYTE opcode);

	void JP		(BYTE opcode);
	void JP_HL	(BYTE opcode);
	void JP_cc	(BYTE opcode);
	void JR		(BYTE opcode);
	void JR_cc	(BYTE opcode);
	void CALL	(BYTE opcode);
	void CALL_cc(BYTE opcode);
	void RET	(BYTE opcode);
	void RET_cc	(BYTE opcode);
	void RETI	(BYTE opcode);
	void RST_nn	(BYTE opcode);

#pragma endregion

#pragma region Stack Operations Instructions

	void LD_SP_HL	(BYTE opcode);
	void LD_n16_SP	(BYTE opcode);
	void LD_HL_SP_e	(BYTE opcode);
	void ADD_SP		(BYTE opcode);
	void PUSH_RR	(BYTE opcode);
	void POP_RR		(BYTE opcode);

#pragma endregion

#pragma region Miscellaneous Instructions

	void CCF	(BYTE opcode);
	void CPL	(BYTE opcode);
	void DAA	(BYTE opcode);
	void DI		(BYTE opcode);
	void EI		(BYTE opcode);
	void HALT	(BYTE opcode);
	void NOP	(BYTE opcode);
	void SCF	(BYTE opcode);
	void STOP	(BYTE opcode);
	void CB		(BYTE opcode);

#pragma endregion
};