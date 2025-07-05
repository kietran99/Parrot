project "Tests"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++latest"
    staticruntime "Off"
    exceptionhandling "Off"

    files
    {
        "*.h",
        "*.cpp",
        "Source/Common/**.h",
        "Source/Common/**.cpp",
    }

    filter "system:windows"
        files
        {
            "Source/Windows/**.h",
            "Source/Windows/**.cpp",
        }

    filter {}

    includedirs
    {
        "Source",
    }

    uses
    {
        "Placeholder",
    }

    targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
    objdir ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

    filter "system:windows"
        systemversion "latest"
        defines {}

    filter "configurations:Debug"
        defines { "DEBUG" }
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        runtime "Release"
        optimize "On"
        symbols "On"

    filter "configurations:Dist"
        defines { "NDEBUG" }
        runtime "Release"
        optimize "On"
        symbols "Off"

    filter {}
