#include "stdafx.h"

#include <iomanip>

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>

#include "header/PPU.h"
#include "header/CPU.h"

PPU::PPU(sf::RenderTarget* screen) :
	m_sm83(nullptr),
	m_ioMemory(nullptr),
	m_oamMemory(nullptr),
	m_gpuClock(0),
	m_screenPixelBuffer(),
	m_displayTexture(),
	m_screen(screen),
	m_scanline(),
	m_vram(),
	m_mode(GPUMode::OAMLOAD)
{
	m_screenPixelBuffer.create(SCREEN_WIDTH, SCREEN_HEIGHT);
	m_displayTexture.create(SCREEN_WIDTH, SCREEN_HEIGHT);
	m_scanline.resize(SCREEN_WIDTH);

	m_vram = new BYTE[0x2000];
}

PPU::~PPU()
{
	delete[] m_vram;
}

void PPU::Initialize(CPU* sm83, BYTE* ioMemory, BYTE* oamMemory)
{
	m_sm83 = sm83;
	m_ioMemory = ioMemory;
	m_oamMemory = oamMemory;
}

void PPU::Step(int clockCycles)
{
	m_gpuClock += clockCycles;

	bool lcdEnabled = m_ioMemory[LCDC_BYTE] & LCD_ENABLE;
	switch (m_mode)
	{
	case GPUMode::HBLANK:
		if (m_gpuClock >= HBLANK_CYCLES)
		{
			m_gpuClock = 0;

			BYTE y = ++m_ioMemory[LCDC_Y_BYTE];
			if (y >= VBLANK_START)
			{
				m_mode = GPUMode::VBLANK;

				// Trigger a VBLANK interrupt after rendering the image
				if (m_sm83->IsInterruptEnabled(CPU::INTERRUPT_VBLANK))
				{
					m_sm83->RequestInterrupt(CPU::INTERRUPT_VBLANK);
				}
			}
			else
			{
				// If we aren't at a VBLANK yet we restart the process
				m_mode = GPUMode::OAMLOAD;
			}
		}
		break;
	case GPUMode::VBLANK:
		if (m_gpuClock >= VBLANK_CYCLES)
		{
			m_gpuClock = 0;
			BYTE y = ++m_ioMemory[LCDC_Y_BYTE];

			if (y > VBLANK_END)
			{
				// Restart
				m_mode = GPUMode::OAMLOAD;
				m_ioMemory[LCDC_Y_BYTE] = 0;
			}
		}
		break;
	case GPUMode::OAMLOAD:
		if (m_gpuClock >= OAMLOAD_CYCLES)
		{
			m_gpuClock = 0;
			m_mode = GPUMode::DRAWING;

			if (lcdEnabled)
			{
				ProcessScanline();
			}
		}
		break;
	case GPUMode::DRAWING:
		if (m_gpuClock >= LCD_CYCLES)
		{
			m_gpuClock = 0;
			m_mode = GPUMode::HBLANK;

			RenderScanline();
			
			// Trigger an LCD interrupt after rendering the line
			if (m_sm83->IsInterruptEnabled(CPU::INTERRUPT_LCD))
			{
				m_sm83->RequestInterrupt(CPU::INTERRUPT_LCD);
			}
		}
		break;
	}
}

void PPU::DrawToScreen()
{
	m_displayTexture.update(m_screenPixelBuffer);

	sf::Vector2 screenSize = m_screen->getView().getSize();
	sf::Sprite drawSprite(m_displayTexture);
	drawSprite.setScale(
		screenSize.x / SCREEN_WIDTH,
		screenSize.y / SCREEN_HEIGHT
	);

	m_screen->draw(drawSprite);
}

BYTE PPU::ReadVRAM(WORD address) const
{
	if (address < 0x2000)
	{
		return m_vram[address];
	}
	return 0xFF;
}

void PPU::WriteVRAM(WORD address, BYTE data)
{
	if (address < 0x2000)
	{
		m_vram[address] = data;
	}
}

void PPU::RenderScanline()
{
	BYTE currentLine = m_ioMemory[LCDC_Y_BYTE];
	for (int x = 0; x < SCREEN_WIDTH; x++)
	{
		m_screenPixelBuffer.setPixel(x, currentLine, m_colourPalette[(m_scanline[x] >> 4) & 0x3]);
	}
}

