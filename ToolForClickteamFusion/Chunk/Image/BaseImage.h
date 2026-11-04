#pragma once

#include "../Chunk.h"
#include "../BaseData.h"
#include "../../Util/JsonUtil.h"
#include "../../Util/CommonStructure.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::JsonUtil;

		using namespace CTT::CommonUtil;

		typedef struct _common_image_info
		{
			UINT32 checksum;	// 체크섬, 0이어도 상관없는듯?
			UINT32 reference;	// 대충 1이면 될듯
			UINT32 size;		// *원본크기가 아니라 [압축크기 4byte] + [압축데이터] 크기임
			UINT16 width;		// *너비
			UINT16 height;		// *높이
			BYTE graphicMode;	// NormalImage / TwoFivePlusImage 별로 달라짐
			BYTE flags;			// NormalImage / TwoFivePlusImage 별로 달라짐
			UINT16 unknown2;	// 뭔지 모르는 값이긴 한데 0이라도 상관없는듯
			UINT16 hotSpotX;	// 0이라도 상관없는듯
			UINT16 hotSpotY;	// 0이라도 상관없는듯
			UINT16 actionX;		// 0이라도 상관없는듯
			UINT16 actionY;		// 0이라도 상관없는듯
			Color transparent;	// 일반적으로 #FF00FF에 alpha가 0이면 될듯
		} CommonImageInfo;

		// 합성할 때 특이점
		// graphicMode - NormalImage : 4/6/7 (일반적으로는 4?), TwoFivePlusImage : 8
		// flags - NormalImage : 0, TwoFivePlusImage : 0x08, 0x80, 0x88 (일반적으로는 0x80?)

		class BaseImage : public BaseData
		{
		public:
			BaseImage();
			~BaseImage();

			UINT32 getWidth() { return info.width; }
			void setWidth(UINT32 width) { info.width = (UINT16)width; }

			UINT32 getHeight() { return info.height; }
			UINT32 getHotSpotX() { return info.hotSpotX; }
			UINT32 getHotSpotY() { return info.hotSpotY; }
			void setHeight(UINT32 height) { info.height = (UINT16)height; }

			BYTE getFlags() { return info.flags; }
			void clearFlags() { info.flags = 0; }

			bool isAlphaSeparated() { return (info.flags & (1 << 4)) != 0; }
			void setAlphaSeparated() { info.flags |= (1 << 4); }

			bool isRGBAData() { return (info.flags & (1 << 7)) != 0; }
			void setRGBAData() { info.flags |= (1 << 7); }

			bool isLZX() { return (info.flags & (1 << 3)) != 0; }

			BYTE getGraphicMode() { return info.graphicMode; }
			void setGraphicMode(BYTE graphicMode) { info.graphicMode = graphicMode; }

			Color& getTransparent() { return info.transparent; }
			void setTransparent(Color &transparent) { info.transparent = transparent; }

			bool exportData(wstring& dirPath, UINT32 index, bool isFullyExport);
			bool importData(wstring& dirPath, UINT32 index);
			// 경로를 직접 지정하여 PNG 파일 하나로 이미지를 교체한다
			bool importFromFile(wstring& importFilePath);
			bool exportJson(Json::Value& exportValue, vector<UINT32>& offsetList) { return true; }
			bool importJson(Json::Value& importValue, vector<UINT32>& offsetList) { return true; }

			bool getBitmapDataFromTranslatingData();
			bool getTranslatingDataFromBitmapData();

		protected:
			CommonImageInfo info;
			BYTE *compressedData, *translatingData, *bitmapData;
			UINT32 compressedSize, translatingSize, bitmapSize;

			virtual bool deriveTranslatingData() = 0;
			virtual bool deriveCompressedData() = 0;
		};

	}

}