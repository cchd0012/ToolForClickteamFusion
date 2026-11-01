#include "BitmapTranslationUtil.h"
#include "CommonUtil.h"
#include "CommonStructure.h"
#include <libpng-1.6.37/png.h>

using namespace CTT::BitmapTranslationUtil;
using namespace CTT::CommonUtil;


bool CTT::BitmapTranslationUtil::Normal32BitImageToRGBA(BYTE* translatingData, UINT32 translatingSize, BYTE*& bitmapData, UINT32& bitmapSize,
	UINT32 width, UINT32 height, bool isAlphaSeparated, Color& transparent)
{
	// 32비트 -> 32비트 변환

	UINT32 bitmapPitch = width * 4;
	UINT32 translationWidthPixel = width * 4;
	UINT32 translationPixelGap = (4 - (translationWidthPixel % 4)) % 4;
	UINT32 translationPitch = (width + translationPixelGap) * 4;
	UINT32 alphaPitch = width + ((4 - (width % 4)) % 4);

	if (translatingSize < (translationPitch * height)) {
		return false;
	}

	bitmapSize = bitmapPitch * height;
	bitmapData = (BYTE*)malloc(bitmapSize);
	memset(bitmapData, 0, bitmapSize);

	for (UINT32 i = 0; i < height; i++) {
		UINT32 bitmapStartPos = (bitmapPitch * i);
		UINT32 translationStartPos = (translationPitch * i);
		for (UINT32 j = 0; j < width; j++) {
			bitmapData[bitmapStartPos + (j * 4) + 0] = translatingData[translationStartPos + (j * 4) + 2];
			bitmapData[bitmapStartPos + (j * 4) + 1] = translatingData[translationStartPos + (j * 4) + 1];
			bitmapData[bitmapStartPos + (j * 4) + 2] = translatingData[translationStartPos + (j * 4) + 0];
			bitmapData[bitmapStartPos + (j * 4) + 3] = translatingData[translationStartPos + (j * 4) + 3];
		}
	}

	return true;
}

bool CTT::BitmapTranslationUtil::RGBAToNormal32BitImage(BYTE* bitmapData, UINT32 bitmapSize, BYTE*& translatingData, UINT32& translatingSize,
	UINT32 width, UINT32 height, bool isAlphaSeparated, Color& transparent)
{
	// 알파 데이터가 분리되어 있으면 그에 맞춰서 추가로 데이터 할당

	UINT32 bitmapPitch = width * 4;
	UINT32 translationWidthPixel = width * 4;
	UINT32 translationPixelGap = (4 - (translationWidthPixel % 4)) % 4;
	UINT32 translationPitch = (width + translationPixelGap) * 4;
	UINT32 alphaPitch = width + ((4 - (width % 4)) % 4);

	// 32비트만 허용
	if (bitmapSize < (bitmapPitch * height)) {
		return false;
	}

	translatingSize = (translationPitch * height);
	translatingData = (BYTE*)malloc(translatingSize);
	memset(translatingData, 0, translatingSize);

	// *** 왜 역순으로 하면 색이 달라지지? 이것도 반대로 해야 맞을거 같은데...
	for (UINT32 i = 0; i < height; i++) {
		UINT32 translationStartPos = (translationPitch * i);
		UINT32 bitmapStartPos = (bitmapPitch * i);
		for (UINT32 j = 0; j < width; j++) {
			translatingData[translationStartPos + (j * 4) + 0] = bitmapData[bitmapStartPos + (j * 4) + 2];
			translatingData[translationStartPos + (j * 4) + 1] = bitmapData[bitmapStartPos + (j * 4) + 1];
			translatingData[translationStartPos + (j * 4) + 2] = bitmapData[bitmapStartPos + (j * 4) + 0];
			translatingData[translationStartPos + (j * 4) + 3] = bitmapData[bitmapStartPos + (j * 4) + 3];
		}
	}

	return true;
}


