#include "stdafx.h"

#include <iomanip>
#include <sstream>
#include <imgui.h>

#include "header/CPU_Debug.h"
#include "header/Cartridge.h"
#include "header/CPU.h"
#include "header/PPU.h"

uint64_t CPU_DEBUG::s_cycles = 0;

static CPU_DEBUG::Opcodes s_opcodes[256] =
{
    { 0, "NOP" },		    	// 0x00
    { 2, "LD\tBC, %04X" },		// 0x01
    { 0, "LD\tBC, A" }, 		// 0x02
    { 0, "INC\tBC" },   		// 0x03
    { 0, "INC\tB" },			// 0x04
    { 0, "DEC\tB" },			// 0x05
    { 1, "LD\tB, %02X" },		// 0x06
    { 0, "RLCA" },			    // 0x07
    { 2, "LD\t%04X, SP" },		// 0x08
    { 0, "ADD\tHL, BC" },		// 0x09
    { 0, "LD\tA, (BC)" },		// 0x0a
    { 0, "DEC\tBC" },   		// 0x0b
    { 0, "INC\tC" },			// 0x0c
    { 0, "DEC\tC" },			// 0x0d
    { 1, "LD\tC, %02X" },		// 0x0e
    { 0, "RRCA" },			    // 0x0f
    { 1, "STOP" },			    // 0x10
    { 2, "LD\tDE, %04X" },		// 0x11
    { 0, "LD\t(DE), A" },		// 0x12
    { 0, "INC\tDE" },       	// 0x13
    { 0, "INC\tD" },			// 0x14
    { 0, "DEC\tD" },			// 0x15
    { 1, "LD\tD, %02X" },		// 0x16
    { 0, "RLA" },		    	// 0x17
    { 1, "JR\t%04X" },  		// 0x18
    { 0, "ADD\tHL, DE" },		// 0x19
    { 0, "LD\tA, (DE)" },		// 0x1a
    { 0, "DEC\tDE" },   		// 0x1b
    { 0, "INC\tE" },			// 0x1c
    { 0, "DEC\tE" },			// 0x1d
    { 1, "LD\tE, %02X" },		// 0x1e
    { 0, "RRA" },   			// 0x1f
    { 1, "JR\tNZ, %04X" },		// 0x20
    { 2, "LD\tHL, %04X" },	    // 0x21
    { 0, "LD\t(HL+), A" },		// 0x22
    { 0, "INC\tHL" },	    	// 0x23
    { 0, "INC\tH" },			// 0x24
    { 0, "DEC\tH" },			// 0x25
    { 1, "LD\tH, %02X" },		// 0x26
    { 0, "DAA" },   			// 0x27
    { 1, "JR\tZ, %04X" },		// 0x28
    { 0, "ADD\tHL, HL" },		// 0x29
    { 0, "LD\tA, (HL+)" },		// 0x2a
    { 0, "DEC\tHL" },   		// 0x2b
    { 0, "INC\tL" },			// 0x2c
    { 0, "DEC\tL" },			// 0x2d
    { 1, "LD\tL, %02X" },		// 0x2e
    { 0, "CPL" },   			// 0x2f
    { 1, "JR\tNC, %04X" },		// 0x30
    { 2, "LD\tSP, %04X" },		// 0x31
    { 0, "LD\t(HL-), A" },		// 0x32
    { 0, "INC\tSP" },	    	// 0x33
    { 0, "INC\t(HL)" }, 		// 0x34
    { 0, "DEC\t(HL)" }, 		// 0x35
    { 1, "LD\t(HL), %02X" },	// 0x36
    { 0, "SCF" },   			// 0x37
    { 1, "JR\tC, %04X" },		// 0x38
    { 0, "ADD\tHL, SP" },		// 0x39
    { 0, "LD\tA, (HL-)" },		// 0x3a
    { 0, "DEC\tSP" },   		// 0x3b
    { 0, "INC\tA" },			// 0x3c
    { 0, "DEC\tA" },			// 0x3d
    { 1, "LD\tA, %02X" },		// 0x3e
    { 0, "CCF" },		    	// 0x3f
    { 0, "LD\tB, B" },	    	// 0x40
    { 0, "LD\tB, C" },	    	// 0x41
    { 0, "LD\tB, D" },	    	// 0x42
    { 0, "LD\tB, E" },	    	// 0x43
    { 0, "LD\tB, H" },	    	// 0x44
    { 0, "LD\tB, L" },	    	// 0x45
    { 0, "LD\tB, (HL)" },		// 0x46
    { 0, "LD\tB, A" },		    // 0x47
    { 0, "LD\tC, B" },		    // 0x48
    { 0, "LD\tC, C" },		    // 0x49
    { 0, "LD\tC, D" },		    // 0x4a
    { 0, "LD\tC, E" },		    // 0x4b
    { 0, "LD\tC, H" },		    // 0x4c
    { 0, "LD\tC, L" },		    // 0x4d
    { 0, "LD\tC, (HL)" },		// 0x4e
    { 0, "LD\tC, A" },  		// 0x4f
    { 0, "LD\tD, B" },  		// 0x50
    { 0, "LD\tD, C" },  		// 0x51
    { 0, "LD\tD, D" },  		// 0x52
    { 0, "LD\tD, E" },  		// 0x53
    { 0, "LD\tD, H" },  		// 0x54
    { 0, "LD\tD, L" },  		// 0x55
    { 0, "LD\tD, (HL)" },		// 0x56
    { 0, "LD\tD, A" },  		// 0x57
    { 0, "LD\tE, B" },  		// 0x58
    { 0, "LD\tE, C" },  		// 0x59
    { 0, "LD\tE, D" },  		// 0x5a
    { 0, "LD\tE, E" },  		// 0x5b
    { 0, "LD\tE, H" },  		// 0x5c
    { 0, "LD\tE, L" },  		// 0x5d
    { 0, "LD\tE, (HL)" },		// 0x5e
    { 0, "LD\tE, A" },  		// 0x5f
    { 0, "LD\tH, B" },  		// 0x60
    { 0, "LD\tH, C" },  		// 0x61
    { 0, "LD\tH, D" },  		// 0x62
    { 0, "LD\tH, E" },  		// 0x63
    { 0, "LD\tH, H" },  		// 0x64
    { 0, "LD\tH, L" },  		// 0x65
    { 0, "LD\tH, (HL)" },		// 0x66
    { 0, "LD\tH, A" },  		// 0x67
    { 0, "LD\tL, B" },  		// 0x68
    { 0, "LD\tL, C" },  		// 0x69
    { 0, "LD\tL, D" },  		// 0x6a
    { 0, "LD\tL, E" },  		// 0x6b
    { 0, "LD\tL, H" },  		// 0x6c
    { 0, "LD\tL, L" },  		// 0x6d
    { 0, "LD\tL, (HL)" },		// 0x6e
    { 0, "LD\tL, A" },  		// 0x6f
    { 0, "LD\t(HL), B" },  		// 0x70
    { 0, "LD\t(HL), C" },  		// 0x71
    { 0, "LD\t(HL), D" },  		// 0x72
    { 0, "LD\t(HL), E" },  		// 0x73
    { 0, "LD\t(HL), H" },  		// 0x74
    { 0, "LD\t(HL), L" },  		// 0x75
    { 0, "HALT" },	    		// 0x76
    { 0, "LD\t(HL), A" },		// 0x77
    { 0, "LD\tA, B" },	    	// 0x78
    { 0, "LD\tA, C" },	    	// 0x79
    { 0, "LD\tA, D" },	    	// 0x7a
    { 0, "LD\tA, E" },	    	// 0x7b
    { 0, "LD\tA, H" },	    	// 0x7c
    { 0, "LD\tA, L" },	    	// 0x7d
    { 0, "LD\tA, (HL)" },		// 0x7e
    { 0, "LD\tA, A" },  		// 0x7f
    { 0, "ADD\tB" },	    	// 0x80
    { 0, "ADD\tC" },	    	// 0x81
    { 0, "ADD\tD" },	    	// 0x82
    { 0, "ADD\tE" },	    	// 0x83
    { 0, "ADD\tH" },	    	// 0x84
    { 0, "ADD\tL" },	    	// 0x85
    { 0, "ADD\t(HL)" }, 		// 0x86
    { 0, "ADD\tA" },			// 0x87
    { 0, "ADC\tB" },			// 0x88
    { 0, "ADC\tC" },			// 0x89
    { 0, "ADC\tD" },			// 0x8a
    { 0, "ADC\tE" },			// 0x8b
    { 0, "ADC\tH" },			// 0x8c
    { 0, "ADC\tL" },			// 0x8d
    { 0, "ADC\t(HL)" },   		// 0x8e
    { 0, "ADC\tA" },			// 0x8f
    { 0, "SUB\tB" },			// 0x90
    { 0, "SUB\tC" },			// 0x91
    { 0, "SUB\tD" },			// 0x92
    { 0, "SUB\tE" },			// 0x93
    { 0, "SUB\tH" },			// 0x94
    { 0, "SUB\tL" },			// 0x95
    { 0, "SUB\t(HL)" }, 		// 0x96
    { 0, "SUB\tA" },			// 0x97
    { 0, "SBC\tB" },			// 0x98
    { 0, "SBC\tC" },			// 0x99
    { 0, "SBC\tD" },			// 0x9a
    { 0, "SBC\tE" },			// 0x9b
    { 0, "SBC\tH" },			// 0x9c
    { 0, "SBC\tL" },			// 0x9d
    { 0, "SBC\t(HL)" }, 		// 0x9e
    { 0, "SBC\tA" },			// 0x9f
    { 0, "AND\tB" },			// 0xa0
    { 0, "AND\tC" },			// 0xa1
    { 0, "AND\tD" },			// 0xa2
    { 0, "AND\tE" },			// 0xa3
    { 0, "AND\tH" },			// 0xa4
    { 0, "AND\tL" },			// 0xa5
    { 0, "AND\t(HL)" },	    	// 0xa6
    { 0, "AND\tA" },			// 0xa7
    { 0, "XOR\tB" },			// 0xa8
    { 0, "XOR\tC" },			// 0xa9
    { 0, "XOR\tD" },			// 0xaa
    { 0, "XOR\tE" },			// 0xab
    { 0, "XOR\tH" },			// 0xac
    { 0, "XOR\tL" },			// 0xad
    { 0, "XOR\t(HL)" }, 		// 0xae
    { 0, "XOR\tA" },			// 0xaf
    { 0, "OR\tB" },			    // 0xb0
    { 0, "OR\tC" },			    // 0xb1
    { 0, "OR\tD" },			    // 0xb2
    { 0, "OR\tE" },			    // 0xb3
    { 0, "OR\tH" },			    // 0xb4
    { 0, "OR\tL" },			    // 0xb5
    { 0, "OR\t(HL)" },			// 0xb6
    { 0, "OR\tA" },			    // 0xb7
    { 0, "CP\tB" },			    // 0xb8
    { 0, "CP\tC" },			    // 0xb9
    { 0, "CP\tD" },			    // 0xba
    { 0, "CP\tE" },			    // 0xbb
    { 0, "CP\tH" },			    // 0xbc
    { 0, "CP\tL" },			    // 0xbd
    { 0, "CP\t(HL)" },			// 0xbe
    { 0, "CP\tA" },		    	// 0xbf
    { 0, "RET\tNZ" },	    	// 0xc0
    { 0, "POP\tBC" },	    	// 0xc1
    { 2, "JP\tNZ, %04X" },		// 0xc2
    { 2, "JP\t%04X" },			// 0xc3
    { 2, "CALL\tNZ, %04X" },	// 0xc4
    { 0, "PUSH\tBC" },	    	// 0xc5
    { 1, "ADD\t%02X" }, 		// 0xc6
    { 0, "RST\t00" },		    // 0xc7
    { 0, "RET\tZ" },			// 0xc8
    { 0, "RET" },		    	// 0xc9
    { 2, "JP\tZ, %04X" },		// 0xca
    { 1, "CB, %02X" },			// 0xcb
    { 2, "CALL\tZ, %04X" },		// 0xcc
    { 2, "CALL\t%04X" },		// 0xcd
    { 1, "ADC\t%02X" }, 		// 0xce
    { 0, "RST\t08" },   		// 0xcf
    { 0, "RET\tNC" },   		// 0xd0
    { 0, "POP\tDE" },   		// 0xd1
    { 2, "JP\tNC, %04X" },		// 0xd2
    { 0, "NOP" },   			// 0xd3
    { 2, "CALL\tNC, %04X" },	// 0xd4
    { 0, "PUSH\tDE" },		    // 0xd5
    { 1, "SUB\t%02X" },		    // 0xd6
    { 0, "RST\t10" },		    // 0xd7
    { 0, "RET\tC" },		    // 0xd8
    { 0, "RETI" },			    // 0xd9
    { 2, "JP\tC, %04X" },		// 0xda
    { 0, "NOP" },		    	// 0xdb
    { 2, "CALL\tC, %04X" },		// 0xdc
    { 0, "NOP" },	    		// 0xdd
    { 1, "SBC\t%02X" },		    // 0xde
    { 0, "RST\t18" },		    // 0xdf
    { 1, "LD\t(FF00+%02X), A" },// 0xe0
    { 0, "POP\tHL" },   		// 0xe1
    { 0, "LD\t(FF00+C), A" },	// 0xe2
    { 0, "NOP" },   			// 0xe3
    { 0, "NOP" },   			// 0xe4
    { 0, "PUSH\tHL" },	    	// 0xe5
    { 1, "AND\t%02X" }, 		// 0xe6
    { 0, "RST\t20" },		    // 0xe7
    { 1, "ADD\tSP, %02X" },		// 0xe8
    { 0, "JP\tHL" },			// 0xe9
    { 2, "LD\t(%04X), A" },		// 0xea
    { 0, "NOP" },   			// 0xeb
    { 0, "NOP" },   			// 0xec
    { 0, "NOP" },   			// 0xed
    { 1, "XOR\t%02X" }, 		// 0xee
    { 0, "RST\t28" },   		// 0xef
    { 1, "LD\tA, (FF00+%02X)" },// 0xf0
    { 0, "POP\tAF" },		    // 0xf1
    { 0, "LD\tA, (FF00+C)" },	// 0xf2
    { 0, "DI" },			    // 0xf3
    { 0 ,"NOP" },			    // 0xf4
    { 0, "PUSH\tAF" },		    // 0xf5
    { 1, "OR\t%02X" },  		// 0xf6
    { 0, "RST\t30" },   		// 0xf7
    { 1, "LD\tHL, SP+%02X" },	// 0xf8
    { 0, "LD\tSP, HL" },		// 0xf9
    { 2, "LD\tA, (%04X)" },		// 0xfa
    { 0, "EI" },		    	// 0xfb
    { 0, "NOP" },		    	// 0xfc
    { 0, "NOP" },		    	// 0xfd
    { 1, "CP\t%02X" },	    	// 0xfe
    { 0, "RST\t38" },	    	// 0xff
};