void PPU::DumpTiles(sf::RenderTarget& renderWindow)
{
	BYTE LCDC = m_ioMemory[LCDC_BYTE];

	if (!(LCDC & LCD_ENABLE))
	{
		return;
	}

	sf::Image im;
	im.create(32 * 8, 32 * 8);
	sf::Texture texture;
	texture.create(32 * 8, 32 * 8);

	for (int currentYPosition = 0; currentYPosition < (32 * 8); currentYPosition++)
	{
		WORD bgTileMapAddress = 0x1800 + ((currentYPosition / 8) * 32);
		for (int x = 0; x < 32; x++)
		{
			BYTE tileAddress = m_vram[bgTileMapAddress];
			WORD address = (tileAddress * 0x10) + ((currentYPosition % 8) * 2);

			BYTE upperTileBits = m_vram[address];
			BYTE lowerTileBits = m_vram[address + 1];

			for (int currentXPosition = 7; currentXPosition >= 0; currentXPosition--)
			{
				BYTE pixel = ((upperTileBits >> currentXPosition) & 0x1) + ((lowerTileBits >> currentXPosition) & 0x1) * 2;
				BYTE palette = (m_ioMemory[PALETTE_DATA] >> (pixel * 2)) & (BIT_0 | BIT_1);

				im.setPixel(x * 8 + (7 - currentXPosition), currentYPosition, m_colourPalette[palette]);
			}

			++bgTileMapAddress;
		}
	}

	texture.update(im);
	renderWindow.draw(sf::Sprite(texture));
}

void PPU::ProcessScanline()
{
	ProcessBackgroundLayer();
	// ProcessWindowLayer();
	// ProcessSpriteLayer();
}

void PPU::ProcessBackgroundLayer()
{
	BYTE LCDC = m_ioMemory[LCDC_BYTE];
	BYTE scrollX = m_ioMemory[SCROLL_X_BYTE];
	BYTE scrollY = m_ioMemory[SCROLL_Y_BYTE];
	BYTE currentLine = m_ioMemory[LCDC_Y_BYTE];

	WORD bgTileMapAddress = 0x1800 + ((currentLine / 8) * 32);
	for (int x = 0; x < 20; x++)
	{
		BYTE tileAddress = m_vram[bgTileMapAddress + x];
		WORD address = (tileAddress * 0x10) + ((currentLine % 8) * 2);

		BYTE upperTileBits = m_vram[address];
		BYTE lowerTileBits = m_vram[address + 1];

		for (int currentXPosition = 7; currentXPosition >= 0; currentXPosition--)
		{
			BYTE pixel = ((upperTileBits >> currentXPosition) & 0x1) + ((lowerTileBits >> currentXPosition) & 0x1) * 2;
			BYTE palette = (m_ioMemory[PALETTE_DATA] >> (pixel * 2)) & (BIT_0 | BIT_1);

			m_scanline[x * 8 + (7 - currentXPosition)] = (palette & 0x3) << 4 | pixel & 0x3;
		}
	}
}

