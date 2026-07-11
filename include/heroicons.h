#pragma once
#include "sdk.hpp"
#include "assets_paths.h"
#include "stealth.h"
#include <d3d11.h>
#include <wincodec.h>
#include <string>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")

extern std::unordered_map<uint16_t, ID3D11ShaderResourceView*> g_heroIcons;
extern std::unordered_map<uint16_t, float> g_heroIconAspect;

inline bool IsTrainingBotHeroId(uint16_t id)
{
	switch (id) {
	case 0x0337: case 0x033C: case 0x035A: case 0x04E7:
		return true;
	default:
		return false;
	}
}

inline uint16_t ResolveHeroIconId(uint16_t heroId)
{
	if (!heroId || IsTrainingBotHeroId(heroId)) return 0;

	static const uint16_t kLegacyAlias[][2] = {
		{ 0x032E, 0x0362 },
		{ 0x04EE, 0x032A },
		{ 0x04F8, 0x03C3 },
		{ 0x04FA, 0x0472 },
		{ 0x04FC, 0x04C4 },
	};
	for (auto& pair : kLegacyAlias) {
		if (heroId == pair[0]) return pair[1];
	}
	return heroId;
}

inline bool _wicLoadRGBA(IWICImagingFactory* factory, const wchar_t* path,
                         std::vector<uint8_t>& out, UINT& w, UINT& h)
{
    IWICBitmapDecoder* dec = nullptr;
    if (FAILED(factory->CreateDecoderFromFilename(path, nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnDemand, &dec)) || !dec)
        return false;

    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* conv = nullptr;
    bool ok = false;
    if (SUCCEEDED(dec->GetFrame(0, &frame)) && frame &&
        SUCCEEDED(factory->CreateFormatConverter(&conv)) && conv &&
        SUCCEEDED(conv->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)) &&
        SUCCEEDED(conv->GetSize(&w, &h)) && w > 0 && h > 0)
    {
        out.resize((size_t)w * h * 4);
        if (SUCCEEDED(conv->CopyPixels(nullptr, w * 4, (UINT)out.size(), out.data())))
            ok = true;
    }
    if (conv)  conv->Release();
    if (frame) frame->Release();
    if (dec)   dec->Release();
    return ok;
}

inline ID3D11ShaderResourceView* _makeTexture(const std::vector<uint8_t>& rgba, UINT w, UINT h)
{
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = rgba.data();
    sd.SysMemPitch = w * 4;

    ID3D11Texture2D* tex = nullptr;
    if (FAILED(g_pd3dDevice->CreateTexture2D(&td, &sd, &tex)) || !tex)
        return nullptr;

    ID3D11ShaderResourceView* srv = nullptr;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format = td.Format;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = 1;
    g_pd3dDevice->CreateShaderResourceView(tex, &srvd, &srv);
    tex->Release();
    return srv;
}

inline std::wstring _heroDir()
{
    return ResolveAssetSubdir(L"assets\\heroes");
}

inline void _registerHeroIcon(uint16_t id, ID3D11ShaderResourceView* srv, float aspect)
{
    if (!id || !srv) return;
    g_heroIcons[id] = srv;
    g_heroIconAspect[id] = aspect;
}

inline void InitHeroIcons()
{
    if (!g_pd3dDevice) return;

    HRESULT coHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (coHr == RPC_E_CHANGED_MODE)
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    IWICImagingFactory* factory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory))) || !factory)
        return;

    for (auto& kv : g_heroIcons)
        if (kv.second) kv.second->Release();
    g_heroIcons.clear();
    g_heroIconAspect.clear();

    std::wstring dir = _heroDir();
    LogAssetPath("Hero icons folder", dir);
    std::wstring search = dir + L"\\*.png";
    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        factory->Release();
        RS_LOG("[-] no hero pngs in assets/heroes\n");
        return;
    }

    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::wstring fname = fd.cFileName;
        size_t dot = fname.rfind(L'.');
        if (dot != std::wstring::npos) fname = fname.substr(0, dot);
        uint16_t id = (uint16_t)wcstoul(fname.c_str(), nullptr, 16);
        if (!id) continue;
        std::wstring full = dir + L"\\" + fd.cFileName;
        std::vector<uint8_t> rgba; UINT w = 0, h = 0;
        if (_wicLoadRGBA(factory, full.c_str(), rgba, w, h)) {
            ID3D11ShaderResourceView* srv = _makeTexture(rgba, w, h);
            if (srv)
                _registerHeroIcon(id, srv, (h > 0) ? (float)w / (float)h : 1.f);
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    factory->Release();

    if (!g_heroIcons.empty())
        CacheHeroPortraitsToAppData(dir);
}

inline ID3D11ShaderResourceView* GetHeroIcon(uint16_t heroId)
{
    uint16_t id = ResolveHeroIconId(heroId);
    if (!id) return nullptr;
    auto it = g_heroIcons.find(id);
    return it == g_heroIcons.end() ? nullptr : it->second;
}

inline bool IsTrainingBotEntity(const EntityInfo& e)
{
    if (IsTrainingBotHeroId(e.heroId)) return true;
    return e.boneCount > 0 && e.boneCount <= 5 && e.health_max <= 150.f;
}

inline bool EntityHasPortrait(const EntityInfo& e)
{
    if (!e.heroId || IsTrainingBotEntity(e)) return false;
    return GetHeroIcon(e.heroId) != nullptr;
}

inline float GetHeroIconAspect(uint16_t heroId)
{
    uint16_t id = ResolveHeroIconId(heroId);
    if (!id) return 1.f;
    auto it = g_heroIconAspect.find(id);
    return it == g_heroIconAspect.end() ? 1.f : it->second;
}
