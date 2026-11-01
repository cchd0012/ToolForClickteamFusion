#pragma once

#include "../Chunk.h"
#include "../../Util/JsonUtil.h"
#include "../../Util/CommonStructure.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::JsonUtil;
		using namespace CTT::CommonUtil;

		typedef struct _object_header
		{
			UINT16 handle;
			UINT16 type;
			BYTE flag1;
			BYTE flag2;
			UINT16 unk1;
			BYTE shaderIndex;
			BYTE flag3;
			UINT16 unk2;
			Color color;
		} ObjectHeader;

		class ObjectHeaderChunk : public Chunk
		{
		public:
			ObjectHeaderChunk();
			~ObjectHeaderChunk();
			ObjectHeader& getValue() { return value; }
			Json::Value exportAsJson();
			void importFromJson(Json::Value& importValues);
			// 데이터 렌더링용 함수
			void renderData();
		protected:
			bool loadFromOrgBuffer();
			bool saveIntoOrgBuffer();

		private:
			ObjectHeader value;
		};

	}

}