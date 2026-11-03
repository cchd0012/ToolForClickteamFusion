#pragma once

#include "../Common.h"

namespace CTT {

	namespace Glyph {

		using namespace std;

		// Text Blitter(txtblt.mfx) 확장에 한글을 그리게 한다 (Extensions/TxtbltKorean/NOTES.txt 참고)
		//  1) 지정한 오브젝트(예: "Text Blitter (particle)")의 문자열 번역에서 한글 음절(KS X 1001)을 모은다
		//  2) 폰트 이미지 아래에 그 음절들만 담은 아틀라스를 이어 붙여 폰트 이미지 PNG 를 덮어쓴다
		//  3) 추출해 둔 txtblt.mfx 모듈에 한글 접기 코드와 음절 표를 심는다
		// 앱 코드페이지는 949(로케일 1042)여야 한글이 2바이트로 확장에 넘어온다 (--replace 의 codepage=949 locale=1042)
		// 설정 json 은 glyphConfig 와 같은 파일에 "txtbltHangul" 블록으로 둔다:
		//   "txtbltHangul": { "module": "modules/0004_8424C67E_txtblt.mfx", "fontImage": "image/000953.png",
		//                     "objectName": "Text Blitter (particle)", "frames": [14],
		//                     "font": "binaries/DTM-Mono-RU.ttf", "size": 32, "cellWidth": 16, "cellHeight": 32 }
		// 경로는 추출 폴더 기준이다. 추출 폴더에는 image, script, text, module 이 추출되어 있어야 한다
		bool patchTxtbltHangul(const wstring& gameDir, const wstring& configPath);

		// 개발용 스위치: ToolForClickteamFusion.exe --txtblt-limited-hangul <추출 폴더> <설정 json>  (옛 이름 --txtblt-hangul 도 받는다)
		int runTxtbltHangulCommand(int argc, WCHAR** argv);

	}

}
