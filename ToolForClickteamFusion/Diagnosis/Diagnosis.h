#pragma once

#include <string>
#include <vector>
#include "../Common.h"
#include "../Util/JsonUtil.h"

// 진단: 아카이브와 추출해 둔 폴더를 보고 한글화를 어떻게 진행할지 안내한다.
//
// 이 모듈은 알아낸 것과 알아내지 못한 것을 나눠서 알려 준다. 항목마다 상태가 있다.
//   ok       문제 없음 / 해결 방법이 정해져 있음
//   warn     확인이 필요하거나 특별한 대응이 필요함
//   unknown  이 도구가 판단하지 못함 (그림 속 글자, 모르는 모듈판 등). 추측해서 단정하지 않는다
//   info     진행 상황 같은 참고 정보
// 분류가 애매한 것은 붙이지 않는 것이 원칙이다 (문자열이 대사인지 키인지는 판단하지 않는다).
//
// 구조: collectDiagnosisInput 이 아카이브 정보와 추출 폴더의 파일에서 사실을 모으고,
//       buildDiagnosis 가 사실만으로 항목과 진행 방향을 만든다 (디스크를 읽지 않아 시험하기 쉽다).
//
// JSON 형식 (text/diagnosis.json):
//   { "type": "diagnosis", "version": 1,
//     "archive": { "file": "x.dat", "kind": "dat", "engine": "...", "build": 292, "codepage": 1252, "locale": 1033 },
//     "summary": { "warn": 2, "unknown": 1 },
//     "items": [ { "id": "reload", "status": "warn", "title": "...", "detail": "...", "step": 3 } ],
//     "guide": [ { "text": "...", "step": 3 } ] }
// step 은 화면의 단계 번호다 (0 진단, 1 추출, 2 문구, 3 글꼴·글자, 4 이미지, 5 재합성, 6 확인, 없으면 -1).

namespace CTT {

	namespace Archive { class Archive; }

	namespace Diagnosis {

		using namespace std;

		enum DiagnosisStep
		{
			DIAG_STEP_NONE = -1,
			DIAG_STEP_DIAGNOSE = 0,
			DIAG_STEP_EXTRACT = 1,
			DIAG_STEP_TEXT = 2,
			DIAG_STEP_FONT = 3,
			DIAG_STEP_IMAGE = 4,
			DIAG_STEP_REBUILD = 5,
			DIAG_STEP_CHECK = 6
		};

		// 진단에 쓰는 사실들
		struct DiagnosisInput
		{
			// 아카이브
			string fileName;				// UTF-8
			string kind;					// exe / dat / ccn
			string engine;					// 화면에 보일 엔진 이름
			UINT32 build;
			bool codepageKnown;
			UINT32 codepage;
			bool localeKnown;
			UINT32 locale;

			// 추출한 폴더
			bool folderExists;
			bool hasScript;					// script/object 또는 script/frame
			bool hasText;					// text/frameEventText.json
			bool hasFlow;					// text/textFlow.json
			bool hasImage;

			// 글자 오브젝트 (script/object)
			int objectScriptCount;
			int blitterCount;				// 텍스트 블리터(TLBT)
			int blittersAssigned;			// 글자표를 배정한 블리터

			// txtblt 모듈
			bool txtbltFound;
			string txtbltName;				// 파일 이름 (UTF-8)
			INT64 txtbltSize;
			bool txtbltHasKfont;			// 글자표 방식으로 이미 패치됨
			bool txtbltHasKfold;			// 접기 방식으로 이미 패치됨

			// 텍스트 흐름(text/textFlow.json)과 이벤트 문자열(text/frameEventText.json)
			Json::Value flow;				// 없으면 null
			vector<string> eventTexts;

			DiagnosisInput() : build(0), codepageKnown(false), codepage(0), localeKnown(false), locale(0),
				folderExists(false), hasScript(false), hasText(false), hasFlow(false), hasImage(false),
				objectScriptCount(0), blitterCount(0), blittersAssigned(0),
				txtbltFound(false), txtbltSize(0), txtbltHasKfont(false), txtbltHasKfold(false) {}
		};

		// 사실만으로 진단 결과(JSON)를 만든다
		void buildDiagnosis(const DiagnosisInput& input, Json::Value& out);

		// 아카이브와 추출 폴더에서 사실을 모은다
		bool collectDiagnosisInput(CTT::Archive::Archive* archive, DiagnosisInput& input);

		// 진단을 실행하고, 추출 폴더가 있으면 text/diagnosis.json 으로 저장한다
		bool diagnoseArchive(CTT::Archive::Archive* archive, Json::Value& out);

		// 사람이 읽을 수 있는 한 줄 요약들 (명령줄 출력용)
		vector<string> diagnosisSummaryLines(const Json::Value& diagnosis);

	}

}
