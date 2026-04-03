---------------------------------------- ImGUI-------------------------------------------------------
project "ImGui"
	kind "StaticLib"
	language "C++"

	targetdir ("ImGui/Binaries/" .. outputdir .. "/%{prj.name}")
	objdir ("ImGui/Intermediate/" .. outputdir .. "/%{prj.name}")

	files
	{
		"ImGui/imconfig.h",
		"ImGui/imgui.h",
		"ImGui/imgui.cpp",
		"ImGui/imgui_draw.cpp",
		"ImGui/imgui_internal.h",
		"ImGui/imgui_widgets.cpp",
		"ImGui/imstb_rectpack.h",
		"ImGui/imstb_textedit.h",
		"ImGui/imstb_truetype.h",
		"ImGui/imgui_demo.cpp",
		"ImGui/imgui_tables.cpp",
		"ImGui/backends/imgui_impl_glfw.cpp",
		"ImGui/backends/imgui_impl_vulkan.cpp"
	}

	includedirs
	{
		"ImGui",
		"ImGui/backends",
		"%{IncludeDir.GLFW}/include",
		"%{IncludeDir.VulkanSDK}"
	}

	defines
	{
	}

	filter "system:windows"
		systemversion "latest"
		cppdialect "C++23"
		staticruntime "On"

	filter "system:linux"
		pic "On"
		systemversion "latest"
		cppdialect "C++23"
		staticruntime "On"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"

---------------------------------------- yaml-cpp -------------------------------------------------------
project "yaml-cpp"
	kind "StaticLib"
	language "C++"
	cppdialect "C++23"

	targetdir ("yaml-cpp/Binaries/" .. outputdir .. "/%{prj.name}")
	objdir ("yaml-cpp/Intermediate/" .. outputdir .. "/%{prj.name}")

	files
	{
		"yaml-cpp/include/yaml-cpp/yaml.h",
		"yaml-cpp/src/binary.cpp",
		"yaml-cpp/src/convert.cpp",
		"yaml-cpp/src/depthguard.cpp",
		"yaml-cpp/src/directives.cpp",
		"yaml-cpp/src/emitter.cpp",
		"yaml-cpp/src/emitterstate.cpp",
		"yaml-cpp/src/emitterutils.cpp",
		"yaml-cpp/src/emitfromevents.cpp",
		"yaml-cpp/src/emit.cpp",
		"yaml-cpp/src/exceptions.cpp",
		"yaml-cpp/src/exp.cpp",
		"yaml-cpp/src/fptostring.cpp",
		"yaml-cpp/src/memory.cpp",
		"yaml-cpp/src/node.cpp",
		"yaml-cpp/src/nodebuilder.cpp",
		"yaml-cpp/src/node_data.cpp",
		"yaml-cpp/src/nodeevents.cpp",
		"yaml-cpp/src/null.cpp",
		"yaml-cpp/src/ostream_wrapper.cpp",
		"yaml-cpp/src/parse.cpp",
		"yaml-cpp/src/parser.cpp",
		"yaml-cpp/src/regex_yaml.cpp",
		"yaml-cpp/src/scanner.cpp",
		"yaml-cpp/src/scantag.cpp",
		"yaml-cpp/src/scanscalar.cpp",
		"yaml-cpp/src/scantoken.cpp",
		"yaml-cpp/src/simplekey.cpp",
		"yaml-cpp/src/singledocparser.cpp",
		"yaml-cpp/src/stream.cpp",
		"yaml-cpp/src/tag.cpp",
		"yaml-cpp/src/contrib/graphbuilder.cpp",
		"yaml-cpp/src/contrib/graphbuilderadapter.cpp"
	}

	includedirs
	{
		"yaml-cpp/include"
	}

	filter "system:windows"
		systemversion "latest"
		staticruntime "off"
		cppdialect "C++23"
		defines { "YAML_CPP_STATIC_DEFINE" }

	filter "system:linux"
		pic "On"
		systemversion "latest"
		staticruntime "On"
		cppdialect "C++23"

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"

---------------------------------------- GLFW-------------------------------------------------------
project "GLFW"
	kind "StaticLib"
	language "C"

	targetdir ("glfw/Binaries/" .. outputdir .. "/%{prj.name}")
	objdir ("glfw/Intermediate/" .. outputdir .. "/%{prj.name}")

	files
	{
		"glfw/include/GLFW/glfw3.h",
		"glfw/include/GLFW/glfw3native.h",
		"glfw/src/glfw_config.h",
		"glfw/src/context.c",
		"glfw/src/init.c",
		"glfw/src/input.c",
		"glfw/src/monitor.c",
		"glfw/src/platform.c",
		"glfw/src/vulkan.c",
		"glfw/src/window.c",
		"glfw/src/egl_context.c",
		"glfw/src/osmesa_context.c",
		"glfw/src/null_platform.h",
		"glfw/src/null_joystick.h",
		"glfw/src/null_init.c",
		"glfw/src/null_monitor.c",
		"glfw/src/null_window.c",
		"glfw/src/null_joystick.c"
	}

	filter "system:windows"
		systemversion "latest"
		staticruntime "On"

		files
		{
			"glfw/src/win32_module.c",
			"glfw/src/win32_init.c",
			"glfw/src/win32_joystick.c",
			"glfw/src/win32_monitor.c",
			"glfw/src/win32_time.c",
			"glfw/src/win32_thread.c",
			"glfw/src/win32_window.c",
			"glfw/src/wgl_context.c",
			"glfw/src/egl_context.c",
			"glfw/src/osmesa_context.c"
		}

		defines 
		{ 
			"_GLFW_WIN32",
			"_CRT_SECURE_NO_WARNINGS"
		}

	filter "system:linux"
		pic "On"
		systemversion "latest"
		staticruntime "On"

		files
		{
			"glfw/src/x11_init.c",
			"glfw/src/x11_monitor.c",
			"glfw/src/x11_window.c",
			"glfw/src/xkb_unicode.c",
			"glfw/src/posix_module.c",
			"glfw/src/posix_time.c",
			"glfw/src/posix_thread.c",
			"glfw/src/posix_poll.c",
			"glfw/src/glx_context.c",
			"glfw/src/egl_context.c",
			"glfw/src/osmesa_context.c",
			"glfw/src/linux_joystick.c"
		}

		defines
		{
			"_GLFW_X11"
		}

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"