bool CTT::BitmapTranslationUtil::Normal24BitImageToRGBA(BYTE* translatingData, UINT32 translatingSize, BYTE* &bitmapData, UINT32 &bitmapSize,
	UINT32 width, UINT32 height, bool isAlphaSeparated, Color& transparent)
{
	// 강제로 24 -> 32비트 변환

	UINT32 bitmapPitch = width * 4;
	UINT32 translationWidthPixel = width * 3;
	UINT32 translationPixelGap = (2 - (translationWidthPixel % 2)) % 2;
	UINT32 translationPitch = (width + translationPixelGap) * 3;
	UINT32 alphaPitch = width + ((4 - (width % 4)) % 4);

	if (translatingSize < (translationPitch * height)) {
		return false;
	}

	BYTE* alphaData = NULL;
	if (isAlphaSeparated) {
		if (translatingSize < ((translationPitch * height) + (alphaPitch * height))) {
			return false;
		}
		alphaData = translatingData + (translationPitch * height);
	}

	bitmapSize = bitmapPitch * height;
	bitmapData = (BYTE*)malloc(bitmapSize);
	memset(bitmapData, 0, bitmapSize);

	for (UINT32 i = 0; i < height; i++) {
		UINT32 bitmapStartPos = (bitmapPitch * i);
		UINT32 translationStartPos = (translationPitch * i);
		for (UINT32 j = 0; j < width; j++) {
			bitmapData[bitmapStartPos + (j * 4) + 0] = translatingData[translationStartPos + (j * 3) + 0];
			bitmapData[bitmapStartPos + (j * 4) + 1] = translatingData[translationStartPos + (j * 3) + 1];
			bitmapData[bitmapStartPos + (j * 4) + 2] = translatingData[translationStartPos + (j * 3) + 2];
			bitmapData[bitmapStartPos + (j * 4) + 3] = 0xFF;
			if (!isAlphaSeparated) {
				if ((bitmapData[bitmapStartPos + (j * 4) + 0] == transparent.r)
					&& (bitmapData[bitmapStartPos + (j * 4) + 1] == transparent.g)
					&& (bitmapData[bitmapStartPos + (j * 4) + 2] == transparent.b)) {
					bitmapData[bitmapStartPos + (j * 4) + 3] = 0x00;
				}
			}
		}
	}

	if (isAlphaSeparated) {
		for (UINT32 i = 0; i < height; i++) {
			UINT32 bitmapStartPos = (bitmapPitch * i);
			UINT32 alphaStartPos = (alphaPitch * i);
			for (UINT32 j = 0; j < width; j++) {
				bitmapData[bitmapStartPos + (j * 4) + 3] = alphaData[alphaStartPos + j];
			}
		}
	}

	return true;
}

