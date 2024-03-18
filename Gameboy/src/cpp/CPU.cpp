#include "stdafx.h"

#include <iomanip>
#include <sstream>

#include "header/CPU.h"
#include "header/CPU_Debug.h"
#include "header/Cartridge.h"
#include "header/Debug.h"

CPU_Register::CPU_Register(WORD initialValue)
{
	pair = initialValue;
}

CPU_Register::CPU_Register(BYTE initialLow, BYTE initialHigh)
{
	lo = initialLow;
	hi = initialHigh;
}

/////////////////////////////////////////////////////////////////

CPU::CPU(Cartridge* cart) :
	PC(0x100),
	SP(0xFFFE),
	AF(0x01B0),
	BC(0x0013),
	DE(0x00D8),
	HL(0x014D),
	m_ppu(),
	m_clockCycles(0),
	m_dividerCounter(0),
	m_isHalted(false),
	m_isStopped(false),
	m_cartridge(cart),
	m_interruptEnableRegister(0x00),
	m_interruptMasterEnableFlag(false),
	m_interruptMasterTimer(0xFF)
{
	m_internalRAM = new BYTE[CPU_RAM];
	m_oam = new BYTE[OAM];
	m_io = new BYTE[IO];
	m_hram = new BYTE[HRAM];

	m_ppu.Initialize(this, m_io);
	m_timerCounter = CLOCKSPEED / FREQUENCY_0;

	Write(0xFF00, 0xCF);
	Write(0xFF05, 0x00);
	Write(0xFF06, 0x00);
	Write(0xFF07, 0x00);
	Write(0xFF10, 0x80);
	Write(0xFF11, 0xBF);
	Write(0xFF12, 0xF3);
	Write(0xFF14, 0xBF);
	Write(0xFF16, 0x3F);
	Write(0xFF17, 0x00);
	Write(0xFF19, 0xBF);
	Write(0xFF1A, 0x7F);
	Write(0xFF1B, 0xFF);
	Write(0xFF1C, 0x9F);
	Write(0xFF1E, 0xBF);
	Write(0xFF20, 0xFF);
	Write(0xFF21, 0x00);
	Write(0xFF22, 0x00);
	Write(0xFF23, 0xBF);
	Write(0xFF24, 0x77);
	Write(0xFF25, 0xF3);
	Write(0xFF26, 0xF1);
	Write(0xFF40, 0x91);
	Write(0xFF42, 0x00);
	Write(0xFF43, 0x00);
	Write(0xFF45, 0x00);
	Write(0xFF47, 0xFC);
	Write(0xFF48, 0xFF);
	Write(0xFF49, 0xFF);
	Write(0xFF4A, 0x00);
	Write(0xFF4B, 0x00);

	SetupOpcodes();
}

CPU::~CPU()
{
	delete[] m_internalRAM;
	delete[] m_oam;
	delete[] m_io;
	delete[] m_hram;
}

void CPU::WriteJoypad(const Joypad& joypad)
{
	// Joypad address is 0xFF00, but that is 0x0000 since memory is split up
	const BYTE joypadRegisterAddress = 0x0000;
	BYTE oldJoypadRegister = m_io[joypadRegisterAddress];
	BYTE joypadRegister = oldJoypadRegister;

	// Note that, rather unconventionally for the Game Boy, a button being pressed is seen as the corresponding bit being 0, not 1.
	// If bit 5 is 0, the lower nibble contains start/select/b/a
	// if bit 4 is 0, the lower nibble contains down/up/left/right

	if (!(joypadRegister & BIT_5))
	{
		joypadRegister = 0xD0
			| (joypad.a		 ? 0 : BIT_0)
			| (joypad.b		 ? 0 : BIT_1)
			| (joypad.select ? 0 : BIT_2)
			| (joypad.start	 ? 0 : BIT_3);
	}
	else if (!(joypadRegister & BIT_4))
	{
		joypadRegister = 0xE0
			| (joypad.right ? 0 : BIT_0)
			| (joypad.left	? 0 : BIT_1)
			| (joypad.up	? 0 : BIT_2)
			| (joypad.down	? 0 : BIT_3);
	}
	else
	{
		joypadRegister = 0xCF;		// reset the register
	}

	m_io[joypadRegisterAddress] = joypadRegister;

	if (((oldJoypadRegister & 0xFF) ^ (joypadRegister & 0xFF)) & (oldJoypadRegister & 0xFF))
	{
		// old joypad had a 1 that was swapped to a 0. This triggers a joypad interrupt
		RequestInterrupt(INTERRUPT_JOYPAD);
	}
}

void CPU::CPU_Step()
{
	BYTE instruction = CycleRead_PC();

	func_opcode opcode = m_opcodes[instruction];
	(this->*opcode)(instruction);

	if (m_interruptMasterTimer == 0x1)
	{
		m_interruptMasterEnableFlag = true;
		m_interruptMasterTimer = 0xFF;
	}
	else if (m_interruptMasterTimer == 0x0)
	{
		m_interruptMasterEnableFlag = false;
		m_interruptMasterTimer = 0xFF;
	}
}

inline void CPU::SetFlagIf(BYTE flag, bool condition)
{
	if (condition)
	{
		SetFlag(flag);
	}
	else
	{
		ClearFlag(flag);
	}
}

inline void CPU::SetZFlag(BYTE valueToCheck)
{
	if (valueToCheck == 0)
	{
		SetFlag(FLAG_Z);
	}
	else
	{
		ClearFlag(FLAG_Z);
	}
}

inline void CPU::SetFlag(BYTE flag)
{
	AF.lo |= flag;
}

inline void CPU::ClearFlag(BYTE flag)
{
	AF.lo &= (~flag);
}

inline void CPU::FlipFlag(BYTE flag)
{
	AF.lo ^= flag;
}

BYTE CPU::GetValueBasedOnOpCode(BYTE opcode)
{
	BYTE sourceRegister = ((opcode >> 1) + 1) & 3;
	BYTE lo = opcode & 1;
	if (sourceRegister == REGISTER_AF)
	{
		if (lo)
		{
			return REGISTER_A;
		}
		return CycleRead(HL);
	}

	if (lo)
	{
		return registers[sourceRegister].lo;
	}

	return registers[sourceRegister].hi;
}

void CPU::SetValueBasedOnOpCode(BYTE opcode, BYTE value)
{
	BYTE sourceRegister = ((opcode >> 1) + 1) & 3;
	BYTE lo = opcode & 1;

	if (sourceRegister == REGISTER_AF)
	{
		if (lo)
		{
			REGISTER_A = value;
			return;
		}

		CycleWrite(HL, value);
		return;
	}

	if (lo)
	{
		registers[sourceRegister].lo = value;
		return;
	}

	registers[sourceRegister].hi = value;
}

void CPU::SpinCycle(int numMachineCycles)
{
	m_clockCycles += 4 * numMachineCycles;
}

void CPU::FlushClockCycles()
{
	CPU_DEBUG::s_cycles += m_clockCycles;
	if (m_clockCycles)
	{
		TimerStep(m_clockCycles);
		m_ppu.Step(m_clockCycles);
	}

	m_clockCycles = 0;
}

