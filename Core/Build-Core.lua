project "Parrot"
    kind "StaticLib"
    language "C++"
    cppdialect "C++latest"
    staticruntime "Off"
    exceptionhandling "Off"

    fatalwarnings { "all" }

    files
    {
        "Public/**.h",
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
        "Public",
        "Source",
    }

    filter {}  

    targetdir ("../Binaries/" .. OutputDir .. "/%{prj.name}")
    objdir ("../Binaries/Intermediates/" .. OutputDir .. "/%{prj.name}")

    filter "system:windows"
        systemversion "latest"
        defines
        {
            "NOMINMAX",
            "WIN32_LEAN_AND_MEAN",
        }

    filter {}

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

    usage "INTERFACE"
        includedirs
        {
            "Public",
        }

    usage ""
