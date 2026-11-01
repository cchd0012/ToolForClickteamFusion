#pragma once

#include "../../Chunk.h"
#include "FrameEventModel.h"
#include "FrameEventText.h"
#include "FrameEventScript.h"
#include "FrameEventPatch.h"

namespace CTT {

	namespace Chunk {

		// 무손실 FrameEventChunkModel 을 사용하는 프레임 이벤트 청크 (0x333D)
		// 데이터를 파싱할 수 없거나 모델이 원본 바이트를 그대로 재현하지 못하면
		// 원본 바이트를 그대로 보존하므로, 저장하더라도 이벤트가 손상되지 않는다
		class FrameEventChunk : public Chunk
		{
		public:
			FrameEventChunk();
			~FrameEventChunk();

			// 데이터 렌더링 함수 (ImGui)
			void renderData();

			// 문자열 토큰을 번역 표에 모은다 (모델을 사용할 수 없으면 건너뛴다)
			bool extractText(UINT32 frameIndex, FrameEventTextTable& table);
			// 번역이 있는 문자열 토큰을 교체한다 (replacedCount = 교체한 토큰의 개수)
			bool replaceText(const FrameEventTextTable& table, UINT32& replacedCount);

			// 이벤트를 JSON 으로 풀어낸다 (디컴파일). 모델을 사용할 수 없으면 false
			bool decompileScript(const FrameEventScriptContext& context, Json::Value& value);
			// JSON 을 이벤트에 반영한다 (리컴파일). 실패하면 이벤트는 바뀌지 않고 errorMessage 에 이유가 담긴다
			bool recompileScript(const Json::Value& value, string& errorMessage);

			// ANSI 확장 문자열 왕복 보정 대상을 찾는다 / 보정을 적용한다 (모델을 사용할 수 없으면 건너뛴다)
			void detectParserSkeleton(vector<ParserSkeletonTarget>& targets);
			bool patchParserSkeleton(const ParserSkeletonConfig& config, ParserSkeletonStats& stats, string& errorMessage);

			bool isParsed() const { return parsed; }
			FrameEventChunkModel& getModel() { return model; }
			const string& getParseMessage() const { return parseMessage; }

		protected:
			bool loadFromOrgBuffer();
			bool saveIntoOrgBuffer();

		private:
			bool parsed;
			FrameEventChunkModel model;
			ByteBuffer rawFallback;		// 모델을 사용할 수 없을 때에만 채워지는 원본 바이트
			string parseMessage;

			// 뷰어에 표시할 통계
			size_t groupCount;
			size_t pointerCount;
		};

	}

}