void CPU::TimerStep(int cycles)
{
	// The divider counter is incremented at a rate of 16384 Hz
	// The CPU has a clockspeed of 4194304 Hz
	// Every 256 cycles, the divider counter is incremented
	WORD increment = m_dividerCounter + cycles;
	if (increment > 255)
	{
		++m_io[DIVIDER_REGISTER];
	}
	m_dividerCounter = (increment & 0xFF);

	// The divider register always increments, but the timer counter only increments if it is enabled
	// Bit 2 of the timer control register determines if the timer is enabled
	if (m_io[TIMER_CONTROL] & BIT_2)
	{
		m_timerCounter -= cycles;
		if (m_timerCounter <= 0)
		{
			SetFrequency();
			BYTE timaRegister = m_io[TIMER_COUNTER];

			// When the TIMA register overflows, it is reset to the value specified by the TMA register
			// An interrupt is then requested
			if (timaRegister == 0xFF)
			{
				timaRegister = m_io[TIMER_MODULO];
				RequestInterrupt(INTERRUPT_TIMER);
			}
			else
			{
				++timaRegister;
			}

			m_io[TIMER_COUNTER] = timaRegister;
		}
	}
}

BYTE CPU::GetFrequency() const
{
	BYTE frequency = m_io[TIMER_CONTROL];

	// Bits 0 and Bits 1 set the value for the frequency.
	// Bit 2 is the enable bit

	return (frequency & (BIT_0 | BIT_1));
}

void CPU::SetFrequency()
{
	BYTE f = GetFrequency();

	int frequency;
	switch (f)
	{
	case 0:
		frequency = FREQUENCY_0;
		break;
	case 1:
		frequency = FREQUENCY_1;
		break;
	case 2:
		frequency = FREQUENCY_2;
		break;
	case 3:
		frequency = FREQUENCY_3;
		break;
	default:
		frequency = FREQUENCY_0;
		break;
	}
	m_timerCounter = CLOCKSPEED / frequency;
}

bool CPU::IsInterruptEnabled(BYTE interrupt) const
{
	return m_interruptEnableRegister & interrupt;
}

void CPU::RequestInterrupt(BYTE interrupt)
{
	BYTE interruptFlag = m_io[0x0F];
	interruptFlag |= interrupt;
	m_io[0x0F] = interruptFlag;
}

void CPU::ServiceInterrupts()
{
	BYTE interruptFlag = m_io[0x0F];
	BYTE interruptsToProcess = interruptFlag & m_interruptEnableRegister;
	if (interruptsToProcess)
	{
		m_isHalted = false;
	}

	if (m_interruptMasterEnableFlag)
	{
		if (interruptsToProcess)
		{
			m_interruptMasterEnableFlag = false;

			// 2 NOPS for 8 cycles
			// PC register is pushed to the stack for another 8
			// 4 final cycles to update the PC register
			m_clockCycles += 20;
			PushStack(PC);
		}

		if (interruptsToProcess & INTERRUPT_VBLANK)
		{
			PC = 0x0040;
			// TODO: should also tell the GPU to draw the screen
		}
		else if (interruptsToProcess & INTERRUPT_LCD)
		{
			PC = 0x0048;
		}
		else if (interruptsToProcess & INTERRUPT_TIMER)
		{
			PC = 0x0050;
		}
		else if (interruptsToProcess & INTERRUPT_SERIAL)
		{
			PC = 0x0058;
		}
		else if (interruptsToProcess & INTERRUPT_JOYPAD)
		{
			m_isStopped = false;
			PC = 0x0060;
		}
	}
	else
	{
		// TODO: halt bug
	}
}

BYTE CPU::Read(WORD address) const
{
	// Reading from one of the 16 KiB ROM banks from the cartridge
	if (address < 0x8000)
	{
		if (m_cartridge)
		{
			return m_cartridge->ReadMemory(address);
		}
		return 0x00;
	}

	// Reading from the 8 KiB Video RAM(VRAM)
	if (address < 0xA000)
	{
		// TODO
		return 0x00;
	}

	// Reading from one of the 8 KiB External RAM	banks from the cartridge
	if (address < 0xC000)
	{
		if (m_cartridge)
		{
			return m_cartridge->ReadMemory(address);
		}

		return 0x00;
	}

	// Reading from the 8 KiB of internal RAM
	if (address < 0xE000)
	{
		MEMORY_ADDRESS internalAddress = address - 0xC000;
		return m_internalRAM[internalAddress];
	}

	// This is a mirror of the internal RAM. Nintendo says use of this area is prohibited.
	if (address < 0xFE00)
	{
		MEMORY_ADDRESS internalAddress = address - 0xE000;
		return m_internalRAM[internalAddress];
	}

	// Reading from Object attribute memory
	if (address < 0xFEA0)
	{
		MEMORY_ADDRESS internalAddress = address - 0xFE00;
		return m_oam[internalAddress];
	}

	// Reading from an invalid part of memory. Nintendo says use of this area is prohibited.
	if (address < 0xFF00)
	{
		return 0x00;
	}

	// Reading from the I / O Registers
	if (address < 0xFF80)
	{
		//	$FF00			Joypad input
		//	$FF01	$FF02	Serial transfer
		//	$FF04	$FF07	Timer and divider
		//	$FF10	$FF26	Audio
		//	$FF30	$FF3F	Wave pattern
		//	$FF40	$FF4B	LCD Control, Status, Position, Scrolling, and Palettes
		//	$FF4F			VRAM Bank Select
		//	$FF50			Set to non - zero to disable boot ROM
		//	$FF51	$FF55	CGB	VRAM DMA
		//	$FF68	$FF6B	CGB	BG / OBJ Palettes
		//	$FF70			WRAM Bank Select

		MEMORY_ADDRESS internalAddress = address - 0xFF00;
		return m_io[internalAddress];
	}

	// Reading from HRAM
	if (address < 0xFFFF)
	{
		MEMORY_ADDRESS internalAddress = address - 0xFF80;
		return m_hram[internalAddress];
	}

	// Reading from the interrupt enable register
	if (address == 0xFFFF)
	{
		return m_interruptEnableRegister;
	}

	DEBUG_ASSERT(false, "Reading from invalid memory");
	return 0x00;
}

BYTE CPU::CycleRead(WORD address)
{
	if (m_clockCycles)
	{
		FlushClockCycles();
	}

	BYTE value = Read(address);
	m_clockCycles = 4;
	return value;
}

BYTE CPU::CycleRead_PC()
{
	return CycleRead(PC++);
}

inline WORD CPU::CycleReadWord_PC()
{
	BYTE lsb = CycleRead_PC();
	BYTE msb = CycleRead_PC();

	WORD value = (msb << 8) | lsb;
	return value;
}

