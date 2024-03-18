#include "stdafx.h"
#include "header/MemoryBankControllers.h"
#include "header/Debug.h"

MemoryBankController::MemoryBankController() :
    m_romSize(0),
    m_ramSize(0),
    m_rom(nullptr),
    m_ram(nullptr)
{
}

void MemoryBankController::Initialize(char* cartridgeBuffer, int bufferSize, int romSize, int ramSize)
{
    m_romSize = romSize;
    m_ramSize = ramSize;
    m_rom = new BYTE[romSize];
    m_ram = new BYTE[ramSize];

    memset(m_ram, 0, ramSize);

    int i = 0;
    for (; i < bufferSize && i < romSize; i++)
    {
        m_rom[i] = cartridgeBuffer[i];
    }

    for (; i < romSize; i++)
    {
        m_rom[i] = 0x00;
    }
}

MemoryBankController::~MemoryBankController()
{
    delete[] m_rom;
    delete[] m_ram;
}

//////////////////////////////////////////////////////////////////////////////////////

MemoryBankController_None::MemoryBankController_None(char* cartridgeBuffer, int bufferSize, int romSize, int ramSize)
{
    // Max 32 kb rom, 8 kb ram
    DEBUG_ASSERT_N(romSize <= 0x8000);
    DEBUG_ASSERT_N(ramSize <= 0x2000);

    if (romSize > 0x8000)
    {
        romSize = 0x8000;
    }

    if (ramSize > 0x2000)
    {
        ramSize = 0x2000;
    }

    Initialize(cartridgeBuffer, bufferSize, romSize, ramSize);
}

MemoryBankController_None::~MemoryBankController_None()
{
}

BYTE MemoryBankController_None::ReadMemory(WORD address)
{
    // 0x0000 - 0x7FFF is the cartridge ROM
    if (address < 0x8000)
    {
        DEBUG_ASSERT_N(address < m_romSize);
        if (address < m_romSize)
        {
            return m_rom[address];
        }
    }

    // 0xA000 to 0xBFFF is the cartridge RAM
    if (address >= 0xA000 && address <= 0xC000)
    {
        MEMORY_ADDRESS ramAddress = address - 0xA000;
        DEBUG_ASSERT_N(ramAddress < m_ramSize);
        if (ramAddress < m_ramSize)
        {
            return m_ram[ramAddress];
        }
    }

    DEBUG_ASSERT(false, "Reading from invalid memory");
    return 0xFF;
}

void MemoryBankController_None::WriteMemory(WORD address, BYTE data)
{
    // RAM is read/write
    if (address >= 0xA000 && address <= 0xC000)
    {
        MEMORY_ADDRESS ramAddress = address - 0xA000;
        DEBUG_ASSERT_N(ramAddress < m_ramSize);
        if (ramAddress < m_ramSize)
        {
            m_ram[ramAddress] = data;
        }
        return;
    }

    DEBUG_ASSERT(false, "Writing to invalid memory");
}

//////////////////////////////////////////////////////////////////////////////////////

MemoryBankController_MBC1::MemoryBankController_MBC1(char* cartridgeBuffer, int bufferSize, int romSize, int ramSize) :
    m_ramEnabled(0x00),
    m_romBank(0x01),
    m_ramBank(0x00),
    m_bankMode(0x00)
{
    // Max 2 mb of rom, 32 kb ram
    DEBUG_ASSERT_N(romSize <= 0x20'0000);
    DEBUG_ASSERT_N(ramSize <= 0x8000);

    if (romSize > 0x20'0000)
    {
        romSize = 0x20'0000;
    }

    if (ramSize > 0x8000)
    {
        ramSize = 0x8000;
    }

    Initialize(cartridgeBuffer, bufferSize, romSize, ramSize);
}

MemoryBankController_MBC1::~MemoryBankController_MBC1()
{
}

BYTE MemoryBankController_MBC1::ReadMemory(WORD address)
{
    // This area always contains the first 16KBytes of the cartridge ROM.
    if (address < 0x4000)
    {
        DEBUG_ASSERT_N(address < m_romSize);
        if (address < m_romSize)
        {
            return m_rom[address];
        }
    }

    // This area may contain any of the further 16KByte banks of the ROM,
    // allowing to address up to 125 ROM Banks (almost 2MByte).
    // 
    // As described below, bank numbers 20h, 40h, and 60h cannot be used, resulting in the odd amount of 125 banks.
    if (address >= 0x4000 && address < 0x8000)
    {
        MEMORY_ADDRESS romAddress = address - 0x4000 + (ROM_BANK_SIZE * m_romBank);
        DEBUG_ASSERT_N(romAddress < m_romSize);
        if (romAddress < m_romSize)
        {
            return m_rom[romAddress];
        }
    }

    // This area is used to address external RAM in the cartridge (if any).
    // External RAM is often battery buffered, allowing to store game positions or high score tables,
    // even if the gameboy is turned off, or if the cartridge is removed from the gameboy.
    // 
    // Available RAM sizes are: 2KByte (at A000-A7FF), 8KByte (at A000-BFFF),
    // and 32KByte (in form of four 8K banks at A000-BFFF).
    if (address >= 0xA000 && address <= 0xC000)
    {
        DEBUG_ASSERT_N(m_ramEnabled);
        if (m_ramEnabled)
        {
            MEMORY_ADDRESS ramAddress = address - 0xA000;
            if (IsRAMBankMode())
            {
                ramAddress = ramAddress + (RAM_BANK_SIZE * m_ramBank);
            }

            DEBUG_ASSERT_N(ramAddress < m_ramSize);
            if (ramAddress < m_ramSize)
            {
                return m_ram[ramAddress];
            }
        }

        return 0x00;
    }

    DEBUG_ASSERT(false, "Reading from invalid memory");
    return 0x00;
}

void MemoryBankController_MBC1::WriteMemory(WORD address, BYTE data)
{
    // Before external RAM can be read or written, it must be enabled by writing to this address space.
    // It is recommended to disable external RAM after accessing it, in order to protect its contents from damage during power down of the gameboy.
    // Usually the following values are used :
    //      00h  Disable RAM(default)
    //      0Ah  Enable RAM
    // Practically any value with 0Ah in the lower 4 bits enables RAM, and any other value disables RAM.
    if (address < 0x2000)
    {
        m_ramEnabled = data & 0x0A;
        return;
    }

    // Writing to this address space selects the lower 5 bits of the ROM Bank Number (in range 01-1Fh).
    // When 00h is written, the MBC translates that to bank 01h also.
    // That doesn't harm so far, because ROM Bank 00h can be always directly accessed by reading from 0000-3FFF.
    // But (when using the register below to specify the upper ROM Bank bits), the same happens for Bank 20h, 40h, and 60h.
    // Any attempt to address these ROM Banks will select Bank 21h, 41h, and 61h instead.
    if (address >= 0x2000 && address < 0x4000)
    {
        data &= 0x1F;
        if (data == 0x00)
        {
            data = 0x01;
        }

        m_romBank &= 0xE0;  // clear bits
        m_romBank |= data;  // set new bits
        return;
    }

    // This 2bit register can be used to select a RAM Bank in range from 00-03h, or to specify the upper two bits (Bit 5-6) of the ROM Bank number
    // depending on the current ROM/RAM Mode. (See below.)
    if (address >= 0x4000 && address < 0x6000)
    {
        data &= 0x03;
        if (IsRAMBankMode())
        {
            m_ramBank = data;
        }
        else
        {
            m_romBank &= 0x1F;          // clear bits
            m_romBank |= (data << 5);   // set new bits
        }
        return;
    }

    // This 1bit Register selects whether the two bits of the above register should be used as upper two bits of the ROM Bank, or as RAM Bank Number.
    //      00h = ROM Banking Mode(up to 8KByte RAM, 2MByte ROM) (default)
    //      01h = RAM Banking Mode(up to 32KByte RAM, 512KByte ROM)
    // The program may freely switch between both modes.
    // The only limitiation is that only RAM Bank 00h can be used during Mode 0, and only ROM Banks 00-1Fh can be used during Mode 1.
    if (address >= 0x6000 && address <= 0x8000)
    {
        data &= 0x01;
        m_bankMode = data;

        if (IsRAMBankMode())
        {
            m_romBank &= 0x1F;  // clear bits as ram bank uses same 2 bits
        }

        return;
    }

    // RAM can only be accessed if it is enabled
    if (address >= 0xA000 && address <= 0xC000)
    {
        DEBUG_ASSERT_N(m_ramEnabled);
        if (m_ramEnabled)
        {
            MEMORY_ADDRESS ramAddress = address - 0xA000;
            if (IsRAMBankMode())
            {
                ramAddress = ramAddress + (RAM_BANK_SIZE * m_ramBank);
            }

            DEBUG_ASSERT_N(ramAddress < m_ramSize);
            if (ramAddress < m_ramSize)
            {
                m_ram[ramAddress] = data;
            }
        }

        return;
    }

    DEBUG_ASSERT(false, "Reading from invalid memory");
}

//////////////////////////////////////////////////////////////////////////////////////

MemoryBankController_MBC2::MemoryBankController_MBC2(char* cartridgeBuffer, int bufferSize, int romSize) :
    m_ramEnabled(0x00),
    m_romBank(0x01)
{
    // Max 256 kb of rom
    DEBUG_ASSERT_N(romSize <= 0x4'0000);

    if (romSize > 0x4'0000)
    {
        romSize = 0x4'0000;
    }

    // Always 512x4 bits of ram
    const int ramSize = 0x200;
    Initialize(cartridgeBuffer, bufferSize, romSize, ramSize);
}

MemoryBankController_MBC2::~MemoryBankController_MBC2()
{
}

BYTE MemoryBankController_MBC2::ReadMemory(WORD address)
{
    // All very similar to MBC1
    if (address < 0x4000)
    {
        DEBUG_ASSERT_N(address < m_romSize);
        if (address < m_romSize)
        {
            return m_rom[address];
        }
    }

    if (address >= 0x4000 && address < 0x8000)
    {
        MEMORY_ADDRESS romAddress = address - 0x4000 + (ROM_BANK_SIZE * m_romBank);
        DEBUG_ASSERT_N(romAddress < m_romSize);
        if (romAddress < m_romSize)
        {
            return m_rom[romAddress];
        }
    }

    // MBC2 doesn’t support external RAM, instead it includes 512 half - bytes of RAM (built into the MBC2 chip itself).
    // It still requires an external battery to save data during power - off though.
    // As the data consists of 4 - bit values, only the lower 4 bits of the bytes in this memory area are used.
    // The upper 4 bits of each byte are undefined and should not be relied upon.
    //
    // Only the bottom 9 bits of the address are used to index into the internal RAM, so RAM access repeats.
    if (address >= 0xA000 && address <= 0xC000)
    {
        DEBUG_ASSERT_N(m_ramEnabled);
        if (m_ramEnabled)
        {
            MEMORY_ADDRESS ramAddress = address - 0xA000;
            ramAddress &= 0x1FF;

            DEBUG_ASSERT_N(ramAddress < m_ramSize);
            if (ramAddress < m_ramSize)
            {
                return m_ram[ramAddress];
            }
        }
    }

    DEBUG_ASSERT(false, "Reading from invalid memory");
    return 0xFF;
}

void MemoryBankController_MBC2::WriteMemory(WORD address, BYTE data)
{
    // The least significant bit of the upper address byte must be zero to enable/disable cart RAM.
    if (address < 0x2000)
    {
        WORD invalidAddress = address & 0x100;
        if (!invalidAddress)
        {
            m_ramEnabled = data & 0x0A;
            return;
        }
    }

    // The least significant bit of the upper address byte must be one to select a ROM bank
    if (address >= 0x2000 && address < 0x4000)
    {
        data &= 0x0F;
        WORD validAddress = address & 0x100;
        if (validAddress)
        {
            m_romBank = data;
            return;
        }
    }

    // only the lower 4 bits of the bytes in this memory area are used
    if (address >= 0xA000 && address <= 0xC000)
    {
        DEBUG_ASSERT_N(m_ramEnabled);
        if (m_ramEnabled)
        {
            data &= 0x0F;

            MEMORY_ADDRESS ramAddress = address - 0xA000;
            ramAddress &= 0x1FF;

            DEBUG_ASSERT_N(ramAddress < m_ramSize);
            if (ramAddress < m_ramSize)
            {
                m_ram[ramAddress] = data;
            }
        }
    }

    DEBUG_ASSERT(false, "Writing to invalid memory");
}

//////////////////////////////////////////////////////////////////////////////////////

MemoryBankController_MBC3::MemoryBankController_MBC3(char* cartridgeBuffer, int bufferSize, int romSize, int ramSize) :
    m_ramAndTimerEnabled(0x00),
    m_romBank(0x01),
    m_isRTCMode(false),
    m_ramBank(0x00),
    m_rtc_s(0x00),
    m_rtc_m(0x00),
    m_rtc_h(0x00),
    m_rtc_dl(0x00),
    m_rtc_dh(0x00)
{
    // Max 2 mb of rom, 32 kb ram
    DEBUG_ASSERT_N(romSize <= 0x20'0000);
    DEBUG_ASSERT_N(ramSize <= 0x8000);

    if (romSize > 0x20'0000)
    {
        romSize = 0x20'0000;
    }

    if (ramSize > 0x8000)
    {
        ramSize = 0x8000;
    }

    Initialize(cartridgeBuffer, bufferSize, romSize, ramSize);
}

MemoryBankController_MBC3::~MemoryBankController_MBC3()
{
}

BYTE MemoryBankController_MBC3::ReadMemory(WORD address)
{
    if (address < 0x4000)
    {
        DEBUG_ASSERT_N(address < m_romSize);
        if (address < m_romSize)
        {
            return m_rom[address];
        }
    }

    if (address >= 0x4000 && address < 0x8000)
    {
        MEMORY_ADDRESS romAddress = address - 0x4000 + (ROM_BANK_SIZE * m_romBank);
        DEBUG_ASSERT_N(romAddress < m_romSize);
        if (romAddress < m_romSize)
        {
            return m_rom[romAddress];
        }
    }

    if (address >= 0xA000 && address <= 0xC000)
    {
        DEBUG_ASSERT_N(m_ramAndTimerEnabled);
        if (m_ramAndTimerEnabled)
        {
            if (!m_isRTCMode)
            {
                MEMORY_ADDRESS ramAddress = address - 0xA000;
                ramAddress = ramAddress + (RAM_BANK_SIZE * m_ramBank);
                DEBUG_ASSERT_N(ramAddress < m_ramSize);
                if (ramAddress < m_ramSize)
                {
                    return m_ram[ramAddress];
                }
            }
            else
            {
                switch (m_ramBank)
                {
                case 0x08:
                    return m_rtc_s;
                case 0x09:
                    return m_rtc_m;
                case 0x0A:
                    return m_rtc_h;
                case 0x0B:
                    return m_rtc_dl;
                case 0x0C:
                    return m_rtc_dh;
                }

                DEBUG_ASSERT(false, "Invalid byte");
            }
        }
    }

    DEBUG_ASSERT(false, "Reading from invalid memory");
    return 0xFF;
}

void MemoryBankController_MBC3::WriteMemory(WORD address, BYTE data)
{
    if (address < 0x2000)
    {
        m_ramAndTimerEnabled = data & 0x0A;
        return;
    }

    if (address >= 0x2000 && address < 0x4000)
    {
        data &= 0x7F;
        if (data == 0x00)
        {
            data = 0x01;
        }

        m_romBank &= 0x80;  // clear bits
        m_romBank |= data;  // set new bits
        return;
    }

    // As for the MBC1s RAM Banking Mode, writing a value in range for 00h-07h maps the corresponding external RAM Bank (if any) into memory at A000-BFFF.
    // When writing a value of 08h-0Ch, this will map the corresponding RTC register into memory at A000-BFFF.
    // That register could then be read/written by accessing any address in that area, typically that is done by using address A000.
    if (address >= 0x4000 && address < 0x6000)
    {
        m_ramBank = data;
        if (data < 0x08)
        {
            m_isRTCMode = false;
        }
        else if (data < 0x0C)
        {
            m_isRTCMode = true;
        }
        else
        {
            DEBUG_ASSERT(false, "Invalid byte");
            m_ramBank = 0x0;
        }
        return;
    }

    // When writing 00h, and then 01h to this register, the current time becomes latched into the RTC registers.
    // The latched data will not change until it becomes latched again, by repeating the write 00h->01h procedure.
    // This is supposed for <reading> from the RTC registers.
    // This can be proven by reading the latched(frozen) time from the RTC registers, and then unlatch the registers to show the clock itself continues to tick in background.
    if (address >= 0x6000 && address < 0x8000)
    {
        // TODO: ???
        return;
    }

    if (address >= 0xA000 && address <= 0xC000)
    {
        DEBUG_ASSERT_N(m_ramAndTimerEnabled);
        if (m_ramAndTimerEnabled)
        {
            if (!m_isRTCMode)
            {
                MEMORY_ADDRESS ramAddress = address - 0xA000;
                ramAddress = ramAddress + (RAM_BANK_SIZE * m_ramBank);
                DEBUG_ASSERT_N(ramAddress < m_ramSize);
                if (ramAddress < m_ramSize)
                {
                    m_ram[ramAddress] = data;
                }
            }
            else
            {
                switch (m_ramBank)
                {
                case 0x08:
                    m_rtc_s = data;
                    return;
                case 0x09:
                    m_rtc_m = data;
                    return;
                case 0x0A:
                    m_rtc_h = data;
                    return;
                case 0x0B:
                    m_rtc_dl = data;
                    return;
                case 0x0C:
                    m_rtc_dh = data;
                    return;
                }

                DEBUG_ASSERT(false, "Invalid byte");
            }
        }
    }

    DEBUG_ASSERT(false, "Writing to invalid memory");
}

//////////////////////////////////////////////////////////////////////////////////////

MemoryBankController_MBC5::MemoryBankController_MBC5(char* cartridgeBuffer, int bufferSize, int romSize, int ramSize) :
    m_romBank(0x0000),
    m_ramBank(0x00),
    m_ramEnabled(0x00)
{
    // Max 8 mb of rom, 128 kb ram
    DEBUG_ASSERT_N(romSize <= 0x80'0000);
    DEBUG_ASSERT_N(ramSize <= 0x2'0000);

    if (romSize > 0x80'0000)
    {
        romSize = 0x80'0000;
    }

    if (ramSize > 0x2'0000)
    {
        ramSize = 0x2'0000;
    }

    Initialize(cartridgeBuffer, bufferSize, romSize, ramSize);
}

MemoryBankController_MBC5::~MemoryBankController_MBC5()
{
}

BYTE MemoryBankController_MBC5::ReadMemory(WORD address)
{
    if (address < 0x4000)
    {
        DEBUG_ASSERT_N(address < m_romSize);
        if (address < m_romSize)
        {
            return m_rom[address];
        }
    }

    if (address >= 0x4000 && address < 0x8000)
    {
        MEMORY_ADDRESS romAddress = address - 0x4000 + (ROM_BANK_SIZE * m_romBank);
        DEBUG_ASSERT_N(romAddress < m_romSize);
        if (romAddress < m_romSize)
        {
            return m_rom[romAddress];
        }
    }

    if (address >= 0xA000 && address <= 0xC000)
    {
        DEBUG_ASSERT_N(m_ramEnabled);
        if (m_ramEnabled)
        {
            MEMORY_ADDRESS ramAddress = address - 0xA000;
            ramAddress = ramAddress + (RAM_BANK_SIZE * m_ramBank);

            DEBUG_ASSERT_N(ramAddress < m_ramSize);
            if (ramAddress < m_ramSize)
            {
                return m_ram[ramAddress];
            }
        }
    }

    DEBUG_ASSERT(false, "Reading from invalid memory");
    return 0xFF;
}

void MemoryBankController_MBC5::WriteMemory(WORD address, BYTE data)
{
    if (address < 0x2000)
    {
        m_ramEnabled = data & 0x0A;
        return;
    }

    if (address >= 0x2000 && address < 0x3000)
    {
        m_romBank &= 0x0100;  // clear bits
        m_romBank |= data;    // set new bits
        return;
    }

    if (address >= 0x3000 && address < 0x4000)
    {
        data &= 0x01;
        m_romBank &= 0x00FF;        // clear bits
        m_romBank |= (data << 8);   // set new bits
        return;
    }

    if (address >= 0x4000 && address < 0x6000)
    {
        data &= 0x0F;
        m_ramBank = data;
        return;
    }

    if (address >= 0xA000 && address <= 0xC000)
    {
        DEBUG_ASSERT_N(m_ramEnabled);
        if (m_ramEnabled)
        {
            MEMORY_ADDRESS ramAddress = address - 0xA000;
            ramAddress = ramAddress + (RAM_BANK_SIZE * m_ramBank);

            DEBUG_ASSERT_N(ramAddress < m_ramSize);
            if (ramAddress < m_ramSize)
            {
                m_ram[ramAddress] = data;
            }
        }

        return;
    }

    DEBUG_ASSERT(false, "Writing to invalid memory");
}

//////////////////////////////////////////////////////////////////////////////////////

MemoryBankController* MemoryBankControllerFactory::CreateMemoryBank(BYTE type, BYTE romSize, BYTE ramSize, char* cartridgeBuffer, int bufferLength)
{
    int actualRomSize = 0x8000 * (1 << romSize);
    int actualRamSize = 0;

    switch (ramSize)
    {
    case 0x00:
    case 0x01:
        actualRamSize = 0;
        break;
    case 0x02:
        actualRamSize = 0x2000;
        break;
    case 0x03:
        actualRamSize = 0x8000;
        break;
    case 0x04:
        actualRamSize = 0x2'0000;
        break;
    case 0x05:
        actualRamSize = 0x1'0000;
        break;
    }

    MemoryBankController* mbc = nullptr;
    switch (type)
    {
    case 0x00:
        actualRamSize = 0;
        [[fallthrough]];
    case 0x08:
    case 0x09:
        mbc = new MemoryBankController_None(cartridgeBuffer, bufferLength, actualRomSize, actualRamSize);
        break;
    case 0x01:
        actualRamSize = 0;
        [[fallthrough]];
    case 0x02:
    case 0x03:
        mbc = new MemoryBankController_MBC1(cartridgeBuffer, bufferLength, actualRomSize, actualRamSize);
        break;
    case 0x05:
    case 0x06:
        mbc = new MemoryBankController_MBC2(cartridgeBuffer, bufferLength, actualRomSize);
        break;
    case 0x0F:
    case 0x11:
        actualRamSize = 0;
        [[fallthrough]];
    case 0x10:
    case 0x12:
    case 0x13:
        mbc = new MemoryBankController_MBC3(cartridgeBuffer, bufferLength, actualRomSize, actualRamSize);
        break;
    case 0x19:
    case 0x1C:
        actualRamSize = 0;
        [[fallthrough]];
    case 0x1A:
    case 0x1B:
    case 0x1D:
    case 0x1E:
        mbc = new MemoryBankController_MBC5(cartridgeBuffer, bufferLength, actualRomSize, actualRamSize);
        break;
    }

    return mbc;
}
