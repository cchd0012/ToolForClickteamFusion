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
//     "text": { "width": 0, "height": 0, "paragraphs": [ { "font": 0, "flags": 0, "color": [r, g, b, a], "text": "..." } ] },
//     "textBlitter": { "cellWidth": 7, "cellHeight": 9, "imageWidth": 1600, "imageHeight": 1600, "imageHandle": 2471, "fontTable": 0, "layout": "byte" } }
//   textBlitter 는 식별자가 TLBT(Text Blitter 확장)인 오브젝트의 편집 데이터 필드이다. fontTable 은 txtblt 모듈에 심은 글자표 번호(Glyph/TxtbltFontPatch)로,
//   0 이면 글자표를 쓰지 않는다(원래 방식으로 그린다). layout 은 "byte"(원래 방식: 바이트 수 x 셀 폭)와 "pixel"(글자 폭의 합)이며,
//   "pixel" 은 fontTable 이 있는 오브젝트에서만 쓰인다
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
