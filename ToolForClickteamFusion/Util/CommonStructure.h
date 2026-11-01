#pragma once

#include "CommonUtil.h"
#include "../Common.h"

namespace CTT {

	namespace CommonUtil {

		using namespace std;

		struct Color {
			BYTE r;
			BYTE g;
			BYTE b;
			BYTE a;
			// 생성자
			Color() : r(0), g(0), b(0), a(0) {}
			Color(BYTE r, BYTE g, BYTE b, BYTE a) : r(r), g(g), b(b), a(a) {}
			// 문자열로
			wstring toString() {
				wstring result;
				result = L"#" + byteToHexStr(r) + L":" + byteToHexStr(g) + L":" + byteToHexStr(b) + L":" + byteToHexStr(a);
				return result;
			}
			// 문자열값 넣기
			void fromString(const wstring& colorStr) {
				r = g = b = a = 0;
				vector<wstring> strings = split(colorStr, L"#:");
				if (strings.size() >= 1) { 
					r = (BYTE)stoul(strings[0], NULL, 16);
				}
				if (strings.size() >= 2) {
					g = (BYTE)stoul(strings[1], NULL, 16);
				}
				if (strings.size() >= 3) {
					b = (BYTE)stoul(strings[2], NULL, 16);
				}
				if (strings.size() >= 4) {
					a = (BYTE)stoul(strings[3], NULL, 16);
				}
			}
		};

	}

}