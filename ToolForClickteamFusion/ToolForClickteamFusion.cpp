// ToolForClickteamFusion.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include "Common.h"
#include "Application/Application.h"

using namespace CTT::App;

// Application/HeadlessApplication.cpp
namespace CTT { namespace App { int runHeadlessCommand(int argc, WCHAR** argv); } }
// Glyph/GlyphGenerator.cpp, Glyph/TxtbltHangul.cpp, Glyph/FontAtlas.cpp, Glyph/TxtbltFontPatch.cpp
namespace CTT { namespace Glyph { int runGlyphCommand(int argc, WCHAR** argv); int runTxtbltHangulCommand(int argc, WCHAR** argv); int runFontAtlasCommand(int argc, WCHAR** argv); int runTxtbltFontCommand(int argc, WCHAR** argv); } }

// Main code
int wmain(int argc, WCHAR* argv[])
{
	_setmode(_fileno(stdout), _O_U16TEXT);

	// 개발용 스위치: 창 없이 아카이브를 읽고 쓴다 (--roundtrip / --extract / --replace, Application/HeadlessApplication.cpp)
	if (argc >= 2 && (wcscmp(argv[1], L"--roundtrip") == 0 || wcscmp(argv[1], L"--extract") == 0 || wcscmp(argv[1], L"--replace") == 0 || wcscmp(argv[1], L"--diagnose") == 0 || wcscmp(argv[1], L"--dtge-build") == 0)) {
		return runHeadlessCommand(argc, argv);
	}

	// 개발용 스위치: 추출해 둔 폴더에 글자 이미지를 만들어 넣는다 (--glyphs, Glyph/GlyphGenerator.cpp)
	if (argc >= 2 && wcscmp(argv[1], L"--glyphs") == 0) {
		return CTT::Glyph::runGlyphCommand(argc, argv);
	}

	// 개발용 스위치: 폰트 아틀라스 + 글자표를 만들고 검사한다 (--font-atlas, Glyph/FontAtlas.cpp)
	if (argc >= 2 && wcscmp(argv[1], L"--font-atlas") == 0) {
		return CTT::Glyph::runFontAtlasCommand(argc, argv);
	}

	// 개발용 스위치: txtblt(Text Blitter) 가 폰트 아틀라스 + 글자표로 글자를 그리게 한다 (--txtblt-font, Glyph/TxtbltFontPatch.cpp)
	if (argc >= 2 && wcscmp(argv[1], L"--txtblt-font") == 0) {
		return CTT::Glyph::runTxtbltFontCommand(argc, argv);
	}

	// 개발용 스위치: txtblt(Text Blitter) 119KB판(dtge)에 LimitedHangul 방식으로 한글을 그리게 한다 (--txtblt-limited-hangul, Glyph/TxtbltHangul.cpp)
	if (argc >= 2 && wcscmp(argv[1], L"--txtblt-limited-hangul") == 0) {
		return CTT::Glyph::runTxtbltHangulCommand(argc, argv);
	}

	wcout << L"----------------------------------------------------" << endl;

	// 실행 파일 이름이 dtge 로 시작하면 dtge 한글 빌더로 시작한다 (같은 프로그램을 이름만 바꿔 쓴다. 예: DtgeKoreanBuilder.exe)
	wchar_t modulePath[MAX_PATH * 2] = {};
	GetModuleFileNameW(NULL, modulePath, MAX_PATH * 2);
	const wchar_t* moduleName = wcsrchr(modulePath, L'\\');
	moduleName = (moduleName != NULL) ? moduleName + 1 : modulePath;
	const bool dtgeName = (_wcsnicmp(moduleName, L"dtge", 4) == 0);

	wcout << (dtgeName ? L"dtge Korean Builder v1.1.0 GUI" : L"ToolForClickteamFusion v1.1.0 GUI") << endl;

	if (!openApp(dtgeName ? L"dtge Korean Builder v1.1.0" : L"ToolForClickteamFusion v1.1.0")) {
		closeApp();
		return -1;
	}
	if (dtgeName) {
		getApp()->getStates()->startupDtge = true;
		getApp()->getStates()->dtgeBranding = true;
	}

	// 개발용 스위치: 시작하자마자 아카이브를 연다 (--open <아카이브> [atlas] [blitter] [module]. 한글화 도구 창을 해당 탭으로 같이 연다)
	if (argc >= 3 && wcscmp(argv[1], L"--open") == 0) {
		ApplicationState* startupStates = getApp()->getStates();
		WideCharToMultiByte(CP_UTF8, 0, argv[2], -1, startupStates->startupArchivePath, MAX_UTF8_PATH - 1, NULL, NULL);
		for (int i = 3; i < argc; i++) {
			startupStates->startupShowAtlas = startupStates->startupShowAtlas || (wcscmp(argv[i], L"atlas") == 0);
			startupStates->startupShowBlitter = startupStates->startupShowBlitter || (wcscmp(argv[i], L"blitter") == 0);
			startupStates->startupShowModule = startupStates->startupShowModule || (wcscmp(argv[i], L"module") == 0);
		}
	}

	getApp()->run();
	getApp()->terminate();

	closeApp();

	wcout << L"\n----------------------------------------------------" << endl;

	_setmode(_fileno(stdout), _O_TEXT);

	return 0;
}