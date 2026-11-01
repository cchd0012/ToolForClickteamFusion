#pragma once

#include "Chunk.h"
#include "Literal/Headers.h"
#include "BaseData.h"

namespace CTT {

	namespace Chunk {

		class DataChunk : public Chunk
		{
		public:
			DataChunk();
			~DataChunk();

			vector<BaseData*>& getValue() { return value; }

			bool exportData(wstring& exportDirPath, bool isFullyExport = false);
			bool importData(wstring& importDirPath);
			bool exportJson(wstring& exportJsonPath);
			bool importJson(wstring& importJsonPath);

			// 오프셋 리스트를 받아야 제대로 된 chunk를 생성할 수 있을 때 쓰인다
			void fillOffsets(ListChunk<UINT32>& offsetsChunk);
			vector<UINT32>& getOffsetList() { return offsetList; }
			void setOffsetList(vector<UINT32>& offsetList) { 
				this->offsetList.resize(offsetList.size());
				memcpy(this->offsetList.data(), offsetList.data(), (offsetList.size() * sizeof(UINT32)));
			}

			// 데이터 카운트용
			virtual bool readOnlyDataCount(HANDLE hArchive);
			// 데이터 렌더링용 함수
			void renderData();
		protected:
			virtual BaseData* allocateData() = 0;
			virtual bool loadFromOrgBuffer();
			virtual bool saveIntoOrgBuffer();
			vector<BaseData*> value;
			vector<UINT32> offsetList;
			UINT32 getMaxHandleValue();
		};

	}

}