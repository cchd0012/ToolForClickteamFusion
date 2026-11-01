#pragma once

#include "../Chunk.h"
#include "../BaseData.h"
#include "../../Util/JsonUtil.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::JsonUtil;

		typedef struct _compressed_data_info
		{
			UINT32 orgSize;
			UINT32 compressedSize;
			BYTE* compressedData;
		} CompressedDataInfo;

		class TrueTypeFont : public BaseData
		{
		public:
			TrueTypeFont();
			~TrueTypeFont();

			UINT32 getDataSize() { return ((sizeof(UINT32) * 2) + value.compressedSize); }

			bool exportData(wstring& dirPath, UINT32 index, bool isFullyExport);
			bool importData(wstring& dirPath, UINT32 index);
			bool exportJson(Json::Value& exportValue, vector<UINT32>& offsetList) { return true; }
			bool importJson(Json::Value& importValue, vector<UINT32>& offsetList) { return true; }

			bool loadFromBuffer(BYTE*& bufferPnt);
			bool saveIntoBuffer(BYTE*& bufferPnt);

			// 데이터 렌더링용 함수
			void renderData(UINT32 index);

		private:
			CompressedDataInfo value;
		};

	}

}