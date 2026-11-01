#pragma once

#include "../Chunk.h"
#include "../BaseData.h"
#include "../../Util/JsonUtil.h"
#include "../Literal/StringChunk.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::JsonUtil;

		class Object : public BaseData
		{
		public:
			Object();
			~Object();

			UINT32 getDataSize()
			{
				UINT32 result = 0;
				for (size_t i = 0; i < chunkList.size(); i++) {
					result += sizeof(ChunkInfo);
					result += chunkList[i]->getSize();
				}
				return result;
			}

			UINT32 getHandle() { return 0; }
			void setHandle(UINT32 handle) { ; }

			BYTE getFlags() { return 0; }
			void clearFlags() { ; }

			bool exportData(wstring& dirPath, UINT32 index, bool isFullyExport);
			bool importData(wstring& dirPath, UINT32 index);
			bool exportJson(Json::Value& exportValue, vector<UINT32>& offsetList) { return true; }
			bool importJson(Json::Value& importValue, vector<UINT32>& offsetList) { return true; }

			bool loadFromBuffer(BYTE*& bufferPnt);
			bool saveIntoBuffer(BYTE*& bufferPnt);

			// 데이터 렌더링용 함수
			void renderData(UINT32 index);

		private:
			vector<CTT::Chunk::Chunk*> chunkList;

		};

	}

}