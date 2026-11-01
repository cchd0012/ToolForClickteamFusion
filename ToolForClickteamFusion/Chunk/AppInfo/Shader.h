#pragma once

#include "../Chunk.h"
#include "../BaseData.h"
#include "../../Util/JsonUtil.h"
#include "ShaderStructure.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::JsonUtil;

		class Shader : public BaseData
		{
		public:
			Shader();
			~Shader();

			UINT32 getDataSize() { return totalDataSize; }

			bool exportData(wstring& dirPath, UINT32 index, bool isFullyExport);
			bool importData(wstring& dirPath, UINT32 index);
			bool exportJson(Json::Value& exportValue, vector<UINT32>& offsetList) { return true; }
			bool importJson(Json::Value& importValue, vector<UINT32>& offsetList) { return true; }

			bool loadFromBuffer(BYTE*& bufferPnt);
			bool saveIntoBuffer(BYTE*& bufferPnt);

			// 이걸 해줘야 데이터를 정상적으로 load할 수 있음
			void setTotalDataSize(UINT32 totalDataSize) { this->totalDataSize = totalDataSize; }

			// frame/object/object 2.5+ 와 연동하기 위해 필요하다
			vector<ShaderParameter>& getParameterList() { return parameterList; }

			// 데이터 렌더링용 함수
			void renderData(UINT32 index);

		private:
			ShaderHeader header;
			UINT32 totalDataSize;
			BYTE* compiledData;
			UINT32 compiledDataSize;
			vector<ShaderParameter> parameterList;
			UINT32 backgroundTexture;
		};

	}

}