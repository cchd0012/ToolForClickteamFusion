#pragma once

#include "../Common.h"
#include <jsoncpp/json/json.h>

namespace CTT {

	namespace JsonUtil {

		using namespace std;

		// json 데이터는 UTF-8, 파일 경로는 GetAnsiStr로 string으로 바꿀 것이 강제됨
		bool readFromJson(Json::Value& value, wstring& path);
		// inlineLimit 이 0 이면 모든 객체/배열을 한 항목씩 줄바꿈해서 쓴다 (jsoncpp 기본 출력)
		// 0 보다 크면 압축했을 때 대략 inlineLimit 글자 이하인 객체/배열은 한 줄에 쓴다
		// (Frame 이벤트처럼 중첩이 깊고 항목이 많은 json 의 크기와 줄 수를 크게 줄인다)
		bool writeIntoJson(Json::Value& value, wstring& path, UINT32 inlineLimit = 0);

	}

}