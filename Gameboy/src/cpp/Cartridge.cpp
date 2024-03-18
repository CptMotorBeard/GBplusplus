#include "stdafx.h"

#include <fstream>

#include "header/Cartridge.h"
#include "header/MemoryBankControllers.h"

Cartridge::Cartridge(std::string filePath)
	: m_mbc(nullptr)
{
	std::ifstream in(filePath, std::ios_base::in | std::ios_base::binary);

	int length;
	in.seekg(0, std::ios::end);
	length = in.tellg();
	in.seekg(0, std::ios::beg);

	char* buffer = new char[length];
	in.read(buffer, length);
	in.close();

	CartridgeHeader* header = reinterpret_cast<CartridgeHeader *>(&buffer[0x100]);
	m_mbc = MemoryBankControllerFactory::CreateMemoryBank(header->cartridgeType, header->romSize, header->ramSize, buffer, length);

	m_title = std::string(reinterpret_cast<char *>(header->titleSection.title));

	delete[] buffer;
}

Cartridge::~Cartridge()
{
	delete m_mbc;
}

BYTE Cartridge::ReadMemory(WORD address)
{
	return m_mbc->ReadMemory(address);
}

void Cartridge::WriteMemory(WORD address, BYTE data)
{
	m_mbc->WriteMemory(address, data);
}

int Cartridge::DumpRom(BYTE*& rom) const
{
	return m_mbc->DumpRom(rom);
}
