#pragma once

#include "CommonStructure.h"
#include "../Common.h"

namespace CTT {

	namespace TextureUtil {

		using namespace std;

		HRESULT CreateTextureFromPngResource(LPDIRECT3DDEVICE9 device, HMODULE hModule, LPCWSTR resourceId, LPDIRECT3DTEXTURE9* ppTexture);

	}

}

