#pragma once

#include "../Chunk.h"
#include "../../Util/JsonUtil.h"
#include "AppHeaderStructure.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::JsonUtil;
		using namespace CTT::CommonUtil;

		class AppHeaderChunk : public Chunk 
		{
		public:
			AppHeaderChunk();
			~AppHeaderChunk();
			AppHeader& getValue() { return value; }
			Json::Value exportAsJson();
			void importFromJson(Json::Value &importValues);
			// 데이터 렌더링용 함수
			void renderData();
		protected:
			bool loadFromOrgBuffer();
			bool saveIntoOrgBuffer();
		private:
			AppHeader value;
		};

		vector<string> getCommonFlagProperties(UINT16 commonFlag);
		vector<string> getWindowFlagProperties(UINT16 windowFlag);
		vector<string> getDeviceFlagProperties(UINT16 deviceFlag);

	}

}