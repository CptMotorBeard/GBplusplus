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
	m_vram()
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

	m_ioMemory[STAT_REG] = MODE_OAMLOAD;
}

void PPU::Step(int clockCycles)
{
	m_gpuClock += clockCycles;
	switch (m_ioMemory[STAT_REG])
	{
	case MODE_HBLANK:
		if (m_gpuClock >= HBLANK_CYCLES)
		{
			m_gpuClock = 0;

			CleanLine();
			BYTE y = ++m_ioMemory[LCDC_Y_BYTE];

			if (y >= VBLANK_START)
			{
				m_ioMemory[STAT_REG] = MODE_VBLANK;

				// Trigger a VBLANK interrupt after rendering the image
				if (m_sm83->IsInterruptEnabled(CPU::INTERRUPT_VBLANK))
				{
					m_sm83->RequestInterrupt(CPU::INTERRUPT_VBLANK);
				}
			}
			else
			{
				// If we aren't at a VBLANK yet we restart the process
				m_ioMemory[STAT_REG] = MODE_OAMLOAD;
			}
		}
		break;
	case MODE_VBLANK:
		if (m_gpuClock >= VBLANK_CYCLES)
		{
			m_gpuClock = 0;
			BYTE y = ++m_ioMemory[LCDC_Y_BYTE];

			if (y > VBLANK_END)
			{
				// Restart
				m_ioMemory[STAT_REG] = MODE_OAMLOAD;
				m_ioMemory[LCDC_Y_BYTE] = 0;
			}
		}
		break;
	case MODE_OAMLOAD:
		if (m_gpuClock >= OAMLOAD_CYCLES)
		{
			m_gpuClock = 0;
			m_ioMemory[STAT_REG] = MODE_LCD;

			ProcessScanline();
		}
		break;
	case MODE_LCD:
		if (m_gpuClock >= LCD_CYCLES)
		{
			m_gpuClock = 0;
			m_ioMemory[STAT_REG] = MODE_HBLANK;

			// Bit 7 tells us if we need to render
			if (m_ioMemory[LCDC_BYTE] & BIT_7)
			{
				RenderScanline();
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
	if (m_ioMemory[STAT_REG] == MODE_LCD)
	{
		// VRAM is unnaccessable during mode 3 (LCD). Reads return garbage
		return 0xFF;
	}

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

void PPU::CleanLine()
{
	for (int x = 0; x < SCREEN_WIDTH; x++)
	{
		m_scanline[x] = sf::Color(0xFF, 0xFF, 0xFF, 0xFF);
	}
}

void PPU::RenderScanline()
{
	BYTE currentLine = m_ioMemory[LCDC_Y_BYTE];
	for (int x = 0; x < SCREEN_WIDTH; x++)
	{
		m_screenPixelBuffer.setPixel(x, currentLine, m_scanline[x]);
	}

	DrawToScreen();
}

typedef struct
{
	unsigned char signature[2];
	unsigned int filesize;
	short reserves[2];
	unsigned int bfoffset;
} fileheader;

typedef struct
{
	unsigned int biSize;
	unsigned int biWidth;
	unsigned int biHeight;
	short biPlanes;
	short biBitCount;
	unsigned int biCompression;
	unsigned int biSizeImage;
	unsigned int biXPelsPerMeter;
	unsigned int biYPelsPerMeter;
	unsigned int biClrUsed;
	unsigned int biClrImportant;
} bitmapheader;

typedef struct
{
	char b;
	char g;
	char r;
}pixel;

void makeBitmap(const char* filename, unsigned int width, unsigned int height, const int* data)
{
	unsigned int padSize = width * 3 % 4;
	unsigned int lineSize = width * 3 + padSize;

	// Create new file
	FILE* fp;
	fopen_s(&fp, filename, "w");

	// Setup the file header
	fileheader fh;
	fh.signature[0] = 'B';
	fh.signature[1] = 'M';
	fh.filesize = 54 + (lineSize)*height;
	fh.reserves[0] = 0;
	fh.reserves[1] = 0;
	fh.bfoffset = 0x36;

	// Write out the file header
	fwrite(&fh.signature, 1, 2 * sizeof(char), fp);
	fwrite(&fh.filesize, 1, sizeof(unsigned int), fp);
	fwrite(&fh.reserves, 1, 2 * sizeof(short), fp);
	fwrite(&fh.bfoffset, 1, sizeof(unsigned int), fp);

	// Set out the bitmap header
	bitmapheader bmh;
	bmh.biSize = 0x28;
	bmh.biWidth = width;
	bmh.biHeight = height;
	bmh.biPlanes = 0x01;
	bmh.biBitCount = 0x18;
	bmh.biCompression = 0;
	bmh.biSizeImage = 0x10;
	bmh.biXPelsPerMeter = 0x0b13;
	bmh.biYPelsPerMeter = 0x0b13;
	bmh.biClrUsed = 0;
	bmh.biClrImportant = 0;
	// Write out the bitmap header
	fwrite(&bmh.biSize, 1, sizeof(unsigned int), fp);
	fwrite(&bmh.biWidth, 1, sizeof(unsigned int), fp);
	fwrite(&bmh.biHeight, 1, sizeof(unsigned int), fp);
	fwrite(&bmh.biPlanes, 1, sizeof(short), fp);
	fwrite(&bmh.biBitCount, 1, sizeof(short), fp);
	fwrite(&bmh.biCompression, 1, sizeof(unsigned int), fp);
	fwrite(&bmh.biSizeImage, 1, sizeof(unsigned int), fp);
	fwrite(&bmh.biXPelsPerMeter, 1, sizeof(unsigned int), fp);
	fwrite(&bmh.biYPelsPerMeter, 1, sizeof(unsigned int), fp);
	fwrite(&bmh.biClrUsed, 1, sizeof(unsigned int), fp);
	fwrite(&bmh.biClrImportant, 1, sizeof(unsigned int), fp);
	// Content of image
	char* padding = new char[padSize * sizeof(char)];
	memset(&padding, 0, padSize);
	pixel current;
	unsigned int i;
	unsigned int j;
	for (i = 0; i < height; i++)
	{
		for (j = 0; j < width; j++)
		{
			current.r = data[(i * width + j) * 3];
			current.g = data[(i * width + j) * 3 + 1];
			current.b = data[(i * width + j) * 3 + 2];
			fwrite(&current, 1, sizeof(pixel), fp);
		}
		fwrite(&padding, 1, padSize, fp);
	}
	// Close and return
	delete[] padding;
	fclose(fp);
}

void PPU::DumpTiles(sf::RenderTarget& renderWindow)
{
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
			WORD address = (tileAddress * 0x10) + (currentYPosition * 2);

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
	BYTE scrollY = m_ioMemory[SCROLL_Y_BYTE];
	BYTE scrollX = m_ioMemory[SCROLL_X_BYTE];
	BYTE currentLine = m_ioMemory[LCDC_Y_BYTE];

	// Bit 0 tells us if we need to draw the background, if it's not enabled we can just leave
	if (!(LCDC & BIT_0))
	{
		return;
	}

	// Bit 3 of LCDC tells us the address range for the BG Tile Map
	WORD bgTileMapAddress = 0x1800 + ((LCDC >> 3 & 0x1) * 0x400);

	/*
	  There are 32 blocks with 8 bits each for both x and y direction.
	  Our screen size is 20 blocks wide and 18 blocks high, or 160x144
	  The base address (bgTileMapAddress) needs to be shifted based on scrollX and scrollY
	  This places us in the correct location for what we want to display.
	  If the screen is scrolled enough where our shift places some of the blocks within our screen >32 then the screen wraps around.
	*/

	int yShift = ((scrollY + currentLine) / 8) * 32;
	int xShift = (scrollX / 8);
	bgTileMapAddress += yShift + xShift;

	/*
	  We need our current screen x and y position, handled by scrollX and scrollY
	  Our tiles are 8x8 pixels. scroll is a pixel location, so scroll % 8 gives us our tiles positions
	  The upper left of our screen (which is where we start) is set by 7 - scrollX
	*/

	SIGNED_BYTE currentXPosition = 7 - (scrollX % 8);
	BYTE currentYPosition = (scrollY + currentLine) % 8;

	BYTE pixelsWritten = 0;
	while (pixelsWritten < SCREEN_WIDTH)
	{
		// Get the address of our tile from the tileSet
		BYTE tileAddress = m_vram[bgTileMapAddress];

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
			m_scanline[pixelsWritten] = m_colourPalette[palette];

			++pixelsWritten;
			--currentXPosition;
		}

		currentXPosition = 7;
		++bgTileMapAddress;
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
				m_scanline[pixelsWritten] = m_colourPalette[palette];

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
					bool bgPixelDrawn = currentSprite.IsBackgroundPrioritized() && m_scanline[currentPixel] == sf::Color(0xFF, 0xFF, 0xFF, 0xFF);

					// We draw fgPixels so long as they are not transparent
					// TODO for some reason sprites 10, 11, 12, 13, 14 are all using a transparent pixel of 1 instead of 0 on the level select of Dr Mario
					int fgPixelDrawn = !currentSprite.IsBackgroundPrioritized() && pixel != 0;

					if (bgPixelDrawn || fgPixelDrawn)
					{
						m_scanline[currentPixel] = m_colourPalette[palette];
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