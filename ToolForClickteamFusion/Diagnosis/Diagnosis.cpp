#include "Diagnosis.h"
#include "../Archive/Archive.h"
#include "../Util/CommonUtil.h"

#include <fstream>
#include <memory>
#include <sstream>

using namespace CTT::Diagnosis;
using namespace CTT::CommonUtil;
using namespace CTT::JsonUtil;

namespace {

	bool dirExists(const wstring& path)
	{
		DWORD attr = GetFileAttributesW(path.c_str());
		return (attr != INVALID_FILE_ATTRIBUTES) && ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0);
	}

	bool fileExists(const wstring& path)
	{
		DWORD attr = GetFileAttributesW(path.c_str());
		return (attr != INVALID_FILE_ATTRIBUTES) && ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0);
	}

	vector<wstring> findFiles(const wstring& dir, const wchar_t* pattern)
	{
		vector<wstring> result;
		WIN32_FIND_DATAW data;
		HANDLE handle = FindFirstFileW((dir + L"\\" + pattern).c_str(), &data);
		if (handle == INVALID_HANDLE_VALUE) {
			return result;
		}
		do {
			if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
				result.push_back(dir + L"\\" + data.cFileName);
			}
		} while (FindNextFileW(handle, &data));
		FindClose(handle);
		return result;
	}

	bool readWholeFile(const wstring& path, string& content, size_t limit)
	{
		ifstream file(path.c_str(), ios::binary);
		if (!file) {
			return false;
		}
		file.seekg(0, ios::end);
		streamoff size = file.tellg();
		if (size < 0 || (size_t)size > limit) {
			return false;
		}
		file.seekg(0, ios::beg);
		content.resize((size_t)size);
		if (size > 0) {
			file.read(&content[0], size);
		}
		return true;
	}

	bool parseJsonText(const string& text, Json::Value& value)
	{
		Json::CharReaderBuilder builder;
		unique_ptr<Json::CharReader> reader(builder.newCharReader());
		string errors;
		return reader->parse(text.data(), text.data() + text.size(), &value, &errors);
	}

	// PE 섹션 이름에 name 이 있는가 (txtblt 에 한글 패치를 이미 심었는지 보기 위함)
	bool peHasSection(const string& data, const char* name)
	{
		if (data.size() < 0x100 || data[0] != 'M' || data[1] != 'Z') {
			return false;
		}
		UINT32 e = *(const UINT32*)&data[0x3c];
		if ((size_t)e + 0x100 > data.size() || *(const UINT32*)&data[e] != 0x00004550) {
			return false;
		}
		UINT16 nsec = *(const UINT16*)&data[e + 6];
		UINT16 optSize = *(const UINT16*)&data[e + 20];
		size_t sec0 = (size_t)e + 24 + optSize;
		for (UINT16 i = 0; i < nsec; i++) {
			size_t o = sec0 + (size_t)i * 40;
			if (o + 8 > data.size()) {
				break;
			}
			if (strncmp(&data[o], name, 8) == 0) {
				return true;
			}
		}
		return false;
	}

	string engineName(CTT::Archive::EngineType type)
	{
		switch (type) {
			case CTT::Archive::EngineType::ENGINE_NORMAL:		return u8"ClickteamFusion 2.0";
			case CTT::Archive::EngineType::ENGINE_MMF_1_5:		return u8"MultimediaFusion 1.5";
			case CTT::Archive::EngineType::ENGINE_MMF_2:		return u8"ClickteamFusion 2.0 (유니코드 아님)";
			case CTT::Archive::EngineType::ENGINE_MMF_2_5_PLUS:	return u8"ClickteamFusion 2.5+";
			default:											return u8"알 수 없는 엔진";
		}
	}

}


