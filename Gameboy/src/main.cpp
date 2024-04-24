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
        Joypad joypad;

        sf::Clock dt;

        joypad.a = false;
        joypad.b = false;
        joypad.start = false;
        joypad.select = false;
        joypad.up = false;
        joypad.down = false;
        joypad.left = false;
        joypad.right = false;

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
                else if (event.type == sf::Event::KeyPressed)
                {
                    if (event.key.scancode == sf::Keyboard::Scancode::Z) { joypad.a = true; }
                    else if (event.key.scancode == sf::Keyboard::Scancode::X) { joypad.b = true; }
                    else if (event.key.scancode == sf::Keyboard::Scancode::Enter) { joypad.start = true; }
                    else if (event.key.scancode == sf::Keyboard::Scancode::Backspace) { joypad.select = true; }
                    else if (event.key.scancode == sf::Keyboard::Scancode::Up) { joypad.up = true; }
                    else if (event.key.scancode == sf::Keyboard::Scancode::Down) { joypad.down = true; }
                    else if (event.key.scancode == sf::Keyboard::Scancode::Left) { joypad.left = true; }
                    else if (event.key.scancode == sf::Keyboard::Scancode::Right) { joypad.right = true; }
                }
                else if (event.type == sf::Event::KeyReleased)
                {
                    if (event.key.scancode == sf::Keyboard::Scancode::Z) { joypad.a = false; }
                    else if (event.key.scancode == sf::Keyboard::Scancode::X) { joypad.b = false; }
                    else if (event.key.scancode == sf::Keyboard::Scancode::Enter) { joypad.start = false; }
                    else if (event.key.scancode == sf::Keyboard::Scancode::Backspace) { joypad.select = false; }
                    else if (event.key.scancode == sf::Keyboard::Scancode::Up) { joypad.up = false; }
                    else if (event.key.scancode == sf::Keyboard::Scancode::Down) { joypad.down = false; }
                    else if (event.key.scancode == sf::Keyboard::Scancode::Left) { joypad.left = false; }
                    else if (event.key.scancode == sf::Keyboard::Scancode::Right) { joypad.right = false; }
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
            sm83.WriteJoypad(joypad);
            while (sm83.GetTotalClockCycles() < 17480)
            {
                sm83.CPU_Step();
                if (dt.getElapsedTime().asMicroseconds() > 16740)
                {
                    break;
                }
            }

            sm83.ResetTotalClockCycles();
            sm83.Draw();
            window.display();
        }
    }

    return 0;
}