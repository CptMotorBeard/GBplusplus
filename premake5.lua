function defaultConfigurations()
	filter "system:windows"
		cppdialect "C++17"
		staticruntime "On"
		systemversion "latest"

	filter "configurations:Debug"
		defines
        {
            "SFML_STATIC",
            "CT_DEBUG"
            
        }
		symbols "On"
        staticruntime "off"
        runtime "Debug"
		links
        {
            "sfml-audio-s-d.lib",
            "sfml-system-s-d.lib",
            "sfml-window-s-d.lib",
            "sfml-network-s-d.lib",
            "sfml-graphics-s-d.lib",
            "opengl32.lib",
            "freetype.lib",
            "winmm.lib",
            "gdi32.lib",
            "flac.lib",
            "vorbisenc.lib",
            "vorbisfile.lib",
            "vorbis.lib",
            "ogg.lib",
            "ws2_32.lib",
            "comdlg32.lib",
            "ole32.lib"
        }
	filter "configurations:OptDebug"
		defines
        {
            "CT_OPTDEBUG",
            "SFML_STATIC"
        }
        symbols "On"
        optimize "On"
        staticruntime "off"
        runtime "Debug"
		links
        {
            "sfml-audio-s-d.lib",
            "sfml-system-s-d.lib",
            "sfml-window-s-d.lib",
            "sfml-network-s-d.lib",
            "sfml-graphics-s-d.lib",
            "opengl32.lib",
            "freetype.lib",
            "winmm.lib",
            "gdi32.lib",
            "flac.lib",
            "vorbisenc.lib",
            "vorbisfile.lib",
            "vorbis.lib",
            "ogg.lib",
            "ws2_32.lib",
            "comdlg32.lib",
            "ole32.lib"
        }
        
	filter "configurations:Distribution"
        defines
        {
            "CT_DIST",
            "SFML_STATIC"
        }
		optimize "On"
        symbols "Off"
        staticruntime "off"
        runtime "Release"
        links
        {
            "sfml-audio-s.lib",
            "sfml-system-s.lib",
            "sfml-window-s.lib",
            "sfml-network-s.lib",
            "sfml-graphics-s.lib",
            "opengl32.lib",
            "freetype.lib",
            "winmm.lib",
            "gdi32.lib",
            "flac.lib",
            "vorbisenc.lib",
            "vorbisfile.lib",
            "vorbis.lib",
            "ogg.lib",
            "ws2_32.lib",
            "comdlg32.lib",
            "ole32.lib"
        }
end

workspace "Gameboy++"
	architecture "x32"
	
	configurations
	{
		"Debug",
		"OptDebug",
		"Distribution"
	}
	
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

project "Gameboy"
	location "Gameboy/project"
	kind "ConsoleApp"
	language "C++"
	
    libdirs { "vendor/SFML-2.6.1/lib" }
    
	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
	
	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.c",
		"%{prj.name}/src/**.cpp"
	}
	
	includedirs
	{
		"%{prj.name}/src/",
		"%{prj.name}/ImGui/include",
        "vendor/SFML-2.6.1/include"
	}
    
	pchheader "stdafx.h"
	pchsource "%{prj.name}/src/stdafx.cpp"
	
	defaultConfigurations()