#pragma once

#include "CommonStructure.h"
#include "../Common.h"

namespace CTT {

	namespace PngUtil {

		using namespace std;

		bool writeToPng(BYTE*& imageBuffer, wstring pngPath, UINT32& width, UINT32& height, UINT32& bitCount, bool isNoneTransparentMode = false);
		bool readFromPng(BYTE*& imageBuffer, wstring pngPath, UINT32& width, UINT32& height, UINT32& bitCount);

		HRESULT createTextureFromPngMemory(BYTE* pngData, size_t pngSize, LPDIRECT3DDEVICE9 device, LPDIRECT3DTEXTURE9* ppTexture);

	}

}

