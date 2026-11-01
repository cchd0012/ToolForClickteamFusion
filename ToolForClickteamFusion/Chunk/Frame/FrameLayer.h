#pragma once

#include "../Chunk.h"
#include "../BaseData.h"
#include "../../Util/JsonUtil.h"
#include "FrameStructure.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::JsonUtil;

		class FrameLayer : public BaseData
		{
		public:
			FrameLayer();
			~FrameLayer();

			UINT32 getDataSize() { return sizeof(FrameLayerInfo) + ((UINT32)(name.length() + 1) * sizeof(WCHAR)); }

			bool loadFromBuffer(BYTE*& bufferPnt);
			bool saveIntoBuffer(BYTE*& bufferPnt);

			bool exportData(wstring& dirPath, UINT32 index, bool isFullyExport) { return true; }
			bool importData(wstring& dirPath, UINT32 index) { return true; }
			bool exportJson(Json::Value& exportValue, vector<UINT32>& offsetList);
			bool importJson(Json::Value& importValue, vector<UINT32>& offsetList);

			// 데이터 렌더링용 함수
			void renderData(UINT32 index);

		private:
			FrameLayerInfo info;
			wstring name;

		};

		vector<string> getFrameLayerFlagProperties(UINT32 frameLayerFlag);

	}

}