bool CTT::BitmapTranslationUtil::RGBAToNormal24BitImage(BYTE* bitmapData, UINT32 bitmapSize, BYTE*& translatingData, UINT32& translatingSize,
	UINT32 width, UINT32 height, bool isAlphaSeparated, Color& transparent)
{
	// 알파 데이터가 분리되어 있으면 그에 맞춰서 추가로 데이터 할당

	UINT32 bitmapPitch = width * 4;
	UINT32 translationWidthPixel = width * 3;
	UINT32 translationPixelGap = (2 - (translationWidthPixel % 2)) % 2;
	UINT32 translationPitch = (width + translationPixelGap) * 3;
	UINT32 alphaPitch = width + ((4 - (width % 4)) % 4);

	// 32비트만 허용
	if (bitmapSize < (bitmapPitch * height)) {
		return false;
	}

	if (isAlphaSeparated) {
		translatingSize = (translationPitch * height) + (alphaPitch * height);
	}
	else {
		translatingSize = (translationPitch * height);
	}

	translatingData = (BYTE*)malloc(translatingSize);
	memset(translatingData, 0, translatingSize);

	BYTE* alphaData = NULL;
	if (isAlphaSeparated) {
		alphaData = translatingData + (translationPitch * height);
	}

	for (UINT32 i = 0; i < height; i++) {
		UINT32 translationStartPos = (translationPitch * i);
		UINT32 bitmapStartPos = (bitmapPitch * i);
		for (UINT32 j = 0; j < width; j++) {
			if (!isAlphaSeparated) {
				if (bitmapData[bitmapStartPos + (j * 4) + 3] = 0x00) {
					translatingData[translationStartPos + (j * 3) + 0] = transparent.r;
					translatingData[translationStartPos + (j * 3) + 1] = transparent.g;
					translatingData[translationStartPos + (j * 3) + 2] = transparent.b;
				}
				else {
					translatingData[translationStartPos + (j * 3) + 0] = bitmapData[bitmapStartPos + (j * 4) + 0];
					translatingData[translationStartPos + (j * 3) + 1] = bitmapData[bitmapStartPos + (j * 4) + 1];
					translatingData[translationStartPos + (j * 3) + 2] = bitmapData[bitmapStartPos + (j * 4) + 2];
				}
			}
			else {
				translatingData[translationStartPos + (j * 3) + 0] = bitmapData[bitmapStartPos + (j * 4) + 0];
				translatingData[translationStartPos + (j * 3) + 1] = bitmapData[bitmapStartPos + (j * 4) + 1];
				translatingData[translationStartPos + (j * 3) + 2] = bitmapData[bitmapStartPos + (j * 4) + 2];
			}
		}
	}

	if (isAlphaSeparated) {
		for (UINT32 i = 0; i < height; i++) {
			UINT32 alphaStartPos = (alphaPitch * i);
			UINT32 bitmapStartPos = (bitmapPitch * i);
			for (UINT32 j = 0; j < width; j++) {
				alphaData[alphaStartPos + j] = bitmapData[bitmapStartPos + (j * 4) + 3];
			}
		}
	}

	return true;
}


bool CTT::BitmapTranslationUtil::Normal15BitImageToRGBA(BYTE* translatingData, UINT32 translatingSize, BYTE*& bitmapData, UINT32& bitmapSize,
	UINT32 width, UINT32 height, Color& transparent)
{
	// (r, g, b) = (5bit, 5bit, 5bit)

	UINT32 bitmapPitch = width * 4;
	UINT32 translationWidthPixel = width * 2;
	UINT32 translationPixelGap = (2 - (translationWidthPixel % 2)) % 2;
	UINT32 translationPitch = (width + translationPixelGap) * 2;

	if (translatingSize < (translationPitch * height)) {
		return false;
	}

	bitmapSize = bitmapPitch * height;
	bitmapData = (BYTE*)malloc(bitmapSize);
	memset(bitmapData, 0, bitmapSize);

	for (UINT32 i = 0; i < height; i++) {
		UINT32 bitmapStartPos = (bitmapPitch * i);
		UINT32 translationStartPos = (translationPitch * i);
		for (UINT32 j = 0; j < width; j++) {
			UINT16 cv = *(UINT16*)(&translatingData[translationStartPos + (j * 2)]);
			BYTE r, g, b;
			r = (BYTE)((cv & 0x001F)) << 3;
			g = (BYTE)((cv & 0x03E0) >> 5) << 3;
			b = (BYTE)((cv & 0x7C00) >> 10) << 3;
			if ((r == transparent.r) && (g == transparent.g) && (b == transparent.b)) {
				bitmapData[bitmapStartPos + (j * 4) + 3] = 0x00;
			}
			else {
				bitmapData[bitmapStartPos + (j * 4) + 3] = 0xFF;
			}
		}
	}

	return true;
}

