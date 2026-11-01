#pragma once

#include "FrameEventModel.h"
#include "../../../Util/JsonUtil.h"

// 프레임 이벤트 디컴파일 / 리컴파일
//
// 프레임의 이벤트(0x333D)를 구조화된 JSON 으로 풀어내고(디컴파일), 고친 JSON 을 다시 이벤트로 되돌린다(리컴파일).
// 조건/액션/파라미터는 원본의 숫자 ID(타입, 번호, 코드) 그대로 보존하며, 파라미터는 종류별로 필드를 풀어낸다.
// 풀어낸 파라미터를 다시 인코딩했을 때 원본과 같지 않으면 그 파라미터만 {"kind": "raw", "hex": ...} 로 보존하므로
// 어떤 경우에도 바이트 손실 없이 되돌릴 수 있다.
//
// 읽기 위한 주석 필드(objectName, text)는 다시 적용할 때 무시한다.
// 그룹 포인터(코드 39)는 대상 그룹의 uid 와 그룹 안 오프셋으로 기록하므로, 그룹을 넣거나 빼도 포인터가 다시 계산된다.
//
// JSON 형식(요약):
//   { "type": "frameEvents", "version": 1, "build": 292,
//     "blocks": [ { "kind": "raw", "hex": "..." }, { "kind": "size" },
//                 { "kind": "groups", "groups": [ { "uid": 0, "line": 1, "flags": 0, "restricted": 0, "cpt": 0,
//                     "conditions": [ { "objectType": -1, "objectInfo": 0, "objectList": 0, "num": -3, "flags": 0, "otherFlags": 0,
//                                       "defType": 0, "ident": 0, "params": [ { "code": 15, "kind": "expression", ... } ] } ],
//                     "actions": [ ... ] } ] } ] }

namespace CTT {

	namespace Chunk {

		using namespace std;

		// 디컴파일할 때 이름을 붙이기 위한 정보
		typedef struct _frame_event_script_context {
			vector<string> objectNames;		// 오브젝트 핸들(objectInfo) -> 이름 (UTF-8), 비어 있으면 이름을 붙이지 않는다
		} FrameEventScriptContext;

		// 이벤트 모델을 JSON 으로 풀어낸다
		bool decompileFrameEvents(const FrameEventChunkModel& model, const FrameEventScriptContext& context, Json::Value& value);

		// JSON 을 이벤트 모델에 반영한다 (build 와 그룹 헤더 형식은 모델의 것을 그대로 쓴다)
		bool recompileFrameEvents(FrameEventChunkModel& model, const Json::Value& value, string& errorMessage);

	}

}
