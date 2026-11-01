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
}