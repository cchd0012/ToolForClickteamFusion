#pragma once

#include "Chunk.h"
#include "../Util/JsonUtil.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::JsonUtil;

		class BaseData
		{
		public:

			virtual UINT32 getDataSize() = 0;

			wstring& getName() { return name; }
			void setName(wstring& name) { this->name = name; }

			virtual UINT32 getHandle() { return 0; }
			virtual void setHandle(UINT32 handle) { ; }

			virtual BYTE getFlags() { return 0; }
			virtual void clearFlags() { ; }

			// 순수 가상 함수를 활용하려면 어쩔 수 없는 건가...?
			virtual bool exportData(wstring& dirPath, UINT32 index, bool isFullyExport = false) = 0;
			virtual bool importData(wstring& dirPath, UINT32 index) = 0;
			virtual bool exportJson(Json::Value& exportValue, vector<UINT32>& offsetList) = 0;
			virtual bool importJson(Json::Value& importValue, vector<UINT32>& offsetList) = 0;

			virtual bool loadFromBuffer(BYTE*& bufferPnt) = 0;
			virtual bool saveIntoBuffer(BYTE*& bufferPnt) = 0;

			// 데이터 렌더링용 함수
			virtual void renderData(UINT32 index) = 0;

		protected:

			wstring name;

		};

	}

}