bool CTT::Diagnosis::collectDiagnosisInput(CTT::Archive::Archive* archive, DiagnosisInput& in)
{
	if (archive == NULL) {
		return false;
	}
	wstring archivePath = archive->getArchivePath();
	// 상대 경로로 열었더라도 게임 폴더를 찾을 수 있게 절대 경로로 바꾼다
	wchar_t fullPath[MAX_PATH * 4];
	DWORD fullLength = GetFullPathNameW(archivePath.c_str(), MAX_PATH * 4, fullPath, NULL);
	if (fullLength > 0 && fullLength < MAX_PATH * 4) {
		archivePath = fullPath;
	}
	wstring baseDir = getExtentionRemovedPath(archivePath);
	wstring archiveDir = getOnlyDirPath(archivePath);

	string pathUtf8 = getAnsiStr(archivePath, CodePage::CP_UTF_8);
	size_t slash = pathUtf8.find_last_of("\\/");
	in.fileName = (slash == string::npos) ? pathUtf8 : pathUtf8.substr(slash + 1);
	switch (archive->getArchiveType()) {
		case CTT::Archive::ArchiveType::ARCHIVE_EXE:	in.kind = "exe"; break;
		case CTT::Archive::ArchiveType::ARCHIVE_DAT:	in.kind = "dat"; break;
		case CTT::Archive::ArchiveType::ARCHIVE_CCN:	in.kind = "ccn"; break;
		default:										in.kind = "?"; break;
	}
	in.engine = engineName(archive->getEngineType());
	in.build = archive->getBuildVersion();
	in.codepageKnown = archive->isAppCodepageRead();
	in.codepage = in.codepageKnown ? archive->getAppCodepage() : 0;
	in.localeKnown = archive->isAppLocaleIdRead();
	in.locale = in.localeKnown ? archive->getAppLocaleId() : 0;

	// 추출한 폴더
	in.folderExists = dirExists(baseDir);
	in.hasScript = dirExists(baseDir + L"/script/object") || dirExists(baseDir + L"/script/frame");
	in.hasText = fileExists(baseDir + L"/text/frameEventText.json");
	in.hasFlow = fileExists(baseDir + L"/text/textFlow.json");
	in.hasImage = dirExists(baseDir + L"/image");

	// 글자 오브젝트: 텍스트 블리터와 글자표 배정 수
	if (in.hasScript && dirExists(baseDir + L"/script/object")) {
		vector<wstring> files = findFiles(baseDir + L"/script/object", L"*.json");
		in.objectScriptCount = (int)files.size();
		for (size_t i = 0; i < files.size(); i++) {
			string content;
			if (!readWholeFile(files[i], content, 4u * 1024u * 1024u) || content.find("\"TLBT\"") == string::npos) {
				continue;
			}
			Json::Value value;
			if (!parseJsonText(content, value) || value.get("identifier", "").asString() != "TLBT" || !value.isMember("textBlitter")) {
				continue;
			}
			in.blitterCount++;
			if (value["textBlitter"].get("fontTable", 0).asInt() > 0) {
				in.blittersAssigned++;
			}
		}
	}

	// txtblt 모듈: 추출한 modules 폴더(exe) 또는 게임 폴더의 외부 모듈 위치
	const wstring moduleDirs[4] = { baseDir + L"/modules", archiveDir + L"/Data/Modules", archiveDir + L"/Extensions", archiveDir };
	for (int d = 0; d < 4 && !in.txtbltFound; d++) {
		if (!dirExists(moduleDirs[d])) {
			continue;
		}
		vector<wstring> found = findFiles(moduleDirs[d], L"*txtblt*.mfx");
		if (found.empty()) {
			continue;
		}
		string data;
		if (!readWholeFile(found[0], data, 64u * 1024u * 1024u)) {
			continue;
		}
		in.txtbltFound = true;
		size_t nameSlash = found[0].find_last_of(L"\\/");
		in.txtbltName = getAnsiStr(found[0].substr((nameSlash == wstring::npos) ? 0 : nameSlash + 1), CodePage::CP_UTF_8);
		in.txtbltSize = (INT64)data.size();
		in.txtbltHasKfont = peHasSection(data, ".kfont");
		in.txtbltHasKfold = peHasSection(data, ".kfold");
	}

	// 텍스트 흐름과 이벤트 문자열
	if (in.hasFlow) {
		wstring flowPath = baseDir + L"/text/textFlow.json";
		if (!readFromJson(in.flow, flowPath)) {
			in.flow = Json::Value();
		}
	}
	if (in.hasText) {
		Json::Value texts;
		wstring textPath = baseDir + L"/text/frameEventText.json";
		if (readFromJson(texts, textPath) && texts["texts"].isArray()) {
			const Json::Value& list = texts["texts"];
			for (Json::ArrayIndex i = 0; i < list.size(); i++) {
				in.eventTexts.push_back(list[i].get("original", "").asString());
			}
		}
	}
	return true;
}


bool CTT::Diagnosis::diagnoseArchive(CTT::Archive::Archive* archive, Json::Value& out)
{
	DiagnosisInput input;
	if (!collectDiagnosisInput(archive, input)) {
		return false;
	}
	buildDiagnosis(input, out);

	// 추출한 폴더가 있으면 결과를 남긴다 (게임을 비교하거나 다시 볼 때 쓴다)
	if (input.folderExists) {
		wstring archivePath = archive->getArchivePath();
		wstring baseDir = getExtentionRemovedPath(archivePath);
		CreateDirectoryW((baseDir + L"/text").c_str(), NULL);
		wstring jsonPath = baseDir + L"/text/diagnosis.json";
		writeIntoJson(out, jsonPath);
	}
	return true;
}