void CPU::Write(WORD address, BYTE data)
{
	// Writing to one of the 16 KiB ROM banks from the cartridge
	if (address < 0x8000)
	{
		if (m_cartridge)
		{
			m_cartridge->WriteMemory(address, data);
		}
		return;
	}

	// Writing to the 8 KiB Video RAM(VRAM)
	if (address < 0xA000)
	{
		// TODO
		return;
	}

	// Writing to one of the 8 KiB External RAM	banks from the cartridge
	if (address < 0xC000)
	{
		if (m_cartridge)
		{
			m_cartridge->WriteMemory(address, data);
		}
		return;
	}

	// Writing to the 8 KiB of internal RAM
	if (address < 0xE000)
	{
		MEMORY_ADDRESS internalAddress = address - 0xC000;
		m_internalRAM[internalAddress] = data;
		return;
	}

	// This is a mirror of the internal RAM. Nintendo says use of this area is prohibited.
	if (address < 0xFE00)
	{
		MEMORY_ADDRESS internalAddress = address - 0xE000;
		m_internalRAM[internalAddress] = data;
		return;
	}

	// Writing to Object attribute memory
	if (address < 0xFEA0)
	{
		MEMORY_ADDRESS internalAddress = address - 0xFE00;
		m_oam[internalAddress] = data;
		return;
	}

	// Writing to an invalid part of memory. Nintendo says use of this area is prohibited.
	if (address < 0xFF00)
	{
		return;
	}

	// Writing to the I / O Registers
	if (address < 0xFF80)
	{
		//	$FF00			Joypad input
		//	$FF01	$FF02	Serial transfer
		//	$FF04	$FF07	Timer and divider
		//	$FF10	$FF26	Audio
		//	$FF30	$FF3F	Wave pattern
		//	$FF40	$FF4B	LCD Control, Status, Position, Scrolling, and Palettes
		//	$FF4F			VRAM Bank Select
		//	$FF50			Set to non - zero to disable boot ROM
		//	$FF51	$FF55	CGB	VRAM DMA
		//	$FF68	$FF6B	CGB	BG / OBJ Palettes
		//	$FF70			WRAM Bank Select

		MEMORY_ADDRESS internalAddress = address - 0xFF00;
		if (address == 0xFF02)
		{
			// A value of 0x81 indicates there is something to be transfered
			// For our purposes that means a character should be written to the console

			if (data == 0x81)
			{
				std::cout << static_cast<char>(m_io[0x01]);
			}
		}
		else if (address == 0xFF04)
		{
			// The divider register resets to 0 when written to
			m_io[internalAddress] = 0x00;
		}
		else
		{
			m_io[internalAddress] = data;
		}

		return;
	}

	// Writing to HRAM
	if (address < 0xFFFF)
	{
		MEMORY_ADDRESS internalAddress = address - 0xFF80;
		m_hram[internalAddress] = data;
		return;
	}

	// Writing to the interrupt enable register
	if (address == 0xFFFF)
	{
		m_interruptEnableRegister = data;
		return;
	}
}

void CPU::CycleWrite(WORD address, BYTE data)
{
	if (m_clockCycles)
	{
		FlushClockCycles();
	}

	Write(address, data);
	m_clockCycles = 4;
}

void CPU::PushStack(WORD data)
{
	BYTE msb = (data >> 8) & 0xFF;
	BYTE lsb = data & 0xFF;
	/*
		------------------
		SP		0000
		SP -1	msb
		SP -2	lsb		-- new SP
		------------------
	*/
	CycleWrite(--SP, msb);
	CycleWrite(--SP, lsb);
}

WORD CPU::PopStack()
{
	/*
	------------------
	SP +2	0000	-- new SP
	SP +1	msb
	SP		lsb
	------------------
	*/
	BYTE lsb = CycleRead(SP.pair++);
	BYTE msb = CycleRead(SP.pair++);

	WORD value = (msb << 8) | lsb;
	return value;
}

/////////////////////////////////////////////////////////////////

void CPU::ADD(WORD value)
{
	WORD result = REGISTER_A + value;

	// Carry flag is set if we overflow from bit 7
	const bool hasFullCarry = result & 0xFF00;
	SetFlagIf(FLAG_C, hasFullCarry);

	// Half carry flag is set if we overflow from bit 3
	BYTE halfAdd = (REGISTER_A & 0xF) + (value & 0xF);
	const bool hasHalfCarry = halfAdd & 0xF0;
	SetFlagIf(FLAG_H, hasHalfCarry);

	REGISTER_A = result & 0xFF;
	SetZFlag(REGISTER_A);

	// negative flag always cleared
	ClearFlag(FLAG_N);
}

void CPU::ADC(WORD value)
{
	ADD(value + IsFlagSet(FLAG_C));
}

void CPU::AND(BYTE value)
{
	REGISTER_A &= value;

	ClearFlag(FLAG_N);
	SetFlag(FLAG_H);
	ClearFlag(FLAG_C);
	SetZFlag(REGISTER_A);
}

void CPU::CP(BYTE value)
{
	const bool isZero = REGISTER_A == value;
	SetFlagIf(FLAG_Z, isZero);

	const bool isCarry = value > REGISTER_A;
	SetFlagIf(FLAG_C, isCarry);

	const bool halfCarry = ((REGISTER_A & 0xF) - (value & 0xF)) & 0x10;
	SetFlagIf(FLAG_H, halfCarry);

	SetFlag(FLAG_N);
}

void CPU::DEC(BYTE& value)
{
	const bool halfCarry = ((value & 0xF) - 1) & 0x10;
	SetFlagIf(FLAG_H, halfCarry);

	--value;
	SetFlag(FLAG_N);
	SetZFlag(value);
}

void CPU::INC(BYTE& value)
{
	const bool hasHalfCarry = ((value & 0xF) + 1) & 0x10;
	SetFlagIf(FLAG_H, hasHalfCarry);

	++value;

	ClearFlag(FLAG_N);
	SetZFlag(value);
}

void CPU::OR(BYTE value)
{
	REGISTER_A |= value;

	ClearFlag(FLAG_N);
	ClearFlag(FLAG_H);
	ClearFlag(FLAG_C);
	SetZFlag(REGISTER_A);
}

void CPU::SBC(WORD value)
{
	SUB(value + IsFlagSet(FLAG_C));
}

void CPU::SUB(WORD value)
{
	const bool isCarry = value > REGISTER_A;
	SetFlagIf(FLAG_C, isCarry);

	const bool halfCarry = ((REGISTER_A & 0xF) - (value & 0xF)) & 0x10;
	SetFlagIf(FLAG_H, halfCarry);

	REGISTER_A -= value;

	SetFlag(FLAG_N);
	SetZFlag(REGISTER_A);
}

void CPU::XOR(BYTE value)
{
	REGISTER_A ^= value;

	ClearFlag(FLAG_N);
	ClearFlag(FLAG_H);
	ClearFlag(FLAG_C);
	SetZFlag(REGISTER_A);
}

void CPU::ADD_16(WORD value)
{
	uint32_t result = HL + value;

	// Carry flag is set if we overflow from bit 15
	const bool hasFullCarry = result & 0xFFFF0000;
	SetFlagIf(FLAG_C, hasFullCarry);

	// Half carry flag is set if we overflow from bit 11
	WORD halfAdd = (HL & 0xFFF) + (value & 0xFFF);
	const bool hasHalfCarry = halfAdd & 0xF000;
	SetFlagIf(FLAG_H, hasHalfCarry);

	HL = result & 0xFFFF;

	// Zero flag isn't touched
	// negative flag always cleared
	ClearFlag(FLAG_N);
}

