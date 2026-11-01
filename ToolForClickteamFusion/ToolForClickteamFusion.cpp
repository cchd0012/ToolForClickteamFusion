// ToolForClickteamFusion.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include "Common.h"
#include "Application/Application.h"

using namespace CTT::App;

// Application/HeadlessApplication.cpp
namespace CTT { namespace App { int runHeadlessCommand(int argc, WCHAR** argv); } }
// Glyph/GlyphGenerator.cpp
namespace CTT { namespace Glyph { int runGlyphCommand(int argc, WCHAR** argv); int runTxtbltHangulCommand(int argc, WCHAR** argv); } }

// Main code
int wmain(int argc, WCHAR* argv[])
{
	_setmode(_fileno(stdout), _O_U16TEXT);

	// 개발용 스위치: 창 없이 아카이브를 읽고 쓴다 (--roundtrip / --extract / --replace, Application/HeadlessApplication.cpp)
	if (argc >= 2 && (wcscmp(argv[1], L"--roundtrip") == 0 || wcscmp(argv[1], L"--extract") == 0 || wcscmp(argv[1], L"--replace") == 0)) {
		return runHeadlessCommand(argc, argv);
	}

	// 개발용 스위치: 추출해 둔 폴더에 글자 이미지를 만들어 넣는다 (--glyphs, Glyph/GlyphGenerator.cpp)
	if (argc >= 2 && wcscmp(argv[1], L"--glyphs") == 0) {
		return CTT::Glyph::runGlyphCommand(argc, argv);
	}

	// 개발용 스위치: txtblt(Text Blitter) 확장에 한글을 그리게 한다 (--txtblt-hangul, Glyph/TxtbltHangul.cpp)
	if (argc >= 2 && wcscmp(argv[1], L"--txtblt-hangul") == 0) {
		return CTT::Glyph::runTxtbltHangulCommand(argc, argv);
	}

	wcout << L"----------------------------------------------------" << endl;

	wcout << L"ToolForClickteamFusion v1.0.01 GUI" << endl;

	if (!openApp(L"ToolForClickteamFusion v1.0.01")) {
		closeApp();
		return -1;
	}

	getApp()->run();
	getApp()->terminate();

	closeApp();

	wcout << L"\n----------------------------------------------------" << endl;

	_setmode(_fileno(stdout), _O_TEXT);

	return 0;
}