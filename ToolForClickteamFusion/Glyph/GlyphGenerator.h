#pragma once

#include "../Common.h"

namespace CTT {

	namespace Glyph {

		using namespace std;

		// 글자 이미지 방식(글자당 프레임 하나)의 게임에 번역문에 쓰인 글자의 이미지를 만들어 넣는다.
		// 추출해 둔 폴더(image, script, text)를 읽어, 글자 PNG 와 image/appendImages.json 을 만들고
		// 대상 오브젝트(script/object/<번호>.json)의 문자표(alterableStrings[0])와 글자 애니메이션 프레임을 늘린다.
		// 설정 json 은 글자를 그리는 스타일(폰트, 크기, 셀 높이)과 스타일이 적용될 오브젝트/슬롯을 정한다.
		// 폰트 경로는 추출 폴더 기준이다.
		bool generateGlyphs(const wstring& gameDir, const wstring& configPath);

		// 개발용 스위치: ToolForClickteamFusion.exe --glyphs <추출 폴더> <설정 json>
		int runGlyphCommand(int argc, WCHAR** argv);

	}

}
