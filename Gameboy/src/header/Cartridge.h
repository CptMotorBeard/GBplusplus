#pragma once

class MemoryBankController;
class Cartridge
{
public:
	explicit Cartridge(std::string filePath);
	~Cartridge();

	// Addresses should be in the range 0x0000 - 0x7FFF for rom memory or 0xA000 to 0xBFFF for ram memory
	BYTE ReadMemory(WORD address);
	// Addresses should be in the range 0x0000 - 0x7FFF for rom memory or 0xA000 to 0xBFFF for ram memory
	void WriteMemory(WORD address, BYTE data);

	const std::string& GetTitle() const { return m_title; }

	int DumpRom(BYTE*& rom) const;

private:
	/*
		An internal information area is located at 0x0100 - 0x014F in
		each cartridge. It contains the following values :
		
		0x0100 - 0x0103 - Entry Point
		
		After displaying the Nintendo Logo, the built-in boot procedure
		jumps to this address (0x0100), which should then jump to the
		actual main program in the cartridge. Usually this 4 byte area
		contains a NOP instruction, followed by a JP 0150h instruction.
		But not always.

		0x0104 - 0x0133		- Nintendo Logo
		0x0134 - 0x0143		- Title
		0x0143				- CGB Flag
		In older cartridges this byte has been part of the Title
		In CGB cartridges the upper bit is used to enable CGB functions.
		This is required, otherwise the CGB switches itself into Non-CGB-Mode.
		Typical values are:
			80h - Game supports CGB functions, but works on old gameboys also.
			C0h - Game works on CGB only (physically the same as 80h).

		0x0144 - 0x0145		- New Licensee Code
		0x0146				- SGB Flag
		Specifies whether the game supports SGB functions, common values are:
			00h = No SGB functions (Normal Gameboy or CGB only game)
			03h = Game supports SGB functions
		The SGB disables its SGB functions if this byte is set to another value than 03h.

		0x0147				- Cartridge Type
			0x00  ROM ONLY                 0x13  MBC3+RAM+BATTERY
			0x01  MBC1                     0x15  MBC4
			0x02  MBC1+RAM                 0x16  MBC4+RAM
			0x03  MBC1+RAM+BATTERY         0x17  MBC4+RAM+BATTERY
			0x05  MBC2                     0x19  MBC5
			0x06  MBC2+BATTERY             0x1A  MBC5+RAM
			0x08  ROM+RAM                  0x1B  MBC5+RAM+BATTERY
			0x09  ROM+RAM+BATTERY          0x1C  MBC5+RUMBLE
			0x0B  MMM01                    0x1D  MBC5+RUMBLE+RAM
			0x0C  MMM01+RAM                0x1E  MBC5+RUMBLE+RAM+BATTERY
			0x0D  MMM01+RAM+BATTERY        0xFC  POCKET CAMERA
			0x0F  MBC3+TIMER+BATTERY       0xFD  BANDAI TAMA5
			0x10  MBC3+TIMER+RAM+BATTERY   0xFE  HuC3
			0x11  MBC3                     0xFF  HuC1+RAM+BATTERY
			0x12  MBC3+RAM

		0x0148				- ROM Size			
			0x00 -  32KByte (no ROM banking)
			0x01 -  64KByte (4 banks)
			0x02 - 128KByte (8 banks)
			0x03 - 256KByte (16 banks)
			0x04 - 512KByte (32 banks)
			0x05 -   1MByte (64 banks)  - only 63 banks used by MBC1
			0x06 -   2MByte (128 banks) - only 125 banks used by MBC1
			0x07 -   4MByte (256 banks)
			0x52 - 1.1MByte (72 banks)
			0x53 - 1.2MByte (80 banks)
			0x54 - 1.5MByte (96 banks)

		0x0149				- RAM Size
		Specifies the size of the external RAM in the cartridge (if any).
			0x00 - None
			0x01 - 2 KBytes
			0x02 - 8 Kbytes
			0x03 - 32 KBytes (4 banks of 8KBytes each)
		When using a MBC2 chip 00h must be specified in this entry,
		even though the MBC2 includes a built-in RAM of 512 x 4 bits.

		0x014A				- Destination Code
		0x014B				- Old Licensee Code
		0x014C				- Mask ROM Version number
		0x014D				- Header Checksum
		0x014E - 0x014F		- Global Checksum
	*/
	struct CartridgeHeader
	{
		BYTE		entryPoint		[0x04];	// 0x0100 - 0x0103
		BYTE		nintendoLogo	[0x30];	// 0x0104 - 0x0133
		union
		{
			BYTE	title			[0x10];	// 0x0134 - 0x0143
			struct
			{
				BYTE title			[0x0F];	// 0x0134 - 0x0142
				BYTE cgbFlag;				// 0x0143
			}		cgbTitle;
		}			titleSection;
		BYTE		newLicenseeCode	[0x02];	// 0x0144 - 0x0145
		BYTE		sgbFlab;				// 0x0146
		BYTE		cartridgeType;			// 0x0147
		BYTE		romSize;				// 0x0148
		BYTE		ramSize;				// 0x0149
		BYTE		destinationCode;		// 0x014A
		BYTE		oldLicenseeCode;		// 0x014B
		BYTE		masROMVersionNumber;	// 0x014C
		BYTE		headerChecksum;			// 0x014D
		BYTE		globalChecksum	[0x02];	// 0x014E - 0x014F
	};

	MemoryBankController* m_mbc;
	std::string m_title;
};