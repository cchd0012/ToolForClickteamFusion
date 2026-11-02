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
	return archive->init();
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
}