bool CTT::BitmapTranslationUtil::RGBAToNormal15BitImage(BYTE* bitmapData, UINT32 bitmapSize, BYTE*& translatingData, UINT32& translatingSize,
	UINT32 width, UINT32 height, Color& transparent)
{
	// (r, g, b) = (5bit, 5bit, 5bit)

	UINT32 bitmapPitch = width * 4;
	UINT32 translationWidthPixel = width * 3;
	UINT32 translationPixelGap = (2 - (translationWidthPixel % 2)) % 2;
	UINT32 translationPitch = (width + translationPixelGap) * 3;

	// 32비트만 허용
	if (bitmapSize < (bitmapPitch * height)) {
		return false;
	}

	translatingSize = translationPitch * height;
	translatingData = (BYTE*)malloc(translatingSize);

	for (UINT32 i = 0; i < height; i++) {
		UINT32 translationStartPos = (translationPitch * i);
		UINT32 bitmapStartPos = (bitmapPitch * i);
		for (UINT32 j = 0; j < width; j++) {
			BYTE r, g, b;
			if (bitmapData[bitmapStartPos + (j * 4) + 3] != 0x00) {
				r = bitmapData[bitmapStartPos + (j * 4) + 0] >> 3;
				g = bitmapData[bitmapStartPos + (j * 4) + 1] >> 3;
				b = bitmapData[bitmapStartPos + (j * 4) + 2] >> 3;
			}
			else {
				r = transparent.r;
				g = transparent.g;
				b = transparent.b;
			}
			UINT16 cv = (r) | (g << 5) | (b << 10);
			*(UINT16*)(&translatingData[translationStartPos + (j * 2)]) = cv;
		}
	}

	return true;
}


bool CTT::BitmapTranslationUtil::Normal16BitImageToRGBA(BYTE* translatingData, UINT32 translatingSize, BYTE*& bitmapData, UINT32& bitmapSize,
	UINT32 width, UINT32 height, Color& transparent)
{
	// (r, g, b) = (5bit, 6bit, 5bit)

	UINT32 bitmapPitch = width * 4;
	UINT32 translationWidthPixel = width * 2;
	UINT32 translationPixelGap = (2 - (translationWidthPixel % 2)) % 2;
	UINT32 translationPitch = (width + translationPixelGap) * 2;

	if (translatingSize < (translationPitch * height)) {
		return false;
	}

	bitmapSize = bitmapPitch * height;
	bitmapData = (BYTE*)malloc(bitmapSize);
	memset(bitmapData, 0, bitmapSize);

	for (UINT32 i = 0; i < height; i++) {
		UINT32 bitmapStartPos = (bitmapPitch * i);
		UINT32 translationStartPos = (translationPitch * i);
		for (UINT32 j = 0; j < width; j++) {
			UINT16 cv = *(UINT16*)(&translatingData[translationStartPos + (j * 2)]);
			bitmapData[bitmapStartPos + (j * 4) + 0] = (BYTE)((cv & 0x001F)) << 3;
			bitmapData[bitmapStartPos + (j * 4) + 1] = (BYTE)((cv & 0x07E0) >> 5) << 3;
			bitmapData[bitmapStartPos + (j * 4) + 2] = (BYTE)((cv & 0xF800) >> 11) << 3;
			if ((bitmapData[bitmapStartPos + (j * 4) + 0] == transparent.r)
				&& (bitmapData[bitmapStartPos + (j * 4) + 1] == transparent.g)
				&& (bitmapData[bitmapStartPos + (j * 4) + 2] == transparent.b)) {
				bitmapData[bitmapStartPos + (j * 4) + 3] = 0x00;
			}
			else {
				bitmapData[bitmapStartPos + (j * 4) + 3] = 0xFF;
			}
		}
	}

	return bitmapData;
}