void PPU::ProcessWindowLayer()
{
	BYTE LCDC = m_ioMemory[LCDC_BYTE];
	BYTE winY = m_ioMemory[WINDOW_X]; // 0 <= WY <= 143
	BYTE winX = m_ioMemory[WINDOW_Y]; // 7 <= WX <= 166. A value between 0-6 should not be allowed for WX

	if (winX < 7)
	{
		winX = 7;
	}
	winX -= 7;

	BYTE currentLine = m_ioMemory[LCDC_Y_BYTE];

	// Our window is only drawn within the bounds of winY, winX
	if (currentLine < winY)
	{
		return;
	}

	// Bit 0 and bit 5 tells us if we need to draw the window layer, if they're not enabled we leave
	if (!((LCDC & BIT_0) || (LCDC & BIT_5)))
	{
		return;
	}

	// Bit 6 of LCDC tells us the address range for the Window Tile Map
	WORD windowTileMapAddress = 0x1800 + ((LCDC >> 6 & 0x1) * 0x400);

	/*
	  We need our current screen x and y position, handled by scrollX and scrollY
	  Our tiles are 8x8 pixels. scroll is a pixel location, so scroll % 8 gives us our tiles positions
	  The upper left of our screen (which is where we start) is set by 7 - scrollX
	*/
	SIGNED_BYTE currentXPosition = 7;
	BYTE currentYPosition = currentLine % 8;

	BYTE pixelsWritten = 0;
	while (pixelsWritten < SCREEN_WIDTH)
	{
		// Do nothing while we pass through invalid draw zones
		if (pixelsWritten < winX)
		{
			pixelsWritten++;
			currentXPosition--;

			if (currentXPosition < 0)
			{
				currentXPosition = 7;
				windowTileMapAddress++;
			}
		}
		else
		{
			// Get the address of our tile from the tileSet
			BYTE tileAddress = m_vram[windowTileMapAddress];

			BYTE upperTileBits;
			BYTE lowerTileBits;

			// Bit 4 of LCDC tells us where and how to find our tile pattern
			/*
			  Tile patterns are
			  taken from the Tile Data Table located either at
			  $8000-8FFF or $8800-97FF. In the first case, patterns
			  are numbered with unsigned numbers from 0 to 255 (i.e.
			  pattern #0 lies at address $8000). In the second case,
			  patterns have signed numbers from -128 to 127 (i.e.
			  pattern #0 lies at address $9000).
			*/

			bool isFirstPattern = (LCDC >> 4 & 0x1);
			WORD address = 0;

			/*
			  The tileAddress we get from our bgTileMap are ordered from 0x00 - 0xFF. In memory, this address corresponds to a value 0x10 times bigger
			  0x8000 would be tileAddress 0x00, 0x8010 would be tile address 0x01 and so on. The reason for this is that there is 16 bytes for each tile (0x10)
			  and 2 bytes for each row of pixels, which gives us our 8x8 tiles.

			  The currentYPosition will tell us which row of pixels we are currently looking at, we multiply by 2 since there are 2 bytes per row.
			*/
			if (isFirstPattern)
			{
				address = (tileAddress * 0x10) + (currentYPosition * 2);
			}
			else
			{
				address = 0x1000 + (((SIGNED_BYTE)tileAddress) * 0x10) + (currentYPosition * 2);
			}

			// We need a total of 2 bytes for each pixel row in our tiles
			upperTileBits = m_vram[address];
			lowerTileBits = m_vram[address + 1];

			// Each time this loops, a new tile is written. The entire screen is 20 8x8 tiles wide.
			while (currentXPosition >= 0)
			{
				/*
				  Tile data is stored in 16 bytes where every 2 bytes represents a line in the tile
				  The values inside these 2 bytes represents the colour of each pixel in the 8x8 tile
				  The data of 2 lines translates into one line of pixels of various colours
					upper bits: 010001  ->  030021
					lower bits: 010010
				  We receive a value between 0 and 3 for our pixel.
				*/

				BYTE pixel = ((upperTileBits >> currentXPosition) & 0x1) + ((lowerTileBits >> currentXPosition) & 0x1) * 2;
				BYTE palette = (m_ioMemory[PALETTE_DATA] >> (pixel * 2)) & (BIT_0 | BIT_1);
				m_scanline[pixelsWritten] = (palette & 0x3) << 4 | (pixel & 0x3);

				++pixelsWritten;
				--currentXPosition;
			}

			currentXPosition = 7;
			++windowTileMapAddress;
		}
	}
}

