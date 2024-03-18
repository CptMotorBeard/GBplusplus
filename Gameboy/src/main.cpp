#include "stdafx.h"

#include <fstream>

#include <SFML/Graphics.hpp>
#include <imgui.h>
#include <imgui-SFML.h>

#include "header/Cartridge.h"
#include "header/CPU.h"
#include "header/CPU_Debug.h"
#include "header/Debug.h"
#include "header/PPU.h"

struct Debugger
{
    sf::Clock deltaClock;
    sf::Clock elapsedClock;
    CPU* sm83 = nullptr;

    bool shouldStep = false;
    bool shouldRun = false;
};

void UpdateDebugWindow(sf::RenderWindow& window, Debugger& debugger)
{
    sf::Time dt = debugger.deltaClock.restart();
    ImGui::SFML::Update(window, dt);

    sf::Vector2u windowSize = window.getSize();
    windowSize.x /= 4;

    ImGui::SetNextWindowSize(windowSize);
    ImGui::SetNextWindowPos(ImVec2(windowSize.x * 2, 0));

    if (ImGui::Begin("Register Debugger", (bool*)0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        CPU_DEBUG::ImGui_Registers(debugger.sm83);
        float totalElapsedTime = debugger.elapsedClock.getElapsedTime().asSeconds();
        float hz = CPU_DEBUG::s_cycles / totalElapsedTime;
        ImGui::Text("%.2f Hz", hz);
        ImGui::Text("FPS: %.2f", (1.f / dt.asSeconds()));
    }
    ImGui::End();

    ImGui::SetNextWindowSize(windowSize);
    ImGui::SetNextWindowPos(ImVec2(windowSize.x * 3, 0));
    if (ImGui::Begin("PPU Debugger", (bool*)0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        CPU_DEBUG::ImGui_PPU_Registers(debugger.sm83);
    }

    ImGui::End();
    ImGui::SFML::Render(window);
}

int main(int argc, char* argv[])
{
    DEBUG_ASSERT(argc >= 2, "No ROM given");
    if (argc >= 2)
    {
        Cartridge cart(argv[1]);
        CPU sm83(&cart);

        bool debugMode = true;

        Debugger debugger;
        debugger.sm83 = &sm83;
        debugger.shouldRun = false;
        debugger.shouldStep = false;

        sf::RenderWindow window(sf::VideoMode(160 * 4, 144*2), cart.GetTitle());
        ImGui::SFML::Init(window, false);

        ImGuiIO& io = ImGui::GetIO();
        io.FontDefault = io.Fonts->AddFontFromFileTTF("../assets/Roboto-Medium.ttf", 18);

        ImGui::SFML::UpdateFontTexture();

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
                else if (event.type == sf::Event::KeyReleased)
                {
                    
                    if (event.key.scancode == sf::Keyboard::Scan::S && event.key.control)
                    {
                        std::string romContents = CPU_DEBUG::GetDebugInfo(&cart);
                        std::ofstream f;
                        f.open("DEBUG_ROM.txt", std::ios::trunc);
                        f << romContents;
                        f.close();
                    }
                    else if (event.key.scancode == sf::Keyboard::Scan::F11)
                    {
                        debugMode = !debugMode;
                    }
                    else if (event.key.scancode == sf::Keyboard::Scan::Space)
                    {
                        if (event.key.control)
                        {
                            debugger.shouldRun = !debugger.shouldRun;
                        }
                        else
                        {
                            debugger.shouldStep = true;
                            if (debugger.shouldRun)
                            {
                                debugger.shouldRun = false;
                            }
                        }
                    }
                }
            }

            if (!window.isOpen())
            {
                break;
            }


            if (debugMode)
            {
                window.clear();
                UpdateDebugWindow(window, debugger);
                if (debugger.shouldStep || debugger.shouldRun)
                {
                    sm83.CPU_Step();
                    debugger.shouldStep = false;
                }
                window.display();
            }
            else
            {
               for (int i = 0; i < 50; ++i)
                {
                    sm83.CPU_Step();
                }
            }
        }
        ImGui::SFML::Shutdown(window);
    }

    return 0;
}