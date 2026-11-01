#pragma once

#include "../Chunk.h"
#include "../BaseData.h"
#include "../../Util/JsonUtil.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::JsonUtil;

		typedef struct _common_log_font_info
		{
			UINT32 checksum;		// 체크섬, 0이어도 상관없는듯?
			UINT32 reference;		// 대충 1이면 될듯
			UINT32 orgSize;			// *원본크기
			INT32 height;
			UINT32 width;
			UINT32 escapement;
			UINT32 orientation;
			UINT32 weight;
			BYTE italic;
			BYTE underline;
			BYTE strikeout;
			BYTE charset;
			BYTE outPrecision;
			BYTE clipPrecision;
			BYTE quality;
			BYTE pitchAndFamily;
			WCHAR name[0x20];
		} CommonLogFontInfo;

		class LogFont : public BaseData
		{
		public:
			LogFont();
			~LogFont();

			UINT32 getDataSize() { 
				deriveCompressedData();
				UINT32 ret_value = (sizeof(UINT32) * 3) + compressedSize;
				deriveFontData();
				return ret_value;
			}

			UINT32 getHandle() { return handle; }
			void setHandle(UINT32 handle) { this->handle = handle; }

			BYTE getFlags() { return 0; }
			void clearFlags() { ; }

			bool exportData(wstring& dirPath, UINT32 index, bool isFullyExport) { return true; }
			bool importData(wstring& dirPath, UINT32 index) { return true; }
			bool exportJson(Json::Value& exportValue, vector<UINT32>& offsetList);
			bool importJson(Json::Value& importValue, vector<UINT32>& offsetList);

			bool loadFromBuffer(BYTE*& bufferPnt);
			bool saveIntoBuffer(BYTE*& bufferPnt);

			// 데이터 렌더링용 함수
			void renderData(UINT32 index);

		private:
			bool deriveFontData();
			bool deriveCompressedData();

			// 추후 남는 공간의 handle 위치를 넣거나, 꽉 차면 (handle최대값 + 1) 하면 될듯
			UINT32 handle;
			UINT32 originalSize;
			CommonLogFontInfo info;
			BYTE* compressedData;
			UINT32 compressedSize;
		};

	}

}