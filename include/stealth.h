#pragma once

#include <windows.h>
#include <cstdio>

#if defined(RS_SILENT_BUILD) && !defined(RS_DEBUG_CONSOLE)
#define RS_LOG(...)       ((void)0)
#define RS_LOGW(...)      ((void)0)
#else
#include <cstdio>
#define RS_LOG(...)       printf(__VA_ARGS__)
#define RS_LOGW(...)      wprintf(__VA_ARGS__)
#endif

inline void RsOpenDebugConsole()
{
#if defined(RS_DEBUG_CONSOLE) || !defined(RS_SILENT_BUILD)
	if (GetConsoleWindow()) return;
	AllocConsole();
	FILE* f = nullptr;
	freopen_s(&f, "CONOUT$", "w", stdout);
	freopen_s(&f, "CONOUT$", "w", stderr);
	freopen_s(&f, "CONIN$", "r", stdin);
	SetConsoleTitleA("blind");
	printf("[blind] console attached — INSERT = menu\n");
	fflush(stdout);
#endif
}

inline void StealthConfigRoot(char* out, int n)
{
	GetEnvironmentVariableA("APPDATA", out, n);
	if (!out[0]) {
		strncpy(out, ".", n - 1);
		out[n - 1] = 0;
		return;
	}
	char tmp[MAX_PATH];
	snprintf(tmp, MAX_PATH, "%s\\.wincache", out);
	CreateDirectoryA(tmp, nullptr);
	snprintf(out, n, "%s", tmp);
}