bool CTT::BitmapTranslationUtil::RGBAToNormal16BitImage(BYTE* bitmapData, UINT32 bitmapSize, BYTE*& translatingData, UINT32& translatingSize,
	UINT32 width, UINT32 height, Color& transparent)
{
	// (r, g, b) = (5bit, 6bit, 5bit)

	UINT32 bitmapPitch = width * 4;
	UINT32 translationWidthPixel = width * 3;
	UINT32 translationPixelGap = (2 - (translationWidthPixel % 2)) % 2;
	UINT32 translationPitch = (width + translationPixelGap) * 3;

	// 32비트만 허용
	if (bitmapSize < (bitmapPitch * height)) {
		return false;
	}

	translatingSize = translationPitch * height;
	translatingData = (BYTE*)malloc(translatingSize);

	for (UINT32 i = 0; i < height; i++) {
		UINT32 translationStartPos = (translationPitch * i);
		UINT32 bitmapStartPos = (bitmapPitch * i);
		for (UINT32 j = 0; j < width; j++) {
			BYTE r, g, b;
			if (bitmapData[bitmapStartPos + (j * 4) + 3] != 0x00) {
				r = bitmapData[bitmapStartPos + (j * 4) + 0] >> 3;
				g = bitmapData[bitmapStartPos + (j * 4) + 1] >> 2;
				b = bitmapData[bitmapStartPos + (j * 4) + 2] >> 3;
			}
			else {
				r = transparent.r;
				g = transparent.g;
				b = transparent.b;
			}
			UINT16 cv = (r) | (g << 5) | (b << 11);
			*(UINT16*)(&translatingData[translationStartPos + (j * 2)]) = cv;
		}
	}

	return true;
}


bool CTT::BitmapTranslationUtil::twoFivePlusImageToRGBA(BYTE* translatingData, UINT32 translatingSize, BYTE*& bitmapData, UINT32& bitmapSize,
	UINT32 width, UINT32 height, bool isAlphaSeparated, bool isRGBAData, Color& transparent)
{
	// isAlphaSeparated와 isRGBAData가 둘 다 참인 경우가 있음...

	UINT32 bitmapPitch = width * 4;
	UINT32 translationPitch = width * 4;
	UINT32 alphaPitch = width + ((4 - (width % 4)) % 4);

	if (translatingSize < (translationPitch * height)) {
		return false;
	}

	BYTE* alphaData = NULL;
	if (isAlphaSeparated && !isRGBAData) {
		if (translatingSize < ((translationPitch * height) + (alphaPitch * height))) {
			return false;
		}
		alphaData = translatingData + (translationPitch * height);
	}

	bitmapSize = bitmapPitch * height;
	bitmapData = (BYTE*)malloc(bitmapSize);
	memset(bitmapData, 0, bitmapSize);

	for (UINT32 i = 0; i < height; i++) {
		UINT32 bitmapStartPos = (bitmapPitch * i);
		UINT32 translationStartPos = (translationPitch * i);
		for (UINT32 j = 0; j < width; j++) {
			bitmapData[bitmapStartPos + (j * 4) + 0] = translatingData[translationStartPos + (j * 4) + 0];
			bitmapData[bitmapStartPos + (j * 4) + 1] = translatingData[translationStartPos + (j * 4) + 1];
			bitmapData[bitmapStartPos + (j * 4) + 2] = translatingData[translationStartPos + (j * 4) + 2];
			if (isRGBAData) {
				// 데이터 자체가 rgba라면 그냥 알파데이터를 세팅
				bitmapData[bitmapStartPos + (j * 4) + 3] = translatingData[translationStartPos + (j * 4) + 3];
			}
			else if (!isAlphaSeparated) {
				// rgba 데이터는 아니지만 알파데이터가 분리되어있지 않다면 transparent를 기준으로 판단
				if ((bitmapData[bitmapStartPos + (j * 4) + 0] == transparent.b)
					&& (bitmapData[bitmapStartPos + (j * 4) + 1] == transparent.g)
					&& (bitmapData[bitmapStartPos + (j * 4) + 2] == transparent.r)) {
					bitmapData[bitmapStartPos + (j * 4) + 3] = 0x00;
				}
				else {
					bitmapData[bitmapStartPos + (j * 4) + 3] = 0xFF;
				}
			}
		}
	}

	// rgba 데이터가 아니지만 알파데이터가 분리되어 있으면, 즉 rgb + 알파데이터면 분리된 알파데이터를 넣어준다
	if (isAlphaSeparated && !isRGBAData) {
		for (UINT32 i = 0; i < height; i++) {
			UINT32 bitmapStartPos = (bitmapPitch * i);
			UINT32 alphaStartPos = (alphaPitch * i);
			for (UINT32 j = 0; j < width; j++) {
				bitmapData[bitmapStartPos + (j * 4) + 3] = alphaData[alphaStartPos + j];
			}
		}
	}

	return bitmapData;
}