void CPU::SetupOpcodes()
{
	m_opcodes[0x00] = &CPU::NOP;
	m_opcodes[0x01] = &CPU::LD_RR_n16;
	m_opcodes[0x02] = &CPU::LD_BC_A;
	m_opcodes[0x03] = &CPU::INC_RR;
	m_opcodes[0x04] = &CPU::INC_R;
	m_opcodes[0x05] = &CPU::DEC_R;
	m_opcodes[0x06] = &CPU::LD_R_n8;
	m_opcodes[0x07] = &CPU::RLCA;
	m_opcodes[0x08] = &CPU::LD_n16_SP;
	m_opcodes[0x09] = &CPU::ADD_HL_RR;
	m_opcodes[0x0A] = &CPU::LD_A_BC;
	m_opcodes[0x0B] = &CPU::DEC_RR;
	m_opcodes[0x0C] = &CPU::INC_R;
	m_opcodes[0x0D] = &CPU::DEC_R;
	m_opcodes[0x0E] = &CPU::LD_R_n8;
	m_opcodes[0x0F] = &CPU::RRCA;
	m_opcodes[0x10] = &CPU::STOP;
	m_opcodes[0x11] = &CPU::LD_RR_n16;
	m_opcodes[0x12] = &CPU::LD_DE_A;
	m_opcodes[0x13] = &CPU::INC_RR;
	m_opcodes[0x14] = &CPU::INC_R;
	m_opcodes[0x15] = &CPU::DEC_R;
	m_opcodes[0x16] = &CPU::LD_R_n8;
	m_opcodes[0x17] = &CPU::RLA;
	m_opcodes[0x18] = &CPU::JR;
	m_opcodes[0x19] = &CPU::ADD_HL_RR;
	m_opcodes[0x1A] = &CPU::LD_A_DE;
	m_opcodes[0x1B] = &CPU::DEC_RR;
	m_opcodes[0x1C] = &CPU::INC_R;
	m_opcodes[0x1D] = &CPU::DEC_R;
	m_opcodes[0x1E] = &CPU::LD_R_n8;
	m_opcodes[0x1F] = &CPU::RRA;
	m_opcodes[0x20] = &CPU::JR_cc;
	m_opcodes[0x21] = &CPU::LD_RR_n16;
	m_opcodes[0x22] = &CPU::LDI_HL_A;
	m_opcodes[0x23] = &CPU::INC_RR;
	m_opcodes[0x24] = &CPU::INC_R;
	m_opcodes[0x25] = &CPU::DEC_R;
	m_opcodes[0x26] = &CPU::LD_R_n8;
	m_opcodes[0x27] = &CPU::DAA;
	m_opcodes[0x28] = &CPU::JR_cc;
	m_opcodes[0x29] = &CPU::ADD_HL_RR;
	m_opcodes[0x2A] = &CPU::LDI_A_HL;
	m_opcodes[0x2B] = &CPU::DEC_RR;
	m_opcodes[0x2C] = &CPU::INC_R;
	m_opcodes[0x2D] = &CPU::DEC_R;
	m_opcodes[0x2E] = &CPU::LD_R_n8;
	m_opcodes[0x2F] = &CPU::CPL;
	m_opcodes[0x30] = &CPU::JR_cc;
	m_opcodes[0x31] = &CPU::LD_RR_n16;
	m_opcodes[0x32] = &CPU::LDD_HL_A;
	m_opcodes[0x33] = &CPU::INC_RR;
	m_opcodes[0x34] = &CPU::INC_R;
	m_opcodes[0x35] = &CPU::DEC_R;
	m_opcodes[0x36] = &CPU::LD_R_n8;
	m_opcodes[0x37] = &CPU::SCF;
	m_opcodes[0x38] = &CPU::JR_cc;
	m_opcodes[0x39] = &CPU::ADD_HL_RR;
	m_opcodes[0x3A] = &CPU::LDD_A_HL;
	m_opcodes[0x3B] = &CPU::DEC_RR;
	m_opcodes[0x3C] = &CPU::INC_R;
	m_opcodes[0x3D] = &CPU::DEC_R;
	m_opcodes[0x3E] = &CPU::LD_R_n8;
	m_opcodes[0x3F] = &CPU::CCF;
	m_opcodes[0x40] = &CPU::LD_R_R;
	m_opcodes[0x41] = &CPU::LD_R_R;
	m_opcodes[0x42] = &CPU::LD_R_R;
	m_opcodes[0x43] = &CPU::LD_R_R;
	m_opcodes[0x44] = &CPU::LD_R_R;
	m_opcodes[0x45] = &CPU::LD_R_R;
	m_opcodes[0x46] = &CPU::LD_R_R;
	m_opcodes[0x47] = &CPU::LD_R_R;
	m_opcodes[0x48] = &CPU::LD_R_R;
	m_opcodes[0x49] = &CPU::LD_R_R;
	m_opcodes[0x4A] = &CPU::LD_R_R;
	m_opcodes[0x4B] = &CPU::LD_R_R;
	m_opcodes[0x4C] = &CPU::LD_R_R;
	m_opcodes[0x4D] = &CPU::LD_R_R;
	m_opcodes[0x4E] = &CPU::LD_R_R;
	m_opcodes[0x4F] = &CPU::LD_R_R;
	m_opcodes[0x50] = &CPU::LD_R_R;
	m_opcodes[0x51] = &CPU::LD_R_R;
	m_opcodes[0x52] = &CPU::LD_R_R;
	m_opcodes[0x53] = &CPU::LD_R_R;
	m_opcodes[0x54] = &CPU::LD_R_R;
	m_opcodes[0x55] = &CPU::LD_R_R;
	m_opcodes[0x56] = &CPU::LD_R_R;
	m_opcodes[0x57] = &CPU::LD_R_R;
	m_opcodes[0x58] = &CPU::LD_R_R;
	m_opcodes[0x59] = &CPU::LD_R_R;
	m_opcodes[0x5A] = &CPU::LD_R_R;
	m_opcodes[0x5B] = &CPU::LD_R_R;
	m_opcodes[0x5C] = &CPU::LD_R_R;
	m_opcodes[0x5D] = &CPU::LD_R_R;
	m_opcodes[0x5E] = &CPU::LD_R_R;
	m_opcodes[0x5F] = &CPU::LD_R_R;
	m_opcodes[0x60] = &CPU::LD_R_R;
	m_opcodes[0x61] = &CPU::LD_R_R;
	m_opcodes[0x62] = &CPU::LD_R_R;
	m_opcodes[0x63] = &CPU::LD_R_R;
	m_opcodes[0x64] = &CPU::LD_R_R;
	m_opcodes[0x65] = &CPU::LD_R_R;
	m_opcodes[0x66] = &CPU::LD_R_R;
	m_opcodes[0x67] = &CPU::LD_R_R;
	m_opcodes[0x68] = &CPU::LD_R_R;
	m_opcodes[0x69] = &CPU::LD_R_R;
	m_opcodes[0x6A] = &CPU::LD_R_R;
	m_opcodes[0x6B] = &CPU::LD_R_R;
	m_opcodes[0x6C] = &CPU::LD_R_R;
	m_opcodes[0x6D] = &CPU::LD_R_R;
	m_opcodes[0x6E] = &CPU::LD_R_R;
	m_opcodes[0x6F] = &CPU::LD_R_R;
	m_opcodes[0x70] = &CPU::LD_R_R;
	m_opcodes[0x71] = &CPU::LD_R_R;
	m_opcodes[0x72] = &CPU::LD_R_R;
	m_opcodes[0x73] = &CPU::LD_R_R;
	m_opcodes[0x74] = &CPU::LD_R_R;
	m_opcodes[0x75] = &CPU::LD_R_R;
	m_opcodes[0x76] = &CPU::HALT;
	m_opcodes[0x77] = &CPU::LD_R_R;
	m_opcodes[0x78] = &CPU::LD_R_R;
	m_opcodes[0x79] = &CPU::LD_R_R;
	m_opcodes[0x7A] = &CPU::LD_R_R;
	m_opcodes[0x7B] = &CPU::LD_R_R;
	m_opcodes[0x7C] = &CPU::LD_R_R;
	m_opcodes[0x7D] = &CPU::LD_R_R;
	m_opcodes[0x7E] = &CPU::LD_R_R;
	m_opcodes[0x7F] = &CPU::LD_R_R;
	m_opcodes[0x80] = &CPU::ADD_R;
	m_opcodes[0x81] = &CPU::ADD_R;
	m_opcodes[0x82] = &CPU::ADD_R;
	m_opcodes[0x83] = &CPU::ADD_R;
	m_opcodes[0x84] = &CPU::ADD_R;
	m_opcodes[0x85] = &CPU::ADD_R;
	m_opcodes[0x86] = &CPU::ADD_R;
	m_opcodes[0x87] = &CPU::ADD_R;
	m_opcodes[0x88] = &CPU::ADC_R;
	m_opcodes[0x89] = &CPU::ADC_R;
	m_opcodes[0x8A] = &CPU::ADC_R;
	m_opcodes[0x8B] = &CPU::ADC_R;
	m_opcodes[0x8C] = &CPU::ADC_R;
	m_opcodes[0x8D] = &CPU::ADC_R;
	m_opcodes[0x8E] = &CPU::ADC_R;
	m_opcodes[0x8F] = &CPU::ADC_R;
	m_opcodes[0x90] = &CPU::SUB_R;
	m_opcodes[0x91] = &CPU::SUB_R;
	m_opcodes[0x92] = &CPU::SUB_R;
	m_opcodes[0x93] = &CPU::SUB_R;
	m_opcodes[0x94] = &CPU::SUB_R;
	m_opcodes[0x95] = &CPU::SUB_R;
	m_opcodes[0x96] = &CPU::SUB_R;
	m_opcodes[0x97] = &CPU::SUB_R;
	m_opcodes[0x98] = &CPU::SBC_R;
	m_opcodes[0x99] = &CPU::SBC_R;
	m_opcodes[0x9A] = &CPU::SBC_R;
	m_opcodes[0x9B] = &CPU::SBC_R;
	m_opcodes[0x9C] = &CPU::SBC_R;
	m_opcodes[0x9D] = &CPU::SBC_R;
	m_opcodes[0x9E] = &CPU::SBC_R;
	m_opcodes[0x9F] = &CPU::SBC_R;
	m_opcodes[0xA0] = &CPU::AND_R;
	m_opcodes[0xA1] = &CPU::AND_R;
	m_opcodes[0xA2] = &CPU::AND_R;
	m_opcodes[0xA3] = &CPU::AND_R;
	m_opcodes[0xA4] = &CPU::AND_R;
	m_opcodes[0xA5] = &CPU::AND_R;
	m_opcodes[0xA6] = &CPU::AND_R;
	m_opcodes[0xA7] = &CPU::AND_R;
	m_opcodes[0xA8] = &CPU::XOR_R;
	m_opcodes[0xA9] = &CPU::XOR_R;
	m_opcodes[0xAA] = &CPU::XOR_R;
	m_opcodes[0xAB] = &CPU::XOR_R;
	m_opcodes[0xAC] = &CPU::XOR_R;
	m_opcodes[0xAD] = &CPU::XOR_R;
	m_opcodes[0xAE] = &CPU::XOR_R;
	m_opcodes[0xAF] = &CPU::XOR_R;
	m_opcodes[0xB0] = &CPU::OR_R;
	m_opcodes[0xB1] = &CPU::OR_R;
	m_opcodes[0xB2] = &CPU::OR_R;
	m_opcodes[0xB3] = &CPU::OR_R;
	m_opcodes[0xB4] = &CPU::OR_R;
	m_opcodes[0xB5] = &CPU::OR_R;
	m_opcodes[0xB6] = &CPU::OR_R;
	m_opcodes[0xB7] = &CPU::OR_R;
	m_opcodes[0xB8] = &CPU::CP_R;
	m_opcodes[0xB9] = &CPU::CP_R;
	m_opcodes[0xBA] = &CPU::CP_R;
	m_opcodes[0xBB] = &CPU::CP_R;
	m_opcodes[0xBC] = &CPU::CP_R;
	m_opcodes[0xBD] = &CPU::CP_R;
	m_opcodes[0xBE] = &CPU::CP_R;
	m_opcodes[0xBF] = &CPU::CP_R;
	m_opcodes[0xC0] = &CPU::RET_cc;
	m_opcodes[0xC1] = &CPU::POP_RR;
	m_opcodes[0xC2] = &CPU::JP_cc;
	m_opcodes[0xC3] = &CPU::JP;
	m_opcodes[0xC4] = &CPU::CALL_cc;
	m_opcodes[0xC5] = &CPU::PUSH_RR;
	m_opcodes[0xC6] = &CPU::ADD_n8;
	m_opcodes[0xC7] = &CPU::RST_nn;
	m_opcodes[0xC8] = &CPU::RET_cc;
	m_opcodes[0xC9] = &CPU::RET;
	m_opcodes[0xCA] = &CPU::JP_cc;
	m_opcodes[0xCB] = &CPU::CB;
	m_opcodes[0xCC] = &CPU::CALL_cc;
	m_opcodes[0xCD] = &CPU::CALL;
	m_opcodes[0xCE] = &CPU::ADC_n8;
	m_opcodes[0xCF] = &CPU::RST_nn;
	m_opcodes[0xD0] = &CPU::RET_cc;
	m_opcodes[0xD1] = &CPU::POP_RR;
	m_opcodes[0xD2] = &CPU::JP_cc;
	m_opcodes[0xD3] = &CPU::NOP;
	m_opcodes[0xD4] = &CPU::CALL_cc;
	m_opcodes[0xD5] = &CPU::PUSH_RR;
	m_opcodes[0xD6] = &CPU::SUB_n8;
	m_opcodes[0xD7] = &CPU::RST_nn;
	m_opcodes[0xD8] = &CPU::RET_cc;
	m_opcodes[0xD9] = &CPU::RETI;
	m_opcodes[0xDA] = &CPU::JP_cc;
	m_opcodes[0xDB] = &CPU::NOP;
	m_opcodes[0xDC] = &CPU::CALL_cc;
	m_opcodes[0xDD] = &CPU::NOP;
	m_opcodes[0xDE] = &CPU::SBC_n8;
	m_opcodes[0xDF] = &CPU::RST_nn;
	m_opcodes[0xE0] = &CPU::LD_FF00_n8_A;
	m_opcodes[0xE1] = &CPU::POP_RR;
	m_opcodes[0xE2] = &CPU::LD_FF00_C_A;
	m_opcodes[0xE3] = &CPU::NOP;
	m_opcodes[0xE4] = &CPU::NOP;
	m_opcodes[0xE5] = &CPU::PUSH_RR;
	m_opcodes[0xE6] = &CPU::AND_n8;
	m_opcodes[0xE7] = &CPU::RST_nn;
	m_opcodes[0xE8] = &CPU::ADD_SP;
	m_opcodes[0xE9] = &CPU::JP_HL;
	m_opcodes[0xEA] = &CPU::LD_n16_A;
	m_opcodes[0xEB] = &CPU::NOP;
	m_opcodes[0xEC] = &CPU::NOP;
	m_opcodes[0xED] = &CPU::NOP;
	m_opcodes[0xEE] = &CPU::XOR_n8;
	m_opcodes[0xEF] = &CPU::RST_nn;
	m_opcodes[0xF0] = &CPU::LD_A_FF00_n8;
	m_opcodes[0xF1] = &CPU::POP_RR;
	m_opcodes[0xF2] = &CPU::LD_A_FF00_C;
	m_opcodes[0xF3] = &CPU::DI;
	m_opcodes[0xF4] = &CPU::NOP;
	m_opcodes[0xF5] = &CPU::PUSH_RR;
	m_opcodes[0xF6] = &CPU::OR_n8;
	m_opcodes[0xF7] = &CPU::RST_nn;
	m_opcodes[0xF8] = &CPU::LD_HL_SP_e;
	m_opcodes[0xF9] = &CPU::LD_SP_HL;
	m_opcodes[0xFA] = &CPU::LD_A_n16;
	m_opcodes[0xFB] = &CPU::EI;
	m_opcodes[0xFC] = &CPU::NOP;
	m_opcodes[0xFD] = &CPU::NOP;
	m_opcodes[0xFE] = &CPU::CP_n8;
	m_opcodes[0xFF] = &CPU::RST_nn;
}

#pragma region 8-bit Arithmetic and Logic Instructions

void CPU::ADC_R(BYTE opcode)
{
	BYTE rValue = GetValueBasedOnOpCode(opcode);
	ADC(rValue);
}

void CPU::ADC_n8(BYTE opcode)
{
	BYTE value = CycleRead_PC();
	ADC(value);
}

void CPU::ADD_R(BYTE opcode)
{
	BYTE rValue = GetValueBasedOnOpCode(opcode);
	ADD(rValue);
}

void CPU::ADD_n8(BYTE opcode)
{
	BYTE value = CycleRead_PC();
	ADD(value);
}

void CPU::AND_R(BYTE opcode)
{
	BYTE rValue = GetValueBasedOnOpCode(opcode);
	AND(rValue);
}

void CPU::AND_n8(BYTE opcode)
{
	BYTE value = CycleRead_PC();
	AND(value);
}

void CPU::CP_R(BYTE opcode)
{
	BYTE rValue = GetValueBasedOnOpCode(opcode);
	CP(rValue);
}

void CPU::CP_n8(BYTE opcode)
{
	BYTE value = CycleRead_PC();
	CP(value);
}

void CPU::DEC_R(BYTE opcode)
{
	BYTE sourceRegister = ((opcode >> 4) + 1) & 3;
	BYTE lo = opcode & BIT_3;

	if (sourceRegister == REGISTER_AF)
	{
		if (lo)
		{
			DEC(REGISTER_A);
			return;
		}

		BYTE value = CycleRead(HL);
		DEC(value);
		CycleWrite(HL, value);
		return;
	}

	if (lo)
	{
		DEC(registers[sourceRegister].lo);
		return;
	}

	DEC(registers[sourceRegister].hi);
}

