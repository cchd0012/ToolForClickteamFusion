#pragma once

#include <map>
#include <set>
#include "../../../Util/JsonUtil.h"

// 텍스트 흐름 분석: 텍스트 블리터(txtblt.mfx, TLBT)가 어떤 문자열을 받아 그리는지 정리한다
//
// 블리터의 문자열은 세 군데서 온다.
//   1) 오브젝트를 만들 때 처음 들어가는 문자열 (textBlitter.text)
//   2) 이벤트의 "텍스트 설정" 동작(번호 80): 상수 / 다른 오브젝트의 알터러블 문자열 / INI 조회 / 계산식
//   3) 글꼴 이름: 알터러블 문자열 0 (동작 49 로 런타임에 바뀔 수 있다)
// 다른 오브젝트의 알터러블 문자열을 가리키는 경우, 같은 프레임에서 그 문자열을 설정하는 동작(번호 49)의
// 상수까지 따라가 "이 블리터에 실제로 나올 수 있는 문자열"을 구한다. 번역이 빠진 곳을 찾는 데 쓴다.
//
// 입력은 디컴파일한 JSON(프레임 스크립트, 오브젝트 스크립트)이라서 이벤트 모델을 다시 풀지 않는다.
//
// JSON 형식 (text/textFlow.json, 추출만 한다):
//   { "type": "textFlow", "version": 1,
//     "summary": { "assignments": 217, "literal": 129, "reference": 53, "calculation": 32, "ini": 3 },
//     "blitters": [ { "name": "...", "indices": [13], "initialTexts": ["x1"], "fonts": ["small1"],
//                     "assignments": [ { "frame": 3, "frameName": "...", "group": 12, "kind": "literal",
//                                        "literals": ["..."], "references": [ { "object": "...", "string": 1, "literals": ["..."] } ] } ],
//                     "texts": ["처음 문자열과 나올 수 있는 모든 문자열"] } ],
//     "qualifierAssignments": [ { "target": "qualifier#33", ... } ],
//     "charEvents": { "blitters": 1, "withHandlers": 0, "objects": [ { "name": "...", "handlers": 0 } ] },
//     "fontReload": { "detected": true, "loadImage": 160, "imageWidth": 160, "imageHeight": 160,
//                     "loads": [ { "frame": 3, "frameName": "...", "group": 12, "target": "...", "action": "loadImage",
//                                  "kind": "literal", "literals": ["/Data/Fonts/small1.png"] } ] } }
//
// fontReload: 게임이 실행 중에 블리터의 폰트 이미지를 바꾸는지 알려 준다 (동작 93 파일에서 불러오기, 95/96 이미지 폭/높이).
// 그러면 오브젝트에 심은 아틀라스가 덮여 글자가 깨지므로, 한글화할 때 아틀라스를 그 파일로 바꾸거나 이미지 뱅크에 넣고
// 모듈이 이 동작을 무시하게 해야 한다. 동작 번호는 37376바이트판 txtblt.mfx 기준이다 (다른 판은 번호가 다를 수 있다)

namespace CTT {

	namespace Chunk {

		using namespace std;

		// 텍스트 설정 동작의 인자 하나를 요약한 것
		typedef struct _frame_event_flow_expression {
			vector<string> literals;				// 식에 들어 있는 문자열 상수
			vector<pair<string, INT32> > references;	// 다른 오브젝트의 알터러블 문자열 (오브젝트 이름, 번호)
			bool ini;								// INI 조회가 들어 있다
			bool calculation;						// 알터러블 값 등 계산식이 들어 있다
			_frame_event_flow_expression() : ini(false), calculation(false) {}
		} FrameEventFlowExpression;

		typedef struct _frame_event_flow_assignment {
			UINT32 frame;
			string frameName;
			INT32 group;
			string target;							// 동작의 대상 오브젝트 이름 (한정자면 qualifier#N)
			FrameEventFlowExpression expression;
			_frame_event_flow_assignment() : frame(0), group(0) {}
		} FrameEventFlowAssignment;

		// 알터러블 문자열 설정 동작 (오브젝트.문자열[번호] = 식)
		typedef struct _frame_event_flow_setter {
			UINT32 frame;
			string object;
			INT32 index;
			vector<string> literals;
			_frame_event_flow_setter() : frame(0), index(0) {}
		} FrameEventFlowSetter;

		// 블리터의 폰트 이미지를 실행 중에 바꾸는 동작 (파일에서 불러오기 / 이미지 폭 / 이미지 높이)
		typedef struct _frame_event_flow_font_load {
			UINT32 frame;
			string frameName;
			INT32 group;
			string target;							// 동작의 대상 오브젝트 이름 (한정자면 qualifier#N)
			INT32 num;								// 동작 번호 (93 불러오기, 95 이미지 폭, 96 이미지 높이)
			FrameEventFlowExpression expression;	// 첫 인자 (불러오기는 파일 이름)
			_frame_event_flow_font_load() : frame(0), group(0), num(0) {}
		} FrameEventFlowFontLoad;

		typedef struct _frame_event_flow_blitter {
			INT32 index;
			string name;
			string font;
			string text;
			vector<string> flagNames;				// 옵션 플래그 이름 (오브젝트 JSON 의 textBlitter.flagNames)
			_frame_event_flow_blitter() : index(0) {}
		} FrameEventFlowBlitter;

		class FrameEventTextFlow
		{
		public:
			void clear();

			// 블리터로 쓰이는 오브젝트의 개수 (addObjectScript 로 모은 것)
			size_t getBlitterCount() const { return blitters.size(); }
			size_t getAssignmentCount() const { return assignments.size(); }

			// 디컴파일한 프레임 스크립트 JSON 하나에서 텍스트 설정 후보 동작과 문자열 설정 동작을 모은다
			void addFrameScript(UINT32 frameIndex, const Json::Value& frameScript);

			// 디컴파일한 오브젝트 스크립트 JSON 하나. 텍스트 블리터가 아니면 무시한다
			void addObjectScript(const Json::Value& objectScript);

			// 모은 것을 블리터별로 정리해 JSON 으로 만든다
			bool exportJson(Json::Value& value) const;

		private:
			vector<FrameEventFlowAssignment> assignments;
			vector<FrameEventFlowSetter> setters;
			vector<FrameEventFlowFontLoad> fontLoads;
			vector<pair<string, UINT32> > eventConditions;	// 블리터(확장 오브젝트)에 걸린 확장 조건: (오브젝트 이름, 프레임)
			vector<FrameEventFlowBlitter> blitters;
		};

	}

}
