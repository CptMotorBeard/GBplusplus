#include "stdafx.h"

#include <fstream>

#include <SFML/Graphics.hpp>

#include "header/Cartridge.h"
#include "header/CPU.h"
#include "header/Debug.h"
#include "header/PPU.h"

int main(int argc, char* argv[])
{
    DEBUG_ASSERT(argc >= 2, "No ROM given");
    if (argc >= 2)
    {
        Cartridge cart(argv[1]);
        sf::RenderWindow window(sf::VideoMode(160 * 2, 144 * 2), cart.GetTitle());
        sf::RenderWindow debugWindow(sf::VideoMode(32 * 8, 32 * 8), "DEBUG");
        window.setFramerateLimit(60);

        CPU sm83(&cart, &window);
        sf::Clock dt;

        while (window.isOpen())
        {
            sf::Event event;
            while (window.pollEvent(event))
            {
                if (event.type == sf::Event::Closed)
                {
                    window.close();
                    break;
                }
            }

            if (debugWindow.isOpen())
            {
                sm83.DumpGPU(debugWindow);
                debugWindow.display();
            }

            if (!window.isOpen())
            {
                break;
            }

            dt.restart();
            while (sm83.GetTotalClockCycles() < 17480)
            {
                sm83.CPU_Step();
                if (dt.getElapsedTime().asMicroseconds() > 16740)
                {
                    break;
                }
            }
            sm83.ResetTotalClockCycles();

            window.display();
        }
    }

    return 0;
}