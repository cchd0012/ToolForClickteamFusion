#pragma once

#include <map>
#include "ObjectCommon.h"
#include "../../Util/JsonUtil.h"

// 오브젝트 텍스트의 추출 / 교체
//
// 오브젝트마다 다음 두 종류의 텍스트를 모은다.
//   - Alterable 문자열 (SPRI 등 모든 오브젝트가 가질 수 있는 문자열, 글자 오브젝트의 문자 목록도 여기에 있다)
//   - 일반 Text 오브젝트의 문단
// 오브젝트는 핸들로 구분하고, 교체할 때는 원문이 현재 값과 같은 항목만 적용한다
// (원문이 다르면 다른 게임이거나 이미 바뀐 데이터이므로 건너뛴다).
//
// JSON 형식:
//   { "type": "objectText", "version": 1,
//     "objects": [ { "handle": 38, "name": "letter", "identifier": "SPRI",
//                    "strings":    [ { "index": 0, "original": "...", "translation": "" } ],
//                    "paragraphs": [ { "index": 0, "font": 1, "original": "...", "translation": "" } ] } ] }
// translation 이 비어 있지 않고 original 과 다른 항목만 적용한다.

namespace CTT {

	namespace Chunk {

		using namespace std;

		typedef struct _object_text_string {
			INT32 index;
			wstring original;
			wstring translation;
			_object_text_string() : index(0) {}
		} ObjectTextString;

		typedef struct _object_text_paragraph_entry {
			INT32 index;
			UINT16 font;
			wstring original;
			wstring translation;
			_object_text_paragraph_entry() : index(0), font(0) {}
		} ObjectTextParagraphEntry;

		typedef struct _object_text_entry {
			INT32 handle;
			string name;			// UTF-8
			string identifier;		// 'SPRI' / 'TEXT' 등
			vector<ObjectTextString> strings;
			vector<ObjectTextParagraphEntry> paragraphs;
			_object_text_entry() : handle(0) {}
		} ObjectTextEntry;

		class ObjectTextTable
		{
		public:
			ObjectTextTable();

			void clear();

			// importJson 에 성공하면 true (번역 표를 불러온 상태)
			bool isLoaded() const { return loaded; }
			// 번역 파일이 없을 때 등, 불러오기를 다시 시도하지 않도록 표시한다
			void markLoaded() { loaded = true; }
			size_t getEntryCount() const { return entries.size(); }
			const vector<ObjectTextEntry>& getEntries() const { return entries; }

			// 오브젝트 하나의 텍스트를 모은다. 모을 텍스트가 없으면 false
			bool addObject(INT32 handle, const string& name, ObjectCommon& common);

			// 핸들에 해당하는 항목, 없으면 NULL
			const ObjectTextEntry* findEntry(INT32 handle) const;

			bool exportJson(Json::Value& value) const;
			bool importJson(Json::Value& value);

		private:
			vector<ObjectTextEntry> entries;
			map<INT32, size_t> indexByHandle;
			bool loaded;
		};

		// 번역을 오브젝트에 적용한다. replacedCount = 교체한 문자열 수, skippedCount = 원문이 달라 건너뛴 수
		bool replaceObjectText(ObjectCommon& common, const ObjectTextEntry& entry, UINT32& replacedCount, UINT32& skippedCount);

	}

}
