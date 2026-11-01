#pragma once

#include "../Common.h"
#include <jsoncpp/json/json.h>

namespace CTT {

	namespace JsonUtil {

		using namespace std;

		// json 데이터는 UTF-8, 파일 경로는 GetAnsiStr로 string으로 바꿀 것이 강제됨
		bool readFromJson(Json::Value& value, wstring& path);
		bool writeIntoJson(Json::Value& value, wstring& path);

	}

}