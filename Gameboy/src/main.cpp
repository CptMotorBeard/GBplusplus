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
    bool hasBreakPoint = false;
    WORD breakpointAddress = 0x0000;
};

void UpdateDebugWindow(sf::RenderWindow& window, Debugger& debugger)
{
    sf::Time dt = debugger.deltaClock.restart();
    ImGui::SFML::Update(window, dt);

    sf::Vector2u windowSize = window.getSize();
    windowSize.x /= 4;

    ImGui::SetNextWindowSize(windowSize);
    ImGui::SetNextWindowPos(ImVec2(windowSize.x * 2, 0));

    if (ImGui::Begin("Debugger0", (bool*)0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
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
    if (ImGui::Begin("Debugger1", (bool*)0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
    {
        ImVec2 availRegion = ImGui::GetContentRegionAvail();
        {
            ImGui::BeginChild("PPU Debugger", ImVec2(0, availRegion.y / 2.f));
            CPU_DEBUG::ImGui_PPU_Registers(debugger.sm83);
            ImGui::EndChild();
        }

        {
            ImGui::BeginChild("Breakpoint");

            ImGui::SeparatorText("Breakpoint");

            static char buf[5];

            bool shouldHaveBreakpoint = debugger.hasBreakPoint;

            if (debugger.hasBreakPoint)
            {
                ImGui::BeginDisabled();
            }

            ImGui::InputText("##", buf, 5, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
            if (ImGui::Button("Set"))
            {
                shouldHaveBreakpoint = true;
                debugger.breakpointAddress = CPU_DEBUG::ConvertHexToNumber(buf);
            }

            if (debugger.hasBreakPoint)
            {
                ImGui::EndDisabled();
            }

            if (!debugger.hasBreakPoint)
            {
                ImGui::BeginDisabled();
            }
            
            if (ImGui::Button("Reset"))
            {
                shouldHaveBreakpoint = false;
            }

            if (!debugger.hasBreakPoint)
            {
                ImGui::EndDisabled();
            }

            debugger.hasBreakPoint = shouldHaveBreakpoint;
            ImGui::EndChild();
        }
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
        bool runMode = false;

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
                        if (!debugMode)
                        {
                            runMode = true;
                        }
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
                    if (debugger.hasBreakPoint && CPU_DEBUG::CheckShouldBreak(&sm83, debugger.breakpointAddress))
                    {
                        debugger.shouldRun = false;
                        if (debugger.shouldStep)
                        {
                            sm83.CPU_Step();
                            debugger.shouldStep = false;
                        }
                    }
                    else
                    {
                        sm83.CPU_Step();
                        debugger.shouldStep = false;
                    }
                }
                window.display();
            }
            else
            {
               for (int i = 0; i < 50; ++i)
               {
                   if (!runMode && debugger.hasBreakPoint && CPU_DEBUG::CheckShouldBreak(&sm83, debugger.breakpointAddress))
                   {
                       debugMode = true;
                       debugger.shouldStep = false;
                       debugger.shouldRun = false;
                       break;
                   }

                   sm83.CPU_Step();
               }
               runMode = false;
            }
        }
        ImGui::SFML::Shutdown(window);
    }

    return 0;
}