void PPU::ProcessSpriteLayer()
{
	BYTE LCDC = m_ioMemory[LCDC_BYTE];

	// Bit 1 of LCDC tells us if the sprite display is on, return if not
	if (!(LCDC & BIT_1))
	{
		return;
	}

	// Width is always 8, but bit 2 will tell us if we need to increase the height by 8
	BYTE spriteXSize = 8;
	BYTE spriteYSize = 8 + ((LCDC >> 2 & 0x1) * 8);

	// Store our current line so we don't have to access the array each time
	BYTE currentYPosition = m_ioMemory[LCDC_Y_BYTE];

	int i;
	SpriteOAM currentSprite;

	// Cycle through each of the 40 possible sprites and display them if applicable
	for (i = 0; i < 40; i++)
	{
		int currentOAMSpriteNum = i * 4;
		currentSprite.yCoord = m_oamMemory[currentOAMSpriteNum];
		currentSprite.xCoord = m_oamMemory[currentOAMSpriteNum + 1];
		currentSprite.tileNumber = m_oamMemory[currentOAMSpriteNum + 2];
		currentSprite.options = m_oamMemory[currentOAMSpriteNum + 3];

		// Check if the current sprite is on the line we are drawing.
		if ((currentSprite.yCoord - (16 - spriteYSize) > currentYPosition) && (currentSprite.yCoord - 16 <= currentYPosition))
		{
			//draw it on the line
			WORD tileAddr = currentSprite.tileNumber * 0x10;

			BYTE upperTileBits;
			BYTE lowerTileBits;

			BYTE curSpriteX = currentSprite.xCoord - 8;
			BYTE curSpriteY = currentSprite.yCoord - 16;

			int address;

			/*
				The tileAddress we get from our tile map are ordered from 0x00 - 0xFF. In memory, this address corresponds to a value 0x10 times bigger
				0x8000 would be tileAddress 0x00, 0x8010 would be tile address 0x01 and so on. The reason for this is that there is 16 bytes for each tile (0x10)
				and 2 bytes for each row of pixels, which gives us our 8x8 tiles.

				The currentYPosition will tell us which row of pixels we are currently looking at, we multiply by 2 since there are 2 bytes per row.
			*/
			int currentSpriteYPosition;
			currentSpriteYPosition = currentYPosition - curSpriteY;

			if (currentSprite.ShouldFlipY())
			{
				address = tileAddr + ((7 - currentSpriteYPosition) * 2);
			}
			else
			{
				address = tileAddr + (currentSpriteYPosition * 2);
			}

			upperTileBits = m_vram[address];
			lowerTileBits = m_vram[address + 1];

			int j;
			for (j = 0; j < 8; j++)
			{
				/*
					Tile data is stored in 16 bytes where every 2 bytes represents a line in the tile
					The values inside these 2 bytes represents the colour of each pixel in the 8x8 tile
					The data of 2 lines translates into one line of pixels of various colours
					upper bits: 010001  ->  030001
					lower bits: 010000
					We receive a value between 0 and 3 for our pixel.
				*/
				int pixel;
				if (currentSprite.ShouldFlipX())
				{
					pixel = ((upperTileBits >> j) & 0x1) + ((lowerTileBits >> j) & 0x1) * 2;
				}
				else
				{
					pixel = ((upperTileBits >> (7 - j)) & 0x1) + ((lowerTileBits >> (7 - j)) & 0x1) * 2;
				}

				BYTE palette = ((m_ioMemory[SPRITE_PALETTE_DATA + currentSprite.UseObjectPalette1()]) >> (pixel * 2)) & (BIT_0 | BIT_1);
				int currentPixel = (curSpriteX + j);

				// Draw the sprite if it's within our screen size
				if ((curSpriteX >= 0) && (curSpriteX <= SCREEN_WIDTH) && (curSpriteY >= 0) && (curSpriteY < SCREEN_HEIGHT))
				{
					// We draw bgPriority pixels only if the background colour is white
					bool bgPixelDrawn = currentSprite.IsBackgroundPrioritized() && (m_scanline[currentPixel] & 0x3) == 0;

					// We draw fgPixels so long as they are not transparent
					// TODO for some reason sprites 10, 11, 12, 13, 14 are all using a transparent pixel of 1 instead of 0 on the level select of Dr Mario
					int fgPixelDrawn = !currentSprite.IsBackgroundPrioritized() && pixel != 0;

					if (bgPixelDrawn || fgPixelDrawn)
					{
						m_scanline[currentPixel] = (palette & 0x3) << 4 | (pixel & 0x3);
					}
				}
			}
		}
	}
}

bool PPU::SpriteOAM::IsBackgroundPrioritized() const
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

bool PPU::SpriteOAM::UseObjectPalette1() const
{
	return options & BIT_4;
}