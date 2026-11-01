#pragma once

#include "../Chunk.h"
#include "../BaseData.h"
#include "../../Util/JsonUtil.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::JsonUtil;

		typedef struct _common_true_type_font_info
		{
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
			UINT32 offset;
		} CommonTrueTypeFontInfo;

		class TrueTypeFontInfo : public BaseData
		{
		public:
			TrueTypeFontInfo();
			~TrueTypeFontInfo();

			UINT32 getDataSize() { return sizeof(CommonTrueTypeFontInfo); }

			bool exportData(wstring& dirPath, UINT32 index, bool isFullyExport = false) { return true; }
			bool importData(wstring& dirPath, UINT32 index) { return true; }
			bool exportJson(Json::Value& exportValue, vector<UINT32>& offsetList);
			bool importJson(Json::Value& importValue, vector<UINT32>& offsetList);

			bool loadFromBuffer(BYTE*& bufferPnt);
			bool saveIntoBuffer(BYTE*& bufferPnt);

			// 데이터 렌더링용 함수
			void renderData(UINT32 index);

		private:
			CommonTrueTypeFontInfo value;
		};

	}

}