#pragma once
#include <SFML/Graphics/Texture.hpp>

class CPU;
class PPU
{
public:
	PPU(sf::RenderTarget* screen);
	~PPU();

	void DumpTiles(sf::RenderTarget& renderWindow);

	void Initialize(CPU* sm83, BYTE* ioMemory, BYTE* oamMemory);
	void Step(int clockCycles);
	void DrawToScreen();

	BYTE ReadVRAM(WORD address) const;
	void WriteVRAM(WORD address, BYTE data);

private:
	void RenderScanline();

	void ProcessScanline();
	void ProcessBackgroundLayer();
	void ProcessWindowLayer();
	void ProcessSpriteLayer();

public:
	static constexpr WORD LCDC_BYTE = 0x40;
	static constexpr WORD STAT_REG = 0x41;
	static constexpr WORD SCROLL_Y_BYTE = 0x42;
	static constexpr WORD SCROLL_X_BYTE = 0x43;
	static constexpr WORD LCDC_Y_BYTE = 0x44;
	static constexpr WORD PALETTE_DATA = 0x47;
	static constexpr WORD SPRITE_PALETTE_DATA = 0x48;
	static constexpr WORD WINDOW_X = 0x4A;
	static constexpr WORD WINDOW_Y = 0x4B;

	static constexpr BYTE LCD_ENABLE = BIT_7;
	static constexpr BYTE WINDOW_TILEMAP = BIT_6;
	static constexpr BYTE WINDOW_ENABLE = BIT_5;
	static constexpr BYTE TILE_LOCATION = BIT_4;
	static constexpr BYTE BG_TILEMAP = BIT_3;
	static constexpr BYTE SPRITE_SIZE = BIT_2;
	static constexpr BYTE SPRITE_ENABLE = BIT_1;
	static constexpr BYTE BG_ENABLE = BIT_0;

	enum class GPUMode
	{
		// Send head to first row (204 cycles)
		HBLANK = 0x00,
		// Send head to first column (4560 cycles), 10 cycles at 456 each
		// Any values of LCDC_Y between 144 and 153 indicates the V-Blank period
		VBLANK = 0x01,
		// This time is used to fetch data from the Object Attribute Memory (80 cycles)
		OAMLOAD = 0x02,
		// Take the current scanline and draws it on screen (172 cycles)
		DRAWING = 0x03,
	};

private:
	CPU* m_sm83;
	BYTE* m_ioMemory;
	BYTE* m_oamMemory;
	sf::Image m_screenPixelBuffer;
	sf::Texture m_displayTexture;
	sf::RenderTarget* m_screen;

	std::vector<BYTE> m_scanline;

	GPUMode m_mode;
	BYTE* m_vram;

	/*
		0xFF40
		  Each bit of 0xFF40 represents various parameters with a 0 or 1 value
		  Bit 0 - BG & Window display OFF / ON
		  Bit 1 - OBJ (Sprite) display OFF / ON
		  Bit 2 - OBJ (Sprite) size 8x8 or 8x16 (width x height)
		  Bit 3 - BG Tile Map Display Select 0x9800 - 0x9BFF or 0x9C00 - 0x9FFF
		  Bit 4 - BG & Window Tile Data Select 0x8800 - 0x97FF or 0x8000 - 0x8FFF
		  Bit 5 - Window Display OFF / ON
		  Bit 6 - Window Tile Map Display Select 0x9800 - 0x9BFF or 0x9C00 - 0x9FFF
		  Bit 7 - LCD Control Operation OFF / ON

		0xFF42
		0xFF43
		  The scrollX and scrollY bytes are values between 0 and 255 which tell us where the LCD screen displays

		0xFF44
		  The GPU controls this value, LCDC_Y, to indicate the current Y position of the LCD
		  If the CPU writes to this location it resets, and CPU is using hardware.writeMemory()
		  to write to memory. We would use that function in other places, but we need complete
		  control over the value so we directly modify it using cpu[LCDC_Y_BYTE]++
	*/

	static constexpr int HBLANK_CYCLES = 204;
	static constexpr int VBLANK_CYCLES = 456;
	static constexpr int VBLANK_START = 144;
	static constexpr int VBLANK_END = 153;
	static constexpr int OAMLOAD_CYCLES = 80;
	static constexpr int LCD_CYCLES = 172;

	int m_gpuClock;

	// The gameboy handles four different colours. Black (Pixel OFF), White (Pixel ON),
	// Dark Grey (33% ON) and Light Grey (66% ON).
	sf::Color m_colourPalette[4] =
	{
		sf::Color(0xFF, 0xFF, 0xFF, 0xFF),
		sf::Color(0xAA, 0xAA, 0xAA, 0xFF),
		sf::Color(0x55, 0x55, 0x55, 0xFF),
		sf::Color(0x00, 0x00, 0x00, 0xFF)
	};

	/*
		Tile map is simply 32*32 bytes refering to a certain tile in the tileset (results to a 256*256 display)

		VRAM MAP
		-------------------------------
		9C00-9FFF Tile Map #1
		-------------------------------
		9800-9BFF Tile Map #0
		-------------------------------
		9000-97FF Tile Set #1 (tiles 0-127)
		-------------------------------
		8800-8FFF Tile set #1 (tiles 128-255)
				  Tile set #0 (tiles (-1)-(-128)
		-------------------------------
		8000-87FF Tile Set #0 (0-127)
		-------------------------------

		LCDC keeps track of the Display operation, tile maps, tile sets, and sprite/tile size
	*/

	/*
		BYTE 0	- y-coordinate of top-left corner, value stored is y-coordinate - 16
		BYTE 1	- x-coordinate of top-left corner, value stored is x-coordinate - 8
		BYTE 2	- Data tile number
		BYTE 3	- Options
	*/
	struct SpriteOAM
	{
		BYTE yCoord;
		BYTE xCoord;
		BYTE tileNumber;
		BYTE options;

		// Options Bit 7 - Sprite / Background priority (0: Above background, 1 : Below Background)
		bool IsBackgroundPrioritized() const;
		// Options Bit 6 - Y - flip(0: Normal, 1 : Vertical Flip)
		bool ShouldFlipY() const;
		// Options Bit 5 - X-flip (0: Normal, 1: Horizontal Flip)
		bool ShouldFlipX() const;
		// Options Bit 4 - Palette (0: OBJ Palette 0, 1: OBJ Palette 1)
		bool UseObjectPalette1() const;
	};
};