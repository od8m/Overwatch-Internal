#pragma once
#include <windows.h>
#include <cstdio>

inline const char* GetOverlayClassNameA()
{
	static char s_cls[24] = {};
	if (!s_cls[0]) {
		DWORD vol = 0;
		GetVolumeInformationA("C:\\", nullptr, 0, &vol, nullptr, nullptr, nullptr, 0);
		unsigned h = (unsigned)(vol ^ GetCurrentProcessId() ^ 0x6D4F2B91u);
		snprintf(s_cls, sizeof(s_cls), "W%08X", h);
	}
	return s_cls;
}

inline void UnregisterOverlayClassA()
{
	UnregisterClassA(GetOverlayClassNameA(), GetModuleHandleA(nullptr));
}
