#pragma once

#include "CommonStructure.h"
#include "../Common.h"

namespace CTT {

	namespace BitmapTranslationUtil {

		using namespace std;
		using namespace CTT::CommonUtil;

		bool Normal32BitImageToRGBA(BYTE* translatingData, UINT32 translatingSize, BYTE*& bitmapData, UINT32& bitmapSize,
			UINT32 width, UINT32 height, bool isAlphaSeparated, Color& transparent);
		bool RGBAToNormal32BitImage(BYTE* bitmapData, UINT32 bitmapSize, BYTE*& translatingData, UINT32& translatingSize,
			UINT32 width, UINT32 height, bool isAlphaSeparated, Color& transparent);

		bool Normal24BitImageToRGBA(BYTE* translatingData, UINT32 translatingSize, BYTE*& bitmapData, UINT32& bitmapSize, 
			UINT32 width, UINT32 height, bool isAlphaSeparated, Color& transparent);
		bool RGBAToNormal24BitImage(BYTE* bitmapData, UINT32 bitmapSize, BYTE*& translatingData, UINT32& translatingSize, 
			UINT32 width, UINT32 height, bool isAlphaSeparated, Color& transparent);

		bool Normal15BitImageToRGBA(BYTE* translatingData, UINT32 translatingSize, BYTE*& bitmapData, UINT32& bitmapSize, 
			UINT32 width, UINT32 height, Color& transparent);
		bool RGBAToNormal15BitImage(BYTE* bitmapData, UINT32 bitmapSize, BYTE*& translatingData, UINT32& translatingSize, 
			UINT32 width, UINT32 height, Color& transparent);

		bool Normal16BitImageToRGBA(BYTE* translatingData, UINT32 translatingSize, BYTE*& bitmapData, UINT32& bitmapSize,
			UINT32 width, UINT32 height, Color& transparent);
		bool RGBAToNormal16BitImage(BYTE* bitmapData, UINT32 bitmapSize, BYTE*& translatingData, UINT32& translatingSize,
			UINT32 width, UINT32 height, Color& transparent);

		bool twoFivePlusImageToRGBA(BYTE* translatingData, UINT32 translatingSize, BYTE*& bitmapData, UINT32& bitmapSize,
			UINT32 width, UINT32 height, bool isAlphaSeparated, bool isRGBAData, Color& transparent);
		bool RGBAToTwoFivePlusImage(BYTE* bitmapData, UINT32 bitmapSize, BYTE*& translatingData, UINT32& translatingSize,
			UINT32 width, UINT32 height, bool isAlphaSeparated, bool isRGBAData, Color& transparent);

	}

}