std::string CPU_DEBUG::GetDebugInfo(const CPU* sm83)
{
    std::stringstream debugInfo;

    debugInfo << "Register AF: " << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << sm83->AF.pair << std::endl;
    debugInfo << "Register BC: " << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << sm83->BC.pair << std::endl;
    debugInfo << "Register DE: " << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << sm83->DE.pair << std::endl;
    debugInfo << "Register HL: " << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << sm83->HL.pair << std::endl;
    debugInfo << "Register SP: " << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << sm83->SP.pair << std::endl;
    debugInfo << "Register PC: " << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << sm83->PC.pair << std::endl;
    debugInfo << std::endl;

    debugInfo << GetOpcodeDebugInfo(sm83);
    return debugInfo.str();
}

std::string CPU_DEBUG::GetDebugInfo(const Cartridge* cart)
{
    BYTE* rom = nullptr;
    int romSize = cart->DumpRom(rom);

    std::stringstream debugInfo;
    if (rom)
    {
        for (int i = 0; i < romSize; i++)
        {
            if ((i & 0xF) == 0x00)
            {
                debugInfo << std::endl << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << i << "\t|\t";
            }

            debugInfo << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << (WORD)rom[i] << "\t";
        }
    }

    return debugInfo.str();
}

