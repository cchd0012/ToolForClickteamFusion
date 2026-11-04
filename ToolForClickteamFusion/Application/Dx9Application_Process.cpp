#include "Dx9Application.h"

#include "../Archive/Exe/ExeArchive.h"
#include "../Archive/Dat/DatArchive.h"
#include "../Archive/Ccn/CcnArchive.h"

using namespace CTT::App;
using namespace CTT::Archive;


// 모든 프로세스를 끝내는 함수
void Dx9Application::terminateProcess()
{
	closeArchive();
}


// 아카이브 열기 함수
bool Dx9Application::openArchive()
{
	closeArchive();
	string archivePathUtf8 = states.openArchivePath;
	wstring archivePath = getUniStr(archivePathUtf8, CodePage::CP_UTF_8);
	if (archivePath.find(L".exe") != string::npos) {
		archive = new ExeArchive(archivePath);
	}
	else if (archivePath.find(L".dat") != string::npos) {
		archive = new DatArchive(archivePath);
	}
	else {
		archive = new CcnArchive(archivePath);
	}
	bool opened = archive->init();
	if (opened) {
		addShellLog(ShellLogLevel::Info, string(u8"아카이브를 열었습니다: ") + archivePathUtf8);
		shell.step = SHELL_STEP_DIAGNOSE;
		shell.problems.clear();
		shell.diagnosis = Json::Value();
		shell.diagnosed = false;
		shell.suggestDiagnosis = false;
	}
	else {
		addShellLog(ShellLogLevel::Error, string(u8"아카이브를 열지 못했습니다: ") + archivePathUtf8);
	}
	return opened;
}


// 아카이브 닫기 함수
void Dx9Application::closeArchive()
{
	if (archive != NULL) {
		delete archive;
		archive = NULL;
	}

	// 한글화 도구 창의 작업 내용도 비운다 (다른 아카이브의 것이 남지 않도록)
	hangul.blittersLoaded = false;
	hangul.atlasesLoaded = false;
	hangul.currentBlitter = -1;
	hangul.previewAtlas = -1;
	hangul.blitters.clear();
	hangul.atlases.clear();
	hangul.releaseTextures();
	hangul.objectSnap = DirSnapshot();
	hangul.flowSnap = DirSnapshot();
	hangul.atlasSnap = DirSnapshot();
	hangul.registrySnap = DirSnapshot();
	check.reset();
	eventSearch.reset();
	shell.refresh = RefreshState();
}