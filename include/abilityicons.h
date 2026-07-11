#pragma once
#include "heroicons.h"
#include "sdk.hpp"
#include "stealth.h"

extern std::unordered_map<uint32_t, ID3D11ShaderResourceView*> g_abilityIcons;

inline uint32_t AbilityIconKey(uint16_t heroId, uint16_t hash)
{
	return ((uint32_t)heroId << 16) | hash;
}

inline uint32_t AbilitySlotKey(uint16_t heroId, int slot)
{
	return ((uint32_t)heroId << 16) | (uint16_t)(0x8000 | (slot & 0x7));
}

inline std::wstring _abilityDir()
{
    return ResolveAssetSubdir(L"assets\\abilities");
}

inline void InitAbilityIcons()
{
	if (!g_pd3dDevice) return;

	HRESULT coHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (coHr == RPC_E_CHANGED_MODE)
		CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	for (auto& kv : g_abilityIcons)
		if (kv.second) kv.second->Release();
	g_abilityIcons.clear();

	IWICImagingFactory* factory = nullptr;
	if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&factory))) || !factory)
		return;

	std::wstring root = _abilityDir();
	LogAssetPath("Ability icons folder", root);
	std::wstring heroSearch = root + L"\\*";
	WIN32_FIND_DATAW fd = {};
	HANDLE hHero = FindFirstFileW(heroSearch.c_str(), &fd);
	if (hHero == INVALID_HANDLE_VALUE) {
		factory->Release();
		RS_LOG("[-] no ability icons folder\n");
		return;
	}

	do {
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
		if (fd.cFileName[0] == L'.') continue;

		uint16_t heroId = (uint16_t)wcstoul(fd.cFileName, nullptr, 16);
		if (!heroId) continue;

		std::wstring heroPath = root + L"\\" + fd.cFileName + L"\\*.png";
		WIN32_FIND_DATAW af = {};
		HANDLE hAb = FindFirstFileW(heroPath.c_str(), &af);
		if (hAb == INVALID_HANDLE_VALUE) continue;

		do {
			std::wstring name = af.cFileName;
			size_t dot = name.rfind(L'.');
			if (dot == std::wstring::npos) continue;
			name = name.substr(0, dot);

			uint32_t key = 0;
			if (_wcsnicmp(name.c_str(), L"slot", 4) == 0) {
				int slot = _wtoi(name.c_str() + 4);
				if (slot < 0 || slot > 7) continue;
				key = AbilitySlotKey(heroId, slot);
			} else {
				uint16_t hash = (uint16_t)wcstoul(name.c_str(), nullptr, 16);
				if (!hash) continue;
				key = AbilityIconKey(heroId, hash);
			}

			std::wstring full = root + L"\\" + fd.cFileName + L"\\" + af.cFileName;
			std::vector<uint8_t> rgba; UINT w = 0, h = 0;
			if (_wicLoadRGBA(factory, full.c_str(), rgba, w, h)) {
				ID3D11ShaderResourceView* srv = _makeTexture(rgba, w, h);
				if (srv) g_abilityIcons[key] = srv;
			}
		} while (FindNextFileW(hAb, &af));
		FindClose(hAb);
	} while (FindNextFileW(hHero, &fd));

	FindClose(hHero);
	factory->Release();
}

inline void ReloadAllIconAssets()
{
	InitHeroIcons();
	InitAbilityIcons();
}

inline ID3D11ShaderResourceView* GetAbilityIcon(uint16_t heroId, uint16_t hash)
{
	auto it = g_abilityIcons.find(AbilityIconKey(heroId, hash));
	return it == g_abilityIcons.end() ? nullptr : it->second;
}

inline ID3D11ShaderResourceView* GetAbilityIconBySlot(uint16_t heroId, int slot)
{
	auto it = g_abilityIcons.find(AbilitySlotKey(heroId, slot));
	return it == g_abilityIcons.end() ? nullptr : it->second;
}
