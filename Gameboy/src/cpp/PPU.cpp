#include "stdafx.h"
#include "header/PPU.h"
#include "header/CPU.h"

PPU::PPU() :
	m_sm83(nullptr),
	m_ioMemory(nullptr),
	m_gpuMode(Mode::OAMLOAD),
	m_gpuClock(0),
	m_currentLinePixels()
{
}

PPU::~PPU()
{
}

void PPU::Initialize(CPU* sm83, BYTE* ioMemory)
{
	m_sm83 = sm83;
	m_ioMemory = ioMemory;
}

void PPU::Step(int clockCycles)
{
	m_gpuClock += clockCycles;
	switch (m_gpuMode)
	{
	case Mode::HBLANK:
		if (m_gpuClock >= HBLANK_CYCLES)
		{
			m_gpuClock = 0;

			// cleanLine();
			BYTE y = ++m_ioMemory[LCDC_Y_BYTE];

			if (y >= VBLANK_START)
			{
				m_gpuMode = Mode::VBLANK;

				// Trigger a VBLANK interrupt after rendering the image
				if (m_sm83->IsInterruptEnabled(CPU::INTERRUPT_VBLANK))
				{
					m_sm83->RequestInterrupt(CPU::INTERRUPT_VBLANK);
				}
			}
			else
			{
				// If we aren't at a VBLANK yet we restart the process
				m_gpuMode = Mode::OAMLOAD;
			}
		}
		break;
	case Mode::VBLANK:
		if (m_gpuClock >= VBLANK_CYCLES)
		{
			m_gpuClock = 0;
			BYTE y = ++m_ioMemory[LCDC_Y_BYTE];

			if (y > VBLANK_END)
			{
				// Restart
				m_gpuMode = Mode::OAMLOAD;
				m_ioMemory[LCDC_Y_BYTE] = 0;
			}
		}
		break;
	case Mode::OAMLOAD:
		if (m_gpuClock >= OAMLOAD_CYCLES)
		{
			m_gpuClock = 0;
			m_gpuMode = Mode::LCD;

			// processLine();
		}
		break;
	case Mode::LCD:
		if (m_gpuClock >= LCD_CYCLES)
		{
			m_gpuClock = 0;
			m_gpuMode = Mode::HBLANK;

			// Bit 7 tells us if we need to render
			if (m_ioMemory[LCDC_BYTE] & BIT_7)
			{
				// renderScanline();
			}

			// Trigger an LCD interrupt after rendering the line
			if (m_sm83->IsInterruptEnabled(CPU::INTERRUPT_LCD))
			{
				m_sm83->RequestInterrupt(CPU::INTERRUPT_LCD);
			}
		}
		break;
	}
}

bool PPU::SpriteOAM::IsAboveBackground() const
{
	return options & BIT_7;
}

bool PPU::SpriteOAM::ShouldFlipY() const
{
	return options & BIT_6;
}

bool PPU::SpriteOAM::ShouldFlipX() const
{
	return options & BIT_5;
}

bool PPU::SpriteOAM::UseObjectPalette0() const
{
	return options & BIT_4;
}