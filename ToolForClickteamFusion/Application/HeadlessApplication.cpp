// 개발용 스위치: 창을 띄우지 않고 아카이브를 읽고 쓴다.
//
//   ToolForClickteamFusion.exe --roundtrip <아카이브 경로> [데이터 종류...]   읽은 뒤 그대로 다시 쓴다 (기본: script)
//   ToolForClickteamFusion.exe --extract   <아카이브 경로> [데이터 종류...]   데이터를 추출한다 (기본: text)
//   ToolForClickteamFusion.exe --replace   <아카이브 경로> [데이터 종류...]   추출해 둔 데이터로 교체한다 (기본: text)
//                                          데이터 종류 자리에 codepage=949 locale=1042 를 적으면 앱 코드페이지/로케일도 바꾼다
//
// --roundtrip / --replace 는 일반 "저장" 과정(rewriteNewArchive)을 실행하고, 입력 파일 옆에
// <이름>_new.<확장자> 로 기록한다. 게임을 읽고 다시 쓸 때 모든 청크(프레임/이벤트 포함)가 그대로 유지되는지
// 검증하는 용도이다. 결과는 외부 청크 비교 도구로 원본과 비교한다.
// --extract 는 아카이브 옆의 <이름> 폴더에 데이터를 추출한다.
// 데이터 종류 "flow" 는 텍스트 블리터가 받는 문자열의 출처를 분석해 text/textFlow.json 으로 저장한다 (추출 전용).

#include "Application.h"
#include "../Archive/Exe/ExeArchive.h"
#include "../Archive/Dat/DatArchive.h"
#include "../Archive/Ccn/CcnArchive.h"

// Application.cpp 에 정의되어 있다
extern CTT::App::Application* _application;

namespace CTT { namespace App { int runHeadlessCommand(int argc, WCHAR** argv); } }

using namespace CTT::App;
using namespace CTT::Archive;

namespace {

	class HeadlessApplication : public Application
	{
	public:
		HeadlessApplication() : Application(L"headless") { initializeAppState(); }
		~HeadlessApplication() {}

		bool init() { return true; }
		void run() {}
		void terminate() {}

		void setArchive(CTT::Archive::Archive* a) { archive = a; }

	private:
		void render() {}
		void terminateProcess() {}
	};

}


int CTT::App::runHeadlessCommand(int argc, WCHAR** argv)
{
	std::wstring mode = (argc >= 2) ? argv[1] : L"";
	if (argc < 3) {
		wcout << L"usage: " << mode << L" <archive path> [data type ...]" << endl;
		return 2;
	}

	// 데이터 종류를 지정하지 않으면 명령에 맞는 기본값을 쓴다
	std::vector<std::wstring> types;
	for (int i = 3; i < argc; i++) {
		types.push_back(argv[i]);
	}
	if (types.empty()) {
		types.push_back((mode == L"--roundtrip") ? L"script" : L"text");
	}

	std::wstring path = argv[2];
	HeadlessApplication* app = new HeadlessApplication();
	_application = app;

	CTT::Archive::Archive* arc = NULL;
	if (path.find(L".exe") != std::wstring::npos) {
		arc = new ExeArchive(path);
	}
	else if (path.find(L".dat") != std::wstring::npos) {
		arc = new DatArchive(path);
	}
	else {
		arc = new CcnArchive(path);
	}
	app->setArchive(arc);

	int resultCode = 1;
	if (!arc->init()) {
		wcout << L"HEADLESS: archive init failed" << endl;
	}
	else {
		bool success;
		if (mode == L"--extract") {
			arc->setDataCount(types);
			success = arc->exportFromArchive(types);
		}
		else {
			// 데이터 종류 자리에 codepage=949, locale=1042 를 적으면 앱 정보의 코드페이지/로케일을 바꿔서 저장한다
			for (size_t i = 0; i < types.size(); ) {
				if (types[i].compare(0, 9, L"codepage=") == 0) {
					arc->setAppCodepageFromEditor((UINT32)_wtoi(types[i].c_str() + 9));
					types.erase(types.begin() + i);
				}
				else if (types[i].compare(0, 7, L"locale=") == 0) {
					arc->setAppLocaleIdFromEditor((UINT32)_wtoi(types[i].c_str() + 7));
					types.erase(types.begin() + i);
				}
				else {
					i++;
				}
			}
			if (types.empty()) {
				types.push_back(L"script");
			}
			success = arc->rewriteNewArchive(types);
		}
		if (success) {
			wcout << L"HEADLESS: " << mode << L" finished" << endl;
			resultCode = 0;
		}
		else {
			wcout << L"HEADLESS: " << mode << L" failed" << endl;
		}
	}

	closeApp();
	return resultCode;
}