void CPU_DEBUG::ImGui_PPU_Registers(const CPU* sm83)
{
    ImGui::SeparatorText("PPU");

    WORD lcdc = sm83->m_io[PPU::LCDC_BYTE];
    WORD scrollY = sm83->m_io[PPU::SCROLL_Y_BYTE];
    WORD scrollX = sm83->m_io[PPU::SCROLL_X_BYTE];
    WORD lcdcY = sm83->m_io[PPU::LCDC_Y_BYTE];

    std::stringstream debugRegister;
    debugRegister << "LCDC:     " << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << lcdc << std::endl;
    debugRegister << "SCROLL Y: " << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << scrollY << std::endl;
    debugRegister << "SCROLL X: " << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << scrollX << std::endl;
    debugRegister << "LCDC Y:   " << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << lcdcY << std::endl;

    ImGui::Text(debugRegister.str().c_str());
}

void CPU_DEBUG::ImGui_Registers(const CPU* sm83)
{
    ImGui::SeparatorText("Registers");
    ImVec2 availRegion = ImGui::GetContentRegionAvail();

    {
        ImGui::BeginChild("Registers", ImVec2(availRegion.x * 0.6f, 140));

        std::stringstream debugRegister;
        debugRegister << "AF: " << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << sm83->AF.pair << std::endl;
        debugRegister << "BC: " << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << sm83->BC.pair << std::endl;
        debugRegister << "DE: " << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << sm83->DE.pair << std::endl;
        debugRegister << "HL: " << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << sm83->HL.pair << std::endl;
        debugRegister << "SP: " << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << sm83->SP.pair << std::endl;
        debugRegister << "PC: " << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << sm83->PC.pair << std::endl;

        ImGui::Text(debugRegister.str().c_str());
        ImGui::EndChild();
    }

    ImGui::SameLine();

    {
        ImGui::BeginChild("Flags", ImVec2(0, 140));

        bool zFlag = sm83->IsFlagSet(CPU::FLAG_Z);
        bool nFlag = sm83->IsFlagSet(CPU::FLAG_N);
        bool hFlag = sm83->IsFlagSet(CPU::FLAG_H);
        bool cFlag = sm83->IsFlagSet(CPU::FLAG_C);

        ImGui::Checkbox("z", &zFlag);
        ImGui::Checkbox("n", &nFlag);
        ImGui::Checkbox("h", &hFlag);
        ImGui::Checkbox("c", &cFlag);
        ImGui::EndChild();
    }

    ImGui::Text(GetOpcodeDebugInfo(sm83).c_str());
}

std::string CPU_DEBUG::GetOpcodeDebugInfo(const CPU* sm83)
{
    std::stringstream debugInfo;

    WORD pc = sm83->PC.pair;

    BYTE opcode = sm83->Read(pc);
    BYTE operand0 = sm83->Read(pc+1);
    BYTE operand1 = sm83->Read(pc+2);

    WORD operand = (operand1 << 8) | operand0;

    Opcodes debugOpcode = s_opcodes[opcode];
    if (debugOpcode.operands == 0)
    {
        debugInfo << debugOpcode.name;
        return debugInfo.str();
    }

    if (debugOpcode.operands == 1)
    {
        // JR opcodes use WORD to represent where the PC will be rather than the actual operand
        if (opcode == 0x18 || opcode == 0x20 || opcode == 0x28 || opcode == 0x30 || opcode == 0x38)
        {
            operand = pc + ((SIGNED_BYTE)operand0) + 2;
        }
        else
        {
            operand = (WORD)operand0;
        }
    }

    char opcodeName[32];
    sprintf_s(opcodeName, sizeof(char) * 32, debugOpcode.name, operand);
    debugInfo << opcodeName;

    return debugInfo.str();
}