bool CTT::BitmapTranslationUtil::RGBAToTwoFivePlusImage(BYTE* bitmapData, UINT32 bitmapSize, BYTE* &translatingData, UINT32 &translatingSize,
	UINT32 width, UINT32 height, bool isAlphaSeparated, bool isRGBAData, Color& transparent)
{
	// 알파 데이터가 분리되어 있으면 그에 맞춰서 추가로 데이터 할당

	UINT32 bitmapPitch = width * 4;
	UINT32 translationPitch = width * 4;
	UINT32 alphaPitch = width + ((4 - (width % 4)) % 4);

	// 32비트만 허용
	if (bitmapSize < (bitmapPitch * height)) {
		return false;
	}

	if (isAlphaSeparated && !isRGBAData) {
		translatingSize = (translationPitch * height) + (alphaPitch * height);
	}
	else {
		translatingSize = (translationPitch * height);
	}

	translatingData = (BYTE*)malloc(translatingSize);
	memset(translatingData, 0, translatingSize);

	BYTE* alphaData = NULL;
	if (isAlphaSeparated && !isRGBAData) {
		alphaData = translatingData + (translationPitch * height);
	}

	for (UINT32 i = 0; i < height; i++) {
		UINT32 translationStartPos = (translationPitch * i);
		UINT32 bitmapStartPos = (bitmapPitch * i);
		for (UINT32 j = 0; j < width; j++) {
			translatingData[translationStartPos + (j * 4) + 0] = bitmapData[bitmapStartPos + (j * 4) + 0];
			translatingData[translationStartPos + (j * 4) + 1] = bitmapData[bitmapStartPos + (j * 4) + 1];
			translatingData[translationStartPos + (j * 4) + 2] = bitmapData[bitmapStartPos + (j * 4) + 2];
			if (isRGBAData) {
				// 데이터 자체가 rgba라면 그냥 알파데이터를 세팅
				translatingData[translationStartPos + (j * 4) + 3] = bitmapData[bitmapStartPos + (j * 4) + 3];
			}
			else if (!isAlphaSeparated) {
				// rgba 데이터는 아니지만 알파데이터가 분리되어있지 않다면 transparent를 기준으로 00 혹은 FF 세팅
				if (bitmapData[bitmapStartPos + (j * 4) + 3] = 0x00) {
					translatingData[translationStartPos + (j * 4) + 0] = transparent.r;
					translatingData[translationStartPos + (j * 4) + 1] = transparent.g;
					translatingData[translationStartPos + (j * 4) + 2] = transparent.b;
					translatingData[translationStartPos + (j * 4) + 3] = 0x00;
				}
				else {
					translatingData[translationStartPos + (j * 4) + 3] = 0xFF;
				}
			}
		}
	}

	// rgba 데이터가 아니지만 알파데이터가 분리되어 있으면, 즉 rgb + 알파데이터면 분리된 알파데이터를 넣어준다
	if (isAlphaSeparated && !isRGBAData) {
		for (UINT32 i = 0; i < height; i++) {
			UINT32 alphaStartPos = (alphaPitch * i);
			UINT32 bitmapStartPos = (bitmapPitch * i);
			for (UINT32 j = 0; j < width; j++) {
				alphaData[alphaStartPos + j] = bitmapData[bitmapStartPos + (j * 4) + 3];
			}
		}
	}

	return true;
}