void CPU::INC_R(BYTE opcode)
{
	BYTE sourceRegister = ((opcode >> 4) + 1) & 3;
	BYTE lo = opcode & BIT_3;

	if (sourceRegister == REGISTER_AF)
	{
		if (lo)
		{
			INC(REGISTER_A);
			return;
		}

		BYTE value = CycleRead(HL);
		INC(value);
		CycleWrite(HL, value);
		return;
	}

	if (lo)
	{
		INC(registers[sourceRegister].lo);
		return;
	}

	INC(registers[sourceRegister].hi);
}

void CPU::OR_R(BYTE opcode)
{
	BYTE rValue = GetValueBasedOnOpCode(opcode);
	OR(rValue);
}

void CPU::OR_n8(BYTE opcode)
{
	BYTE value = CycleRead_PC();
	OR(value);
}

void CPU::SBC_R(BYTE opcode)
{
	BYTE rValue = GetValueBasedOnOpCode(opcode);
	SBC(rValue);
}

void CPU::SBC_n8(BYTE opcode)
{
	BYTE value = CycleRead_PC();
	SBC(value);
}

void CPU::SUB_R(BYTE opcode)
{
	BYTE rValue = GetValueBasedOnOpCode(opcode);
	SUB(rValue);
}

void CPU::SUB_n8(BYTE opcode)
{
	BYTE value = CycleRead_PC();
	SUB(value);
}

void CPU::XOR_R(BYTE opcode)
{
	BYTE rValue = GetValueBasedOnOpCode(opcode);
	XOR(rValue);
}

void CPU::XOR_n8(BYTE opcode)
{
	BYTE value = CycleRead_PC();
	XOR(value);
}

#pragma endregion

#pragma region 16-bit Arithmetic Instructions

void CPU::ADD_HL_RR(BYTE opcode)
{
	BYTE sourceRegister = ((opcode >> 4) + 1) & 0x7;
	if (sourceRegister < REGISTER_COUNT)
	{
		ADD_16(registers[sourceRegister].pair);
		SpinCycle();
	}
}

void CPU::DEC_RR(BYTE opcode)
{
	BYTE sourceRegister = ((opcode >> 4) + 1) & 0x7;
	if (sourceRegister < REGISTER_COUNT)
	{
		--registers[sourceRegister];
		SpinCycle();
	}
}

void CPU::INC_RR(BYTE opcode)
{
	BYTE sourceRegister = ((opcode >> 4) + 1) & 0x7;
	if (sourceRegister < REGISTER_COUNT)
	{
		++registers[sourceRegister];
		SpinCycle();
	}
}

#pragma endregion

#pragma region Bit and Shift Operations

void CPU::BIT(BYTE opcode)
{
	BYTE value = GetValueBasedOnOpCode(opcode);
	BYTE bit = 1 << ((opcode >> 3) & 7);
	const bool isSet = value & bit;

	SetFlagIf(FLAG_Z, !isSet);
	ClearFlag(FLAG_N);
	SetFlag(FLAG_H);
}

void CPU::RES(BYTE opcode)
{
	BYTE value = GetValueBasedOnOpCode(opcode);
	BYTE bit = 1 << ((opcode >> 3) & 7);
	value &= ~bit;
	SetValueBasedOnOpCode(opcode, value);
}

void CPU::SET(BYTE opcode)
{
	BYTE value = GetValueBasedOnOpCode(opcode);
	BYTE bit = 1 << ((opcode >> 3) & 7);
	value |= bit;
	SetValueBasedOnOpCode(opcode, value);
}

void CPU::SWAP(BYTE opcode)
{
	BYTE value = GetValueBasedOnOpCode(opcode);
	value = ((value & 0xF) << 4) | ((value & 0xF0) >> 4);

	SetZFlag(value);
	ClearFlag(FLAG_N);
	ClearFlag(FLAG_H);
	ClearFlag(FLAG_C);

	SetValueBasedOnOpCode(opcode, value);
}

void CPU::RL(BYTE opcode)
{
	BYTE value = GetValueBasedOnOpCode(opcode);

	bool isCarrySet = IsFlagSet(FLAG_C);
	bool willShiftACarry = value & BIT_7;

	value <<= 1;
	value += isCarrySet;

	SetZFlag(value);
	ClearFlag(FLAG_N);
	ClearFlag(FLAG_H);
	SetFlagIf(FLAG_C, willShiftACarry);

	SetValueBasedOnOpCode(opcode, value);
}

void CPU::RLA(BYTE opcode)
{
	bool isCarrySet = IsFlagSet(FLAG_C);
	bool willShiftACarry = REGISTER_A & BIT_7;

	REGISTER_A <<= 1;
	REGISTER_A += isCarrySet;

	ClearFlag(FLAG_Z);
	ClearFlag(FLAG_N);
	ClearFlag(FLAG_H);
	SetFlagIf(FLAG_C, willShiftACarry);
}

