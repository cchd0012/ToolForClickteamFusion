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
//     "qualifierAssignments": [ { "target": "qualifier#33", ... } ] }

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

		typedef struct _frame_event_flow_blitter {
			INT32 index;
			string name;
			string font;
			string text;
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
			vector<FrameEventFlowBlitter> blitters;
		};

	}

}
