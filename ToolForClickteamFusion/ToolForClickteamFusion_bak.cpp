// ToolForClickteamFusion.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include "Common.h"
#include "Archive/Exe/ExeArchive.h"
#include "Archive/Archive.h"

using namespace CTT::Archive;

int wmain(int argc, WCHAR *argv[])
{
	_setmode(_fileno(stdout), _O_U16TEXT);

	wcout << L"----------------------------------------------------" << endl;

	if (!CTT::Archive::open(L"dtge-v0.1.2.exe")) {
		close();
		return -1;
	}

	// 버전이 0x126 이상이면 Object 2.5+의 Properties 데이터가 압축되어 있다

	///*
	vector<wstring> extractingDataTypes;
	extractingDataTypes.push_back(L"appData");
	//extractingDataTypes.push_back(L"sound");
	//extractingDataTypes.push_back(L"font");
	//extractingDataTypes.push_back(L"image");
	//extractingDataTypes.push_back(L"object-full");
	getArchive()->exportFromArchive(extractingDataTypes);
	//*/

	/*
	if ((getArchive()->getArchiveType() == ArchiveType::ARCHIVE_EXE) && (contains(extractingDataTypes, L"modules"))) {
		ExeArchive* eac = (ExeArchive*)getArchive();
		eac->exportModules();
	}
	getArchive()->exportFromArchive(extractingDataTypes);
	*/

	/*
	vector<wstring> replacingDataTypes;
	replacingDataTypes.push_back(L"appInfo");
	replacingDataTypes.push_back(L"image");
	replacingDataTypes.push_back(L"sound");
	replacingDataTypes.push_back(L"font");
	getArchive()->rewriteNewArchive(replacingDataTypes);
	*/

	CTT::Archive::close();

	wcout << L"\n----------------------------------------------------" << endl;
	
	_setmode(_fileno(stdout), _O_TEXT);
}

// 프로그램 실행: <Ctrl+F5> 또는 [디버그] > [디버깅하지 않고 시작] 메뉴
// 프로그램 디버그: <F5> 키 또는 [디버그] > [디버깅 시작] 메뉴

// 시작을 위한 팁: 
//   1. [솔루션 탐색기] 창을 사용하여 파일을 추가/관리합니다.
//   2. [팀 탐색기] 창을 사용하여 소스 제어에 연결합니다.
//   3. [출력] 창을 사용하여 빌드 출력 및 기타 메시지를 확인합니다.
//   4. [오류 목록] 창을 사용하여 오류를 봅니다.
//   5. [프로젝트] > [새 항목 추가]로 이동하여 새 코드 파일을 만들거나, [프로젝트] > [기존 항목 추가]로 이동하여 기존 코드 파일을 프로젝트에 추가합니다.
//   6. 나중에 이 프로젝트를 다시 열려면 [파일] > [열기] > [프로젝트]로 이동하고 .sln 파일을 선택합니다.
