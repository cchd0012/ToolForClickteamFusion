#pragma once

#include "ObjectCommon.h"
#include "../../Util/JsonUtil.h"

// 오브젝트 디컴파일 / 리컴파일
//
// 오브젝트 하나의 ObjectCommon 을 사람이 읽고 고칠 수 있는 JSON 으로 풀어내고(디컴파일),
// 고친 JSON 을 다시 원래 데이터에 반영한다(리컴파일).
// 풀어내는 항목은 도구가 해석하는 것들(애니메이션, Alterable 값/문자열, Text 문단)이고,
// 이동/확장 데이터처럼 해석하지 않는 하위 블록은 JSON 에 넣지 않고 원본 바이트 그대로 보존한다.
//
// JSON 형식:
//   { "type": "objectScript", "version": 1, "index": 38, "name": "letter", "objectType": 2, "identifier": "SPRI",
//     "animationSlotCount": 16,
//     "animations": [ { "slot": 12, "directions": [ { "direction": 0, "minSpeed": 0, "maxSpeed": 0,
//                                                      "repeat": 0, "backTo": 0, "frames": [ 이미지 핸들, ... ] } ] } ],
//     "alterableValues": [ 정수, ... ],
//     "alterableStrings": [ "문자열", ... ],
//     "text": { "width": 0, "height": 0, "paragraphs": [ { "font": 0, "flags": 0, "color": [r, g, b, a], "text": "..." } ] } }
// JSON 에 없는 항목은 바꾸지 않는다. 슬롯이 animationSlotCount 이상이면 슬롯 수가 늘어난다.

namespace CTT {

	namespace Chunk {

		using namespace std;

		// 오브젝트 하나를 JSON 으로 풀어낸다
		bool decompileObjectScript(INT32 index, const string& name, INT16 objectType, ObjectCommon& common, Json::Value& value);

		// JSON 을 오브젝트에 반영한다. changed = 실제로 바뀐 항목이 있는지
		bool recompileObjectScript(ObjectCommon& common, const Json::Value& value, bool& changed, string& errorMessage);

	}

}
