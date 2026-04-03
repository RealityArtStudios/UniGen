#pragma once

#include "Application.h"

#ifdef CAE_PLATFORM_WINDOWS
	#ifdef CAE_DIST
		#include <Windows.h>
		int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow);
	#else
		int main(int argc, char** argv);
	#endif
#endif



	extern Application* CreateApplication(int argc, char** argv);