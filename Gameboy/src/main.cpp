#include "stdafx.h"

#include <fstream>

#include <imgui.h>
#include <imgui-SFML.h>
#include <SFML/Graphics.hpp>
#include <tinyfiledialogs/tinyfiledialogs.h>

#include "header/Cartridge.h"
#include "header/CPU.h"
#include "header/Debug.h"
#include "header/PPU.h"

std::string OpenFile()
{
    char const* lTheOpenFileName;
    char const* lFilterPatterns[1] = { "*.gb" };

    FILE* lIn;
    lTheOpenFileName = tinyfd_openFileDialog(
        "Select a ROM",
        "../",
        1,
        lFilterPatterns,
        "gb files",
        1);

    std::string file;
    if (lTheOpenFileName)
    {
        file = lTheOpenFileName;
    }

    return file;
}

int main(int argc, char* argv[])
{
    sf::RenderWindow window(sf::VideoMode(160 * 2, 144 * 2 + 19), "EMULATOR");
    window.setFramerateLimit(60);
    ImGui::SFML::Init(window);

    Cartridge cart;
    CPU sm83(&window);

    Joypad joypad;
    joypad.a = false;
    joypad.b = false;
    joypad.start = false;
    joypad.select = false;
    joypad.up = false;
    joypad.down = false;
    joypad.left = false;
    joypad.right = false;

    sf::Clock deltaClock;
    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            ImGui::SFML::ProcessEvent(event);
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

        if (!window.isOpen())
        {
            break;
        }

        sf::Time dt = deltaClock.restart();
        ImGui::SFML::Update(window, dt);

        const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2());
        ImGui::SetNextWindowSize(mainViewport->WorkSize);

        ImGuiWindowFlags windowFlags = 
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_MenuBar |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_UnsavedDocument;

        if (ImGui::Begin("menu_bar", nullptr, windowFlags))
        {
            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::MenuItem("Open"))
                {
                    std::string fileName = OpenFile();
                    cart.OpenFile(fileName);
                    if (cart.IsValid())
                    {
                        window.setTitle(cart.GetTitle());
                        sm83.AddCartridge(&cart);
                        sm83.PowerOn();
                    }
                }
                ImGui::EndMainMenuBar();
            }
        }
        ImGui::End();
        ImGui::SFML::Render(window);

        if (sm83.IsRunning())
        {
            sm83.WriteJoypad(joypad);
            while (sm83.GetTotalClockCycles() < 17480)
            {
                sm83.CPU_Step();
                if (deltaClock.getElapsedTime().asMicroseconds() > 16740)
                {
                    break;
                }
            }

            sm83.ResetTotalClockCycles();
            sm83.Draw();
        }
        
        window.display();
    }

    ImGui::SFML::Shutdown(window);
    return 0;
}