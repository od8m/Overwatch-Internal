#pragma once
#include <windows.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include "stealth.h"

#pragma comment(lib, "shlwapi.lib")

extern HMODULE g_overlayModule;

extern char g_assetHeroPathA[512];
extern char g_assetAbilityPathA[512];
extern int  g_assetHeroFilesOnDisk;

inline HMODULE GetReaderModuleHandle()
{
	if (g_overlayModule)
		return g_overlayModule;
	HMODULE mod = GetModuleHandleW(L"blind.dll");
	if (mod) return mod;
	GetModuleHandleExW(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCWSTR)&GetReaderModuleHandle, &mod);
	return mod;
}

inline std::wstring NormalizePath(const std::wstring& path)
{
	wchar_t full[MAX_PATH] = {};
	if (!GetFullPathNameW(path.c_str(), MAX_PATH, full, nullptr))
		return path;
	return full;
}

inline void SetAssetPathA(char* out, int n, const std::wstring& wpath)
{
	if (!out || n <= 0) return;
	out[0] = 0;
	if (wpath.empty()) return;
	WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, out, n, nullptr, nullptr);
	out[n - 1] = 0;
}

inline bool DirHasMatchingFiles(const std::wstring& dir, const wchar_t* pattern)
{
	if (dir.empty()) return false;
	std::wstring search = dir + pattern;
	WIN32_FIND_DATAW fd = {};
	HANDLE h = FindFirstFileW(search.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return false;
	do {
		if (fd.cFileName[0] != L'.') {
			FindClose(h);
			return true;
		}
	} while (FindNextFileW(h, &fd));
	FindClose(h);
	return false;
}

inline int CountMatchingFiles(const std::wstring& dir, const wchar_t* pattern)
{
	if (dir.empty()) return 0;
	int n = 0;
	std::wstring search = dir + pattern;
	WIN32_FIND_DATAW fd = {};
	HANDLE h = FindFirstFileW(search.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return 0;
	do {
		if (fd.cFileName[0] != L'.') ++n;
	} while (FindNextFileW(h, &fd));
	FindClose(h);
	return n;
}

inline bool AbilitiesDirPopulated(const std::wstring& dir)
{
	if (dir.empty()) return false;
	WIN32_FIND_DATAW fd = {};
	HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return false;
	bool ok = false;
	do {
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
		if (fd.cFileName[0] == L'.') continue;
		if (DirHasMatchingFiles(dir + L"\\" + fd.cFileName, L"\\*.png")) {
			ok = true;
			break;
		}
	} while (FindNextFileW(h, &fd));
	FindClose(h);
	return ok;
}

inline void CollectAssetRoots(std::vector<std::wstring>& roots)
{
	auto pushUnique = [&](const std::wstring& p) {
		if (p.empty()) return;
		std::wstring n = NormalizePath(p);
		for (const auto& r : roots)
			if (_wcsicmp(r.c_str(), n.c_str()) == 0) return;
		roots.push_back(n);
	};

	HMODULE reader = GetReaderModuleHandle();
	wchar_t dll[MAX_PATH] = {};
	if (reader && GetModuleFileNameW(reader, dll, MAX_PATH)) {
		std::wstring dllDir = dll;
		size_t slash = dllDir.find_last_of(L"\\/");
		if (slash != std::wstring::npos) {
			dllDir = dllDir.substr(0, slash);
			pushUnique(dllDir);
			pushUnique(dllDir + L"\\..");
			pushUnique(dllDir + L"\\..\\..");
		}
	}

	wchar_t appData[MAX_PATH] = {};
	if (GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH))
		pushUnique(std::wstring(appData) + L"\\.wincache\\assets");

	wchar_t cwd[MAX_PATH] = {};
	if (GetCurrentDirectoryW(MAX_PATH, cwd))
		pushUnique(cwd);
}

inline std::wstring ResolveAssetSubdir(const wchar_t* subdir)
{
	const bool isHeroes = wcsstr(subdir, L"heroes") != nullptr;

	std::vector<std::wstring> roots;
	CollectAssetRoots(roots);

	for (const auto& root : roots) {
		std::wstring test = NormalizePath(root + L"\\" + subdir);
		if (isHeroes) {
			if (DirHasMatchingFiles(test, L"\\*.png")) {
				if (isHeroes) {
					SetAssetPathA(g_assetHeroPathA, sizeof(g_assetHeroPathA), test);
					g_assetHeroFilesOnDisk = CountMatchingFiles(test, L"\\*.png");
				}
				return test;
			}
		} else if (AbilitiesDirPopulated(test)) {
			SetAssetPathA(g_assetAbilityPathA, sizeof(g_assetAbilityPathA), test);
			return test;
		}
	}

	if (!roots.empty()) {
		std::wstring fallback = NormalizePath(roots[0] + L"\\" + subdir);
		if (isHeroes) {
			SetAssetPathA(g_assetHeroPathA, sizeof(g_assetHeroPathA), fallback);
			g_assetHeroFilesOnDisk = CountMatchingFiles(fallback, L"\\*.png");
		} else {
			SetAssetPathA(g_assetAbilityPathA, sizeof(g_assetAbilityPathA), fallback);
		}
		return fallback;
	}

	wchar_t cwd[MAX_PATH] = {};
	GetCurrentDirectoryW(MAX_PATH, cwd);
	return NormalizePath(std::wstring(cwd) + L"\\" + subdir);
}

inline bool CopyFileIfMissing(const std::wstring& src, const std::wstring& dst)
{
	if (GetFileAttributesW(dst.c_str()) != INVALID_FILE_ATTRIBUTES)
		return true;
	if (GetFileAttributesW(src.c_str()) == INVALID_FILE_ATTRIBUTES)
		return false;

	size_t slash = dst.find_last_of(L"\\/");
	if (slash != std::wstring::npos) {
		std::wstring dir = dst.substr(0, slash);
		CreateDirectoryW(dir.c_str(), nullptr);
	}
	return CopyFileW(src.c_str(), dst.c_str(), TRUE) != FALSE;
}

inline void CacheHeroPortraitsToAppData(const std::wstring& srcDir)
{
	if (srcDir.empty() || !DirHasMatchingFiles(srcDir, L"\\*.png")) return;

	wchar_t appData[MAX_PATH] = {};
	if (!GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH)) return;

	std::wstring dstDir = NormalizePath(std::wstring(appData) + L"\\.wincache\\assets\\heroes");
	if (_wcsicmp(NormalizePath(srcDir).c_str(), dstDir.c_str()) == 0) return;

	CreateDirectoryW((std::wstring(appData) + L"\\.wincache").c_str(), nullptr);
	CreateDirectoryW((std::wstring(appData) + L"\\.wincache\\assets").c_str(), nullptr);
	CreateDirectoryW(dstDir.c_str(), nullptr);

	std::wstring search = srcDir + L"\\*.png";
	WIN32_FIND_DATAW fd = {};
	HANDLE h = FindFirstFileW(search.c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE) return;
	do {
		if (fd.cFileName[0] == L'.') continue;
		CopyFileIfMissing(srcDir + L"\\" + fd.cFileName, dstDir + L"\\" + fd.cFileName);
	} while (FindNextFileW(h, &fd));
	FindClose(h);
}

inline void LogAssetPath(const char* label, const std::wstring& path)
{
	RS_LOG("[assets] %s: ", label);
	RS_LOGW(L"%s\n", path.c_str());
}
