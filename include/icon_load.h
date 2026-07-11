#pragma once
#include <vector>
#include <windows.h>

bool LoadImageFileRGBA(const wchar_t* path, std::vector<uint8_t>& out, UINT& w, UINT& h);