void CPU::RLC(BYTE opcode)
{
	BYTE value = GetValueBasedOnOpCode(opcode);

	bool willShiftACarry = value & BIT_7;

	value <<= 1;
	value += willShiftACarry;

	SetZFlag(value);
	ClearFlag(FLAG_N);
	ClearFlag(FLAG_H);
	SetFlagIf(FLAG_C, willShiftACarry);

	SetValueBasedOnOpCode(opcode, value);
}

void CPU::RLCA(BYTE opcode)
{
	bool willShiftACarry = REGISTER_A & BIT_7;

	REGISTER_A <<= 1;
	REGISTER_A += willShiftACarry;

	ClearFlag(FLAG_Z);
	ClearFlag(FLAG_N);
	ClearFlag(FLAG_H);
	SetFlagIf(FLAG_C, willShiftACarry);
}

void CPU::RR(BYTE opcode)
{
	BYTE value = GetValueBasedOnOpCode(opcode);

	bool isCarrySet = IsFlagSet(FLAG_C);
	bool willShiftACarry = value & BIT_0;

	value >>= 1;
	if (isCarrySet)
	{
		value |= BIT_7;
	}

	SetZFlag(value);
	ClearFlag(FLAG_N);
	ClearFlag(FLAG_H);
	SetFlagIf(FLAG_C, willShiftACarry);

	SetValueBasedOnOpCode(opcode, value);
}

void CPU::RRA(BYTE opcode)
{
	bool isCarrySet = IsFlagSet(FLAG_C);
	bool willShiftACarry = REGISTER_A & BIT_0;

	REGISTER_A >>= 1;
	if (isCarrySet)
	{
		REGISTER_A |= BIT_7;
	}

	ClearFlag(FLAG_Z);
	ClearFlag(FLAG_N);
	ClearFlag(FLAG_H);
	SetFlagIf(FLAG_C, willShiftACarry);
}

void CPU::RRC(BYTE opcode)
{
	BYTE value = GetValueBasedOnOpCode(opcode);
	bool willShiftACarry = value & BIT_0;

	value >>= 1;
	if (willShiftACarry)
	{
		value |= BIT_7;
	}

	SetZFlag(value);
	ClearFlag(FLAG_N);
	ClearFlag(FLAG_H);
	SetFlagIf(FLAG_C, willShiftACarry);

	SetValueBasedOnOpCode(opcode, value);
}

void CPU::RRCA(BYTE opcode)
{
	bool willShiftACarry = REGISTER_A & BIT_0;

	REGISTER_A >>= 1;
	if (willShiftACarry)
	{
		REGISTER_A |= BIT_7;
	}

	ClearFlag(FLAG_Z);
	ClearFlag(FLAG_N);
	ClearFlag(FLAG_H);
	SetFlagIf(FLAG_C, willShiftACarry);
}

void CPU::SLA(BYTE opcode)
{
	BYTE value = GetValueBasedOnOpCode(opcode);
	bool willShiftACarry = value & BIT_7;

	value <<= 1;
	SetZFlag(value);
	ClearFlag(FLAG_N);
	ClearFlag(FLAG_H);
	SetFlagIf(FLAG_C, willShiftACarry);
	SetValueBasedOnOpCode(opcode, value);
}

void CPU::SRA(BYTE opcode)
{
	BYTE value = GetValueBasedOnOpCode(opcode);
	bool willShiftACarry = value & BIT_0;
	bool oldBit7 = value & BIT_7;

	value >>= 1;
	value |= oldBit7;

	SetZFlag(value);
	ClearFlag(FLAG_N);
	ClearFlag(FLAG_H);
	SetFlagIf(FLAG_C, willShiftACarry);
	SetValueBasedOnOpCode(opcode, value);
}

void CPU::SRL(BYTE opcode)
{
	BYTE value = GetValueBasedOnOpCode(opcode);
	bool willShiftACarry = value & BIT_0;

	value >>= 1;

	SetZFlag(value);
	ClearFlag(FLAG_N);
	ClearFlag(FLAG_H);
	SetFlagIf(FLAG_C, willShiftACarry);
	SetValueBasedOnOpCode(opcode, value);
}

#pragma endregion

#pragma region Load Instructions

void CPU::LD_R_R(BYTE opcode)
{
	BYTE targetRegister = ((opcode >> 4) + 1) & 3;
	BYTE value = GetValueBasedOnOpCode(opcode);
	BYTE lo = opcode & BIT_3;

	if (targetRegister == REGISTER_AF)
	{
		if (lo)
		{
			REGISTER_A = value;
			return;
		}

		CycleWrite(HL, value);
		return;
	}

	if (lo)
	{
		registers[targetRegister].lo = value;
		return;
	}

	registers[targetRegister].hi = value;
}

void CPU::LD_R_n8(BYTE opcode)
{
	BYTE targetRegister = ((opcode >> 4) + 1) & 3;
	BYTE lo = opcode & BIT_3;

	BYTE value = CycleRead_PC();
	if (targetRegister == REGISTER_AF)
	{
		if (lo)
		{
			REGISTER_A = value;
			return;
		}

		CycleWrite(HL, value);
		return;
	}

	if (lo)
	{
		registers[targetRegister].lo = value;
		return;
	}

	registers[targetRegister].hi = value;
}

void CPU::LD_RR_n16(BYTE opcode)
{
	BYTE sourceRegister = ((opcode >> 4) + 1) & 0x7;
	if (sourceRegister < REGISTER_COUNT)
	{
		registers[sourceRegister].lo = CycleRead_PC();
		registers[sourceRegister].hi = CycleRead_PC();
	}
}

void CPU::LD_BC_A(BYTE opcode)
{
	CycleWrite(BC, REGISTER_A);
}

void CPU::LD_DE_A(BYTE opcode)
{
	CycleWrite(DE, REGISTER_A);
}

void CPU::LD_A_BC(BYTE opcode)
{
	REGISTER_A = CycleRead(BC);
}

void CPU::LD_A_DE(BYTE opcode)
{
	REGISTER_A = Read(DE);
}

void CPU::LDI_HL_A(BYTE opcode)
{
	CycleWrite(HL, REGISTER_A);
	++HL;
}

void CPU::LDD_HL_A(BYTE opcode)
{
	CycleWrite(HL, REGISTER_A);
	--HL;
}

void CPU::LDI_A_HL(BYTE opcode)
{
	REGISTER_A = CycleRead(HL);
	++HL;
}

void CPU::LDD_A_HL(BYTE opcode)
{
	REGISTER_A = CycleRead(HL);
	--HL;
}

void CPU::LD_n16_A(BYTE opcode)
{
	WORD address = CycleReadWord_PC();
	CycleWrite(address, REGISTER_A);
}

void CPU::LD_FF00_n8_A(BYTE opcode)
{
	WORD address = 0xFF00 + CycleRead_PC();
	CycleWrite(address, REGISTER_A);
}

void CPU::LD_FF00_C_A(BYTE opcode)
{
	WORD address = 0xFF00 + REGISTER_C;
	CycleWrite(address, REGISTER_A);
}

void CPU::LD_A_n16(BYTE opcode)
{
	WORD address = CycleReadWord_PC();
	REGISTER_A = CycleRead(address);
}

void CPU::LD_A_FF00_n8(BYTE opcode)
{
	WORD address = 0xFF00 + CycleRead_PC();
	REGISTER_A = CycleRead(address);
}

void CPU::LD_A_FF00_C(BYTE opcode)
{
	WORD address = 0xFF00 + REGISTER_C;
	REGISTER_A = CycleRead(address);
}

