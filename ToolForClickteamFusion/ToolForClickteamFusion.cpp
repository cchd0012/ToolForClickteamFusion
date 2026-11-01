// ToolForClickteamFusion.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include "Common.h"
#include "Application/Application.h"

using namespace CTT::App;

// Application/HeadlessApplication.cpp
namespace CTT { namespace App { int runHeadlessRoundTrip(int argc, WCHAR** argv); } }

// Main code
int wmain(int argc, WCHAR* argv[])
{
	_setmode(_fileno(stdout), _O_U16TEXT);

	// developer switch: windowless read/rewrite of an archive (ToolForClickteamFusion.exe --roundtrip <archive>)
	if (argc >= 2 && wcscmp(argv[1], L"--roundtrip") == 0) {
		return runHeadlessRoundTrip(argc, argv);
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