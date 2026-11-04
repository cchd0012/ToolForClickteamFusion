#pragma once

#include <string>
#include <vector>
#include "../Common.h"

// 이벤트 찾기 화면의 상태
//
// 추출한 폴더의 script/frame/*.json (디컴파일한 이벤트)에서 오브젝트 이름, 동작/조건 번호, 파라미터 글자로 이벤트 그룹을 찾아 보여 준다.
// 찾은 이벤트에 뜻을 아는 동작이 있으면 그 이름과 "이럴 때 이렇게 한다"는 안내를 같이 보여 준다.
// 이벤트를 고치는 일은 하지 않는다. 찾은 위치(프레임 JSON 의 어느 동작인지)를 알려 줄 뿐이고, 고친 뒤에는 3.교체의 '스크립트'로 반영한다.

namespace CTT {

	namespace App {

		using namespace std;

		struct EventSearchLine
		{
			bool isCondition;
			bool matched;				// 찾는 조건에 맞은 줄
			string text;				// UTF-8
			EventSearchLine() : isCondition(false), matched(false) {}
		};

		struct EventSearchResult
		{
			int frame;					// 프레임 번호 (JSON 파일 이름)
			int line;					// 이벤트 줄 번호
			int uid;
			vector<EventSearchLine> lines;
			int hiddenLines;			// 너무 길어서 줄인 개수
			EventSearchResult() : frame(0), line(0), uid(0), hiddenLines(0) {}
		};

		// "이 게임에 쓰인 동작/조건" 목록의 한 줄: (조건/동작, 오브젝트 종류, 번호) 마다 한 줄
		struct EventKindRow
		{
			bool isCondition;
			int objectType;
			int num;
			int count;
			vector<string> objectNames;		// 이 번호를 쓴 오브젝트 이름 예 (최대 4개)
			string sample;					// 처음 나온 이벤트의 파라미터 예
			int firstFrame;
			EventKindRow() : isCondition(false), objectType(0), num(0), count(0), firstFrame(0) {}
		};

		// 찾는 조건
		struct EventSearchCriteria
		{
			string objectName;			// 비어 있으면 모두 (대소문자 구분 없이 일부만 맞아도 된다)
			bool hasNum;
			int num;
			int scope;					// 0 = 조건과 동작, 1 = 조건만, 2 = 동작만
			string paramText;			// 파라미터의 식이나 값에 이 글자가 있는 것
			int frame;					// -1 이면 모든 프레임
			bool hasType;				// 오브젝트 종류(카운터 7, 액티브 2 ...)를 고정한다
			int objectType;
			EventSearchCriteria() : hasNum(false), num(0), scope(0), frame(-1), hasType(false), objectType(0) {}
		};

		struct EventSearchUiState
		{
			char objectName[128];
			char numText[16];
			char paramText[128];
			char frameText[16];
			int scope;
			bool hasType;				// 목록에서 고른 오브젝트 종류로 고정
			int objectType;
			char kindFilter[64];		// 목록 거르기

			// 찾는 중이면 스레드가 끝날 때 done 이 1 이 된다. 스레드는 buffer* 만 쓰고, 화면은 끝난 뒤에 results 로 옮겨 받는다
			volatile LONG running;
			volatile LONG done;
			vector<EventSearchResult> bufferResults;
			vector<string> bufferHints;
			int bufferGroups;
			int bufferFrames;
			bool bufferTruncated;
			string bufferMessage;
			bool aggregateMode;			// 이번 스레드가 목록을 만드는 일이다
			vector<EventKindRow> bufferKinds;
			vector<EventKindRow> kinds;
			bool kindsLoaded;
			bool pendingSearch;			// 목록에서 고르면 다음 프레임에 바로 찾는다

			vector<EventSearchResult> results;
			vector<string> hints;
			int totalGroups;			// 훑은 이벤트 그룹 수
			int totalFrames;
			bool truncated;
			string message;
			bool searched;

			EventSearchUiState();
			void reset();
		};

	}

}
