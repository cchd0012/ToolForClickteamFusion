// ToolForClickteamFusion.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include "Common.h"
#include "Application/Application.h"

using namespace CTT::App;

// Application/HeadlessApplication.cpp
namespace CTT { namespace App { int runHeadlessCommand(int argc, WCHAR** argv); } }

// Main code
int wmain(int argc, WCHAR* argv[])
{
	_setmode(_fileno(stdout), _O_U16TEXT);

	// 개발용 스위치: 창 없이 아카이브를 읽고 쓴다 (--roundtrip / --extract / --replace, Application/HeadlessApplication.cpp)
	if (argc >= 2 && (wcscmp(argv[1], L"--roundtrip") == 0 || wcscmp(argv[1], L"--extract") == 0 || wcscmp(argv[1], L"--replace") == 0)) {
		return runHeadlessCommand(argc, argv);
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