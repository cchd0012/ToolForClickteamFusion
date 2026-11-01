#pragma once

#include <map>
#include "FrameEventModel.h"
#include "../../../Util/JsonUtil.h"

// 프레임 이벤트의 텍스트 추출 / 교체
//
// 수식 파라미터에 들어 있는 모든 문자열 토큰("글자 찍기" 텍스트, 대사, 반복문 이름 등)을
// 원문 그대로를 키로 하는 표에 모은다. 따라서 여러 곳에 나오는 텍스트(또는 모든 프레임에 복사되는
// 공유 전역 그룹의 텍스트)는 한 번만 나열되고, 모든 곳에서 똑같이 교체된다.
// 리터럴과 비교하는 조건과 같은 리터럴을 쓰는 액션도 그래서 서로 일관성을 유지한다.
//
// JSON 형식:
//   { "type": "frameEventText", "version": 1,
//     "texts": [ { "original": "...", "translation": "", "count": 3, "frameCount": 1, "firstFrame": 8 }, ... ] }
// translation 이 비어 있지 않고 original 과 다른 항목만 적용한다.

namespace CTT {

	namespace Chunk {

		using namespace std;

		typedef struct _frame_event_text_entry {
			wstring original;
			wstring translation;
			UINT32 count;			// 등장 횟수
			UINT32 frameCount;		// 텍스트가 나타나는 프레임의 수
			UINT32 firstFrame;
			UINT32 lastFrame;
			_frame_event_text_entry() : count(0), frameCount(0), firstFrame(0), lastFrame(0) {}
		} FrameEventTextEntry;

		class FrameEventTextTable
		{
		public:
			FrameEventTextTable();

			void clear();

			// importJson 에 성공하면 true (번역 표를 불러온 상태)
			bool isLoaded() const { return loaded; }
			// 번역 파일이 없을 때 등, 불러오기를 다시 시도하지 않도록 표시한다
			void markLoaded() { loaded = true; }
			size_t getEntryCount() const { return entries.size(); }
			const vector<FrameEventTextEntry>& getEntries() const { return entries; }

			// 프레임 하나의 문자열 토큰을 모은다. 찾은 토큰의 개수를 반환한다
			UINT32 addFrameEventText(const FrameEventChunkModel& model, UINT32 frameIndex);

			// 원문에 대한 번역, 적용할 번역이 없으면 NULL
			const wstring* findTranslation(const wstring& original) const;

			bool exportJson(Json::Value& value) const;
			bool importJson(Json::Value& value);

		private:
			vector<FrameEventTextEntry> entries;
			map<wstring, size_t> indexByOriginal;
			bool loaded;
		};

		// 번역이 있는 모든 문자열 토큰을 교체한다. replacedCount = 교체한 토큰의 개수
		bool replaceFrameEventText(FrameEventChunkModel& model, const FrameEventTextTable& table, UINT32& replacedCount, string& errorMessage);

	}

}
