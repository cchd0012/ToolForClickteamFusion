// Developer switch: ToolForClickteamFusion.exe --roundtrip <archive path>
//
// Opens the archive without any window, runs the normal "save" pipeline (rewriteNewArchive) and writes
// <name>_new.<ext> next to the input. Used to verify that reading and rewriting a game keeps every chunk
// (frames / events included) intact. Compare the result with the original using an external chunk diff.

#include "Application.h"
#include "../Archive/Exe/ExeArchive.h"
#include "../Archive/Dat/DatArchive.h"
#include "../Archive/Ccn/CcnArchive.h"

// defined in Application.cpp
extern CTT::App::Application* _application;

namespace CTT { namespace App { int runHeadlessRoundTrip(int argc, WCHAR** argv); } }

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


int CTT::App::runHeadlessRoundTrip(int argc, WCHAR** argv)
{
	if (argc < 3)
	{
		wcout << L"usage: --roundtrip <archive path>" << endl;
		return 2;
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

	int rc = 1;
	if (!arc->init()) {
		wcout << L"ROUNDTRIP: archive init failed" << endl;
	}
	else {
		std::vector<std::wstring> types;
		types.push_back(L"text");
		types.push_back(L"script");
		if (arc->rewriteNewArchive(types)) {
			wcout << L"ROUNDTRIP: rewrite finished" << endl;
			rc = 0;
		}
		else {
			wcout << L"ROUNDTRIP: rewrite failed" << endl;
		}
	}

	closeApp();
	return rc;
}
