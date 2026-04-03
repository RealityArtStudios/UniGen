project "Editor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++23"
	staticruntime "off"

	targetdir ("Binaries/" .. outputdir .. "/%{prj.name}")
	objdir ("Intermediate/" .. outputdir .. "/%{prj.name}")

	files { 
		"Source/**.h", 
		"Source/**.cpp"
	}

	includedirs
	{
		"Source",
		"../Engine/Source",
		"../Engine/ThirdParty/GLFW",
		"../Engine/ThirdParty/GLFW/include",
		"../Engine/ThirdParty/ImGui",
		"../Engine/ThirdParty/ImGui/backends",
		"%{IncludeDir.VulkanSDK}",
		"%{IncludeDir.GLM}",
		"%{IncludeDir.stb}",
		"%{IncludeDir.tinyobjloader}",
		"%{IncludeDir.tinygltf}",
		"%{IncludeDir.KTX}",
		"../Engine/ThirdParty/yaml-cpp/include"
	}

	links
	{
		"UniGen",
		"ImGui",
		"yaml-cpp"
	}

	-- Link to yaml-cpp library
	filter {"system:windows", "configurations:*"}
		defines { "YAML_CPP_STATIC_DEFINE" }

	filter "configurations:Debug"
		libdirs { "../Engine/ThirdParty/yaml-cpp/Binaries/Debug/windows/x86_64/yaml-cpp" }

	filter "configurations:Release"
		libdirs { "../Engine/ThirdParty/yaml-cpp/Binaries/Release/windows/x86_64/yaml-cpp" }

	filter "system:windows"
		systemversion "latest"
		defines { "CAE_PLATFORM_WINDOWS" }

	filter "configurations:Debug"
		defines { "CAE_DEBUG" }
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
		defines { "CAE_RELEASE" }
		runtime "Release"
		optimize "On"

filter "configurations:Dist"
		defines { "CAE_DIST" }
		runtime "Release"
		optimize "On"
		symbols "Off"