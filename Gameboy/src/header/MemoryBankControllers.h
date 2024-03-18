#pragma once

#define ROM_BANK_SIZE 0x4000
#define RAM_BANK_SIZE 0x2000

class MemoryBankController
{
public:
	MemoryBankController();
	virtual ~MemoryBankController();

	virtual BYTE ReadMemory (WORD address) = 0;
	virtual void WriteMemory(WORD address, BYTE data) = 0;

	int DumpRom(BYTE*& rom) const 
	{
		rom = m_rom;
		return m_romSize;
	}

	int DumpRam(BYTE*& ram) const 
	{
		ram = m_ram;
		return m_ramSize;
	}

protected:
	void Initialize(char* cartridgeBuffer, int bufferSize, int romSize, int ramSize);

	int m_romSize;
	int m_ramSize;

	BYTE* m_rom;
	BYTE* m_ram;
};

class MemoryBankController_None : public MemoryBankController
{
public:
	MemoryBankController_None(char* cartridgeBuffer, int bufferSize, int romSize, int ramSize);
	~MemoryBankController_None() override;

	BYTE ReadMemory(WORD address) override;
	void WriteMemory(WORD address, BYTE data) override;
};

class MemoryBankController_MBC1 : public MemoryBankController
{
public:
	MemoryBankController_MBC1(char* cartridgeBuffer, int bufferSize, int romSize, int ramSize);
	~MemoryBankController_MBC1() override;

	BYTE ReadMemory(WORD address) override;
	void WriteMemory(WORD address, BYTE data) override;

private:
	BYTE m_ramEnabled;
	BYTE m_ramBank;
	BYTE m_romBank;
	BYTE m_bankMode;

	bool IsRAMBankMode() { return m_bankMode; }
};

class MemoryBankController_MBC2 : public MemoryBankController
{
public:
	MemoryBankController_MBC2(char* cartridgeBuffer, int bufferSize, int romSize);
	~MemoryBankController_MBC2() override;

	BYTE ReadMemory(WORD address) override;
	void WriteMemory(WORD address, BYTE data) override;

private:
	BYTE m_romBank;
	BYTE m_ramEnabled;
};

class MemoryBankController_MBC3 : public MemoryBankController
{
public:
	MemoryBankController_MBC3(char* cartridgeBuffer, int bufferSize, int romSize, int ramSize);
	~MemoryBankController_MBC3() override;

	BYTE ReadMemory(WORD address) override;
	void WriteMemory(WORD address, BYTE data) override;

private:
	BYTE m_ramAndTimerEnabled;
	BYTE m_ramBank;
	BYTE m_romBank;

	bool m_isRTCMode;

	// Timer
	// 08h  RTC S   Seconds   0 - 59 (0 - 3Bh)
	// 09h  RTC M   Minutes   0 - 59 (0 - 3Bh)
	// 0Ah  RTC H   Hours     0 - 23 (0 - 17h)
	// 0Bh  RTC DL  Lower 8 bits of Day Counter(0 - FFh)
	// 0Ch  RTC DH  Upper 1 bit of Day Counter, Carry Bit, Halt Flag
	//	Bit 0  Most significant bit of Day Counter(Bit 8)
	//	Bit 6  Halt(0 = Active, 1 = Stop Timer)
	//	Bit 7  Day Counter Carry Bit(1 = Counter Overflow)

	BYTE m_rtc_s;
	BYTE m_rtc_m;
	BYTE m_rtc_h;
	BYTE m_rtc_dl;
	BYTE m_rtc_dh;
};

class MemoryBankController_MBC5 : public MemoryBankController
{
public:
	MemoryBankController_MBC5(char* cartridgeBuffer, int bufferSize, int romSize, int ramSize);
	~MemoryBankController_MBC5() override;

	BYTE ReadMemory(WORD address) override;
	void WriteMemory(WORD address, BYTE data) override;

private:
	BYTE m_ramEnabled;
	BYTE m_ramBank;
	WORD m_romBank;
};

class MemoryBankControllerFactory
{
public:
	static MemoryBankController* CreateMemoryBank(BYTE type, BYTE romSize, BYTE ramSize, char* cartridgeBuffer, int bufferLength);
};