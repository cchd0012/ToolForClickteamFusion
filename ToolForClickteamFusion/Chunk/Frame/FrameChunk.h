#pragma once

#include "../Chunk.h"
#include "FrameEvent/FrameEventText.h"
#include "FrameEvent/FrameEventScript.h"
#include "FrameEvent/FrameEventPatch.h"

namespace CTT {

	namespace Chunk {

		class FrameChunk : public Chunk 
		{
		public:
			FrameChunk();
			~FrameChunk();
			// 오직 정보 읽기용
			bool readChunkTreeInfoFromArchive(HANDLE hArchive);
			// 프레임 이벤트의 텍스트 추출 / 교체
			bool extractEventText(UINT32 frameIndex, FrameEventTextTable& table);
			bool replaceEventText(const FrameEventTextTable& table, UINT32& replacedCount);

			// 프레임 디컴파일 / 리컴파일 (현재는 이벤트). 프레임 이름과 이벤트를 JSON 으로 풀어내거나 다시 반영한다
			bool decompileScript(UINT32 frameIndex, const FrameEventScriptContext& context, Json::Value& value);
			bool recompileScript(const Json::Value& value, string& errorMessage);
			// ANSI 확장 문자열 왕복 보정 (한글이 들어가면 글자 배치가 깨지는 문제). 대상 감지 / 적용
			void detectParserSkeleton(vector<ParserSkeletonTarget>& targets);
			bool patchParserSkeleton(const ParserSkeletonConfig& config, ParserSkeletonStats& stats, string& errorMessage);
			// 데이터 렌더링용 함수
			void renderData();
		protected:
			bool loadFromOrgBuffer();
			bool saveIntoOrgBuffer();
		private:
			vector<CTT::Chunk::Chunk*> chunkList;
		};

	}

}
