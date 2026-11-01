#include "TextureUtil.h"
#include "PngUtil.h"
#include "CommonUtil.h"

using namespace CTT::TextureUtil;
using namespace CTT::PngUtil;
using namespace CTT::CommonUtil;

#pragma warning (disable:6031)
#pragma warning (disable:4996)


HRESULT CTT::TextureUtil::CreateTextureFromPngResource(LPDIRECT3DDEVICE9 device, HMODULE hModule, LPCWSTR resourceId, LPDIRECT3DTEXTURE9* ppTexture)
{
	// 리소스 찾기
	HRSRC hResource = FindResourceW(hModule, resourceId, L"PNG");
	if (!hResource)
	{
		return -1;
	}

	// 리소스 로드
	HGLOBAL hLoadedResource = LoadResource(hModule, hResource);
	if (!hLoadedResource)
	{
		return -1;
	}

	// 리소스 데이터 잠금
	LPVOID pResourceData = LockResource(hLoadedResource);
	DWORD resourceSize = SizeofResource(hModule, hResource);
	if (!pResourceData)
	{
		return -1;
	}

	// 이때 데이터는 png 데이터이므로 그대로 텍스처로 세팅하도록 한다
	return createTextureFromPngMemory((BYTE*)pResourceData, resourceSize, device, ppTexture);
}