#pragma endregion

#pragma region Jumps and Subroutines

bool CPU::CheckCondition(BYTE opcode)
{
	BYTE flagCheck = (opcode >> 3) & 0x3;
	switch (flagCheck)
	{
	case 0:
		return !IsFlagSet(FLAG_Z);
	case 1:
		return IsFlagSet(FLAG_Z);
	case 2:
		return !IsFlagSet(FLAG_C);
	case 3:
		return IsFlagSet(FLAG_C);
	}

	return false;
}

void CPU::JP(BYTE opcode)
{
	WORD address = CycleReadWord_PC();
	PC = address;
	SpinCycle();
}

void CPU::JP_HL(BYTE opcode)
{
	PC = HL;
}

void CPU::JP_cc(BYTE opcode)
{
	WORD address = CycleReadWord_PC();
	if (CheckCondition(opcode))
	{
		PC = address;
		SpinCycle();
	}
}

void CPU::JR(BYTE opcode)
{
	SIGNED_BYTE offset = static_cast<SIGNED_BYTE>(CycleRead_PC());
	PC += offset;
	SpinCycle();
}

void CPU::JR_cc(BYTE opcode)
{
	SIGNED_BYTE offset = static_cast<SIGNED_BYTE>(CycleRead_PC());
	if (CheckCondition(opcode))
	{
		PC += offset;
		SpinCycle();
	}
}

void CPU::CALL(BYTE opcode)
{
	WORD address = CycleReadWord_PC();
	PushStack(PC);
	SpinCycle();

	PC = address;
}

void CPU::CALL_cc(BYTE opcode)
{
	WORD address = CycleReadWord_PC();
	if (CheckCondition(opcode))
	{
		PushStack(PC);
		SpinCycle();

		PC = address;
	}
}

void CPU::RET(BYTE opcode)
{
	PC = PopStack();
	SpinCycle();
}

void CPU::RET_cc(BYTE opcode)
{
	SpinCycle();
	if (CheckCondition(opcode))
	{
		PC = PopStack();
		SpinCycle();
	}
}

void CPU::RETI(BYTE opcode)
{
	PC = PopStack();
	SpinCycle();
	m_interruptMasterTimer = 0x1;
}

void CPU::RST_nn(BYTE opcode)
{
	WORD address = opcode ^ 0xC7;
	PushStack(PC);
	PC = address;
	SpinCycle();
}

#pragma endregion

#pragma region Stack Operations Instructions

void CPU::LD_SP_HL(BYTE opcode)
{
	SP = HL;
	SpinCycle();
}

void CPU::ADD_SP(BYTE opcode)
{
	SIGNED_BYTE offset = static_cast<SIGNED_BYTE>(CycleRead_PC());
	SpinCycle(2);

	const bool halfCarry = ((SP & 0xF) + (offset & 0xF) > 0xF);
	const bool fullCarry = (SP & 0xFF) + (offset & 0xFF) > 0xFF;

	ClearFlag(FLAG_Z);
	ClearFlag(FLAG_N);
	SetFlagIf(FLAG_H, halfCarry);
	SetFlagIf(FLAG_C, fullCarry);

	SP += offset;
}

void CPU::LD_n16_SP(BYTE opcode)
{
	WORD address = CycleReadWord_PC();

	BYTE lsb = SP & 0xFF;
	BYTE msb = (SP >> 8) & 0xFF;

	CycleWrite(address, lsb);
	CycleWrite(address + 1, msb);
}

void CPU::LD_HL_SP_e(BYTE opcode)
{
	SIGNED_BYTE offset = static_cast<SIGNED_BYTE>(CycleRead_PC());

	HL = SP + offset;
	const bool halfCarry = ((SP & 0xF) + (offset & 0xF) > 0xF);
	const bool fullCarry = (SP & 0xFF) + (offset & 0xFF) > 0xFF;

	ClearFlag(FLAG_Z);
	ClearFlag(FLAG_N);
	SetFlagIf(FLAG_H, halfCarry);
	SetFlagIf(FLAG_C, fullCarry);
	SpinCycle();
}

void CPU::PUSH_RR(BYTE opcode)
{
	BYTE sourceRegister = ((opcode >> 4) + 1) & 0x3;
	PushStack(registers[sourceRegister]);
}

void CPU::POP_RR(BYTE opcode)
{
	BYTE sourceRegister = ((opcode >> 4) + 1) & 0x3;
	registers[sourceRegister] = PopStack();
}

#pragma endregion

#pragma region Miscellaneous Instructions

void CPU::CCF(BYTE opcode)
{
	ClearFlag(FLAG_N);
	ClearFlag(FLAG_H);
	FlipFlag(FLAG_C);
}

void CPU::CPL(BYTE opcode)
{
	REGISTER_A = ~REGISTER_A;

	SetFlag(FLAG_N);
	SetFlag(FLAG_H);
}

void CPU::DAA(BYTE opcode)
{
	WORD result = REGISTER_A;

	if (IsFlagSet(FLAG_N))
	{
		if (IsFlagSet(FLAG_H))
		{
			result = (result - 0x06) & 0xFF;
		}

		if (IsFlagSet(FLAG_C))
		{
			result -= 0x60;
		}
	}
	else
	{
		if (IsFlagSet(FLAG_H) || (result & 0x0F) > 0x09)
		{
			result += 0x06;
		}

		if (IsFlagSet(FLAG_C) || result > 0x9F) {
			result += 0x60;
		}
	}

	const bool setCarryFlag = (result & 0x100) == 0x100;

	REGISTER_A = result & 0xFF;
	SetZFlag(REGISTER_A);
	SetFlagIf(FLAG_C, setCarryFlag);
	ClearFlag(FLAG_H);
}

void CPU::DI(BYTE opcode)
{
	m_interruptMasterTimer = 0x0;
}

void CPU::EI(BYTE opcode)
{
	m_interruptMasterTimer = 0x1;
}

void CPU::HALT(BYTE opcode)
{
	m_isHalted = true;
}

void CPU::NOP(BYTE opcode)
{
}

void CPU::SCF(BYTE opcode)
{
	ClearFlag(FLAG_N);
	ClearFlag(FLAG_H);
	SetFlag(FLAG_C);
}

void CPU::STOP(BYTE opcode)
{
	m_isStopped = true;

	// When entering with IF & IE, the 2nd byte of STOP is actually executed
	BYTE interruptFlag = m_io[0x0F];
	const bool interruptPending = m_interruptEnableRegister & interruptFlag & 0x1F;
	if (!interruptPending)
	{
		CycleRead_PC();
	}
}

void CPU::CB(BYTE opcode)
{
	opcode = CycleRead_PC();
	switch (opcode >> 3)
	{
	case 0:
		RLC(opcode);
		break;
	case 1:
		RRC(opcode);
		break;
	case 2:
		RL(opcode);
		break;
	case 3:
		RR(opcode);
		break;
	case 4:
		SLA(opcode);
		break;
	case 5:
		SRA(opcode);
		break;
	case 6:
		SWAP(opcode);
		break;
	case 7:
		SRL(opcode);
		break;
	default:
		if ((opcode & 0xC0) == 0x40)
		{
			BIT(opcode);
		}
		else if ((opcode & 0xC0) == 0x80)
		{
			RES(opcode);
		}
		else if ((opcode & 0xC0) == 0xC0)
		{
			SET(opcode);
		}
		break;
	}
}

#pragma endregion
