#pragma once

#include "../Chunk.h"
#include "../BaseData.h"
#include "../../Util/JsonUtil.h"
#include "FrameStructure.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::JsonUtil;

		class FrameObject : public BaseData
		{
		public:
			FrameObject();
			~FrameObject();

			UINT32 getDataSize() { return sizeof(FrameObjectInfo); }

			bool loadFromBuffer(BYTE*& bufferPnt);
			bool saveIntoBuffer(BYTE*& bufferPnt);

			bool exportData(wstring& dirPath, UINT32 index, bool isFullyExport) { return true; }
			bool importData(wstring& dirPath, UINT32 index) { return true; }
			bool exportJson(Json::Value& exportValue, vector<UINT32>& offsetList);
			bool importJson(Json::Value& importValue, vector<UINT32>& offsetList);

			// 데이터 렌더링용 함수
			void renderData(UINT32 index);

		private:
			FrameObjectInfo info;

		};

	}

}