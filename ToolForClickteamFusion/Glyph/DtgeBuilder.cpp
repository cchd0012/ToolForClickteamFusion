#include "DtgeBuilder.h"
#include "DtgeEmbedded.h"
#include "GlyphGenerator.h"
#include "TxtbltHangul.h"
#include "../Archive/Exe/ExeArchive.h"
#include "../Application/Application.h"
#include "../Util/CommonUtil.h"
#include "../Util/JsonUtil.h"

using namespace CTT::Dtge;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;
using namespace CTT::JsonUtil;


namespace {
	wstring joinPath(const wstring& a, const wstring& b);

	const INT64 kDtge014Size = 107693336;		// 0.1.4 원본 exe 의 크기 (다르면 경고만 한다)

	// 글자 오브젝트 번호는 버전마다 다르다. 0.1.4: 이름이 letter 인 8개 (문자표 95자)
	const int kLetterObjects[8] = { 38, 61, 321, 349, 520, 522, 556, 579 };

	// 한글 폰트로 바꾸는 14개 자리 (docs/dtge/apply_ko_fonts.py 와 같은 대응). 앞 5개는 내장 TTF(font/ttf/), 뒤 9개는 설치용 TTF(binaries/)
	struct FontSlot
	{
		bool embedded;				// true 면 font/ttf/<target>, false 면 binaries/<target>
		const char* target;			// 바꿀 파일 이름
		const char* defaultSource;	// 폰트 폴더에서 기본으로 쓰는 파일
		const char* role;			// 어디에 쓰이는지 (UTF-8)
	};

	const FontSlot kFontSlots[CTT::Dtge::kDtgeFontSlotCount] = {
		{ true, "000000.ttf", "000003.ttf", u8"네이티브 글자: Determination Sans" },
		{ true, "000001.ttf", "000000.ttf", u8"네이티브 글자: Mars Needs Cunnilingus" },
		{ true, "000002.ttf", "000001.ttf", u8"네이티브 글자: Determination Mono (RUS)" },
		{ true, "000003.ttf", "000005.ttf", u8"네이티브 글자: Undertale Battle Font" },
		{ true, "000005.ttf", "000007.ttf", u8"네이티브 글자: Undertale Sans" },
		{ false, "DTM-Mono-RU.ttf", "000001.ttf", u8"대사 글씨체 1 (기본 대사, 글자 이미지)" },
		{ false, "17838.ttf", "000006.ttf", u8"대사 글씨체 2 (Sans, 말풍선, 글자 이미지) / 파티클 문구" },
		{ false, "14613.ttf", "000009.ttf", u8"대사 글씨체 3 (Papyrus, 글자 이미지)" },
		{ false, "pixelated-wingdings.ttf", "000008.ttf", u8"글리치 연출 (윙딩스풍 기호, 글자 이미지)" },
		{ false, "DTM-Mono.ttf", "000004.ttf", u8"게임 시작 때 설치하는 글꼴: DTM-Mono" },
		{ false, "DTM-Sans.ttf", "000003.ttf", u8"게임 시작 때 설치하는 글꼴: DTM-Sans" },
		{ false, "Mars_Needs_Cunnilingus.ttf", "000000.ttf", u8"게임 시작 때 설치하는 글꼴: Mars Needs Cunnilingus" },
		{ false, "Undertale-Battle-Font.ttf", "000005.ttf", u8"게임 시작 때 설치하는 글꼴: Undertale Battle Font" },
		{ false, "UndertaleSans.ttf", "000007.ttf", u8"게임 시작 때 설치하는 글꼴: Undertale Sans" },
	};

	// 자리 하나에 쓸 폰트 파일: 직접 고른 것이 있으면 그것, 없으면 폰트 폴더의 기본 파일
	wstring fontSourceOf(const DtgeOptions& o, int slot)
	{
		if (!o.fontOverride[slot].empty()) {
			return o.fontOverride[slot];
		}
		return o.fontDir.empty() ? wstring() : joinPath(o.fontDir, getUniStr(kFontSlots[slot].defaultSource, CodePage::CP_UTF_8));
	}

	const char* kStepNames[CTT::Dtge::kDtgeStepCount] = {
		u8"추출", u8"한글 폰트 교체", u8"번역 넣기", u8"대사 글자 이미지", u8"파티클 문구", u8"유니코드 파서 모듈", u8"재합성"
	};

	struct Ctx
	{
		DtgeLogFn log;
		void* user;

		void say(int step, int state, const string& text) const
		{
			if (log != NULL) {
				log(user, step, state, text);
			}
		}
	};

	string utf8(const wstring& w)
	{
		return getAnsiStr(w, CodePage::CP_UTF_8);
	}

	wstring joinPath(const wstring& a, const wstring& b)
	{
		if (a.empty() || a[a.size() - 1] == L'\\' || a[a.size() - 1] == L'/') {
			return a + b;
		}
		return a + L"\\" + b;
	}

	bool fileExists(const wstring& path)
	{
		DWORD attr = GetFileAttributesW(path.c_str());
		return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
	}

	bool dirExists(const wstring& path)
	{
		DWORD attr = GetFileAttributesW(path.c_str());
		return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}

	INT64 fileSize(const wstring& path)
	{
		WIN32_FILE_ATTRIBUTE_DATA data;
		if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
			return -1;
		}
		return ((INT64)data.nFileSizeHigh << 32) | data.nFileSizeLow;
	}

	bool writeBytes(const wstring& path, const void* data, size_t size, const void* prefix = NULL, size_t prefixSize = 0)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (h == INVALID_HANDLE_VALUE) {
			return false;
		}
		DWORD written = 0;
		bool ok = true;
		if (prefixSize > 0) {
			ok = WriteFile(h, prefix, (DWORD)prefixSize, &written, NULL) != 0;
		}
		if (ok) {
			ok = WriteFile(h, data, (DWORD)size, &written, NULL) != 0 && written == size;
		}
		CloseHandle(h);
		return ok;
	}

	// 폴더 안에서 패턴에 맞는 파일 이름을 모은다 (폴더가 없으면 빈 목록)
	vector<wstring> listFiles(const wstring& dir, const wstring& pattern)
	{
		vector<wstring> names;
		WIN32_FIND_DATAW find;
		HANDLE h = FindFirstFileW(joinPath(dir, pattern).c_str(), &find);
		if (h == INVALID_HANDLE_VALUE) {
			return names;
		}
		do {
			if ((find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
				names.push_back(find.cFileName);
			}
		} while (FindNextFileW(h, &find));
		FindClose(h);
		return names;
	}

	// 아직 없는 결과 폴더 이름을 정한다: <이름>, <이름>_2, <이름>_3 ...  (만들지는 않는다)
	wstring chooseNewFolder(const wstring& parent, const wstring& baseName)
	{
		wstring candidate = joinPath(parent, baseName);
		for (int n = 2; GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES && n < 1000; n++) {
			candidate = joinPath(parent, baseName + L"_" + to_wstring(n));
		}
		return candidate;
	}

	wstring baseNameOf(const wstring& path)
	{
		size_t slash = path.find_last_of(L"\\/");
		wstring name = (slash == wstring::npos) ? path : path.substr(slash + 1);
		size_t dot = name.find_last_of(L'.');
		return (dot == wstring::npos) ? name : name.substr(0, dot);
	}

	// 추출/교체에 쓰는 아카이브를 만든다 (호출한 쪽이 delete 한다)
	CTT::Archive::Archive* openExe(const wstring& exePath)
	{
		CTT::Archive::Archive* arc = new ExeArchive(exePath);
		// 아카이브 코드는 getArchive() (= 앱이 들고 있는 현재 아카이브) 를 통해 자기 자신을 찾으므로 init 전에 등록한다
		CTT::App::getApp()->setCurrentArchive(arc);
		if (!arc->init()) {
			CTT::App::getApp()->setCurrentArchive(NULL);
			delete arc;
			return NULL;
		}
		return arc;
	}

	// openExe 로 연 아카이브를 해제한다
	void closeExe(CTT::Archive::Archive* arc)
	{
		CTT::App::getApp()->setCurrentArchive(NULL);
		delete arc;
	}

	// ---- 1. 추출
	bool stepExtract(const Ctx& c, const wstring& workExe)
	{
		CTT::Archive::Archive* arc = openExe(workExe);
		if (arc == NULL) {
			c.say(0, 3, u8"원본 exe 를 열지 못했습니다. 클릭팀 퓨전 게임(exe)이 맞는지 확인하세요");
			return false;
		}
		vector<wstring> types;
		types.push_back(L"font");
		types.push_back(L"blobs");
		types.push_back(L"image");
		types.push_back(L"script");
		types.push_back(L"text");
		types.push_back(L"module");
		types.push_back(L"flow");
		arc->setDataCount(types);
		bool ok = arc->exportFromArchive(types);
		closeExe(arc);
		if (!ok) {
			c.say(0, 3, u8"추출하지 못했습니다");
		}
		return ok;
	}

	// 폴더 안의 모든 파일을 root 기준 상대 경로로 모은다 (링크/접합 지점은 따라가지 않는다)
	void collectFiles(const wstring& root, const wstring& rel, vector<wstring>& out)
	{
		wstring dir = rel.empty() ? root : root + L"\\" + rel;
		WIN32_FIND_DATAW fd;
		HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
		if (h == INVALID_HANDLE_VALUE) {
			return;
		}
		do {
			wstring name = fd.cFileName;
			if (name == L"." || name == L"..") {
				continue;
			}
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
				continue;
			}
			wstring childRel = rel.empty() ? name : rel + L"\\" + name;
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				collectFiles(root, childRel, out);
			}
			else {
				out.push_back(childRel);
			}
		} while (FindNextFileW(h, &fd));
		FindClose(h);
	}

	bool readAllBytes(const wstring& path, string& out)
	{
		out.clear();
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (h == INVALID_HANDLE_VALUE) {
			return false;
		}
		LARGE_INTEGER size;
		bool ok = GetFileSizeEx(h, &size) != 0 && size.QuadPart < (LONGLONG)256 * 1024 * 1024;
		if (ok && size.QuadPart > 0) {
			out.resize((size_t)size.QuadPart);
			DWORD read = 0;
			ok = ReadFile(h, &out[0], (DWORD)size.QuadPart, &read, NULL) != 0 && read == (DWORD)size.QuadPart;
		}
		CloseHandle(h);
		return ok;
	}

	// ---- 3-1. 고친 스크립트 덮어쓰기: overrideDir\script\... 의 파일을 추출한 폴더의 같은 경로에 덮어쓴다.
	// 재합성에서는 스크립트가 먼저 반영되고 그 위에 번역 문구가 얹히므로, 스크립트 파일에는 원문(영어)을 그대로 둔 채 숫자/구조만 고친다.
	// 추출한 폴더에 이미 있는 파일만 덮어쓰고(오타로 엉뚱한 파일이 생기지 않게), 원본과 내용이 같은 파일은 건너뛴다.
	bool stepOverrides(const Ctx& c, const DtgeOptions& o, const wstring& gameDir)
	{
		wstring scriptRoot = joinPath(o.overrideDir, L"script");
		if (!dirExists(scriptRoot)) {
			return true;	// script 폴더가 없으면 스크립트는 건드리지 않는다 (image 폴더만 있어도 된다)
		}
		vector<wstring> files;
		collectFiles(scriptRoot, wstring(), files);
		int changed = 0;
		int skippedSame = 0;
		int skippedMissing = 0;
		for (size_t i = 0; i < files.size(); i++) {
			wstring rel = L"script\\" + files[i];
			wstring from = joinPath(scriptRoot, files[i]);
			wstring to = joinPath(gameDir, rel);
			if (!fileExists(to)) {
				skippedMissing++;
				c.say(2, 0, string(u8"원본에 없는 파일이라 건너뜁니다: ") + utf8(rel));
				continue;
			}
			string a, b;
			if (readAllBytes(from, a) && readAllBytes(to, b) && a == b) {
				skippedSame++;
				continue;
			}
			if (!CopyFileW(from.c_str(), to.c_str(), FALSE)) {
				c.say(2, 3, string(u8"스크립트를 덮어쓰지 못했습니다: ") + utf8(rel));
				return false;
			}
			changed++;
			c.say(2, 0, string(u8"스크립트를 덮어썼습니다: ") + utf8(rel));
		}
		c.say(2, 0, string(u8"고친 스크립트 ") + to_string(changed) + u8"개를 반영했습니다 (원본과 같아 건너뜀 " + to_string(skippedSame) + u8"개, 원본에 없어 건너뜀 " + to_string(skippedMissing) + u8"개)");
		return true;
	}

	// ---- 4-1. 고친 이미지 덮어쓰기: overrideDir\image\<번호>.png 를 추출한 image 폴더에 넣는다.
	// 글자 이미지 단계가 원본 PNG 를 모두 치우므로, (a) 그 전에 원본과 내용이 다른 파일만 골라 두고 (b) 그 단계가 끝난 뒤에 넣는다.
	// 그러면 재합성 때 같은 번호의 이미지가 교체된다. 원본을 추출했을 때 있던 번호만 받는다 (오타로 새 이미지가 생기지 않게).
	// 000953.png 는 파티클 문구 글꼴 이미지라 건드리지 않는다. 추출 폴더 전체를 지정해도 원본과 같은 파일은 건너뛰므로 안전하다.
	wstring imageOverrideRoot(const DtgeOptions& o)
	{
		return !o.imageDir.empty() ? o.imageDir : joinPath(o.overrideDir, L"image");
	}

	void selectChangedImages(const Ctx& c, const DtgeOptions& o, const wstring& gameDir, vector<wstring>& selected)
	{
		selected.clear();
		wstring imageRoot = imageOverrideRoot(o);
		if (!dirExists(imageRoot)) {
			return;
		}
		vector<wstring> files = listFiles(imageRoot, L"*.png");
		int same = 0;
		for (size_t i = 0; i < files.size(); i++) {
			wstring original = joinPath(joinPath(gameDir, L"image"), files[i]);
			if (_wcsicmp(files[i].c_str(), L"000953.png") == 0 || !fileExists(original)) {
				c.say(3, 0, string(u8"원본에 없거나 건드리면 안 되는 이미지라 건너뜁니다: ") + utf8(files[i]));
				continue;
			}
			string a, b;
			if (readAllBytes(joinPath(imageRoot, files[i]), a) && readAllBytes(original, b) && a == b) {
				same++;
				continue;
			}
			selected.push_back(files[i]);
		}
		if (same > 0) {
			c.say(3, 0, string(u8"원본과 같은 이미지 ") + to_string(same) + u8"개는 건너뜁니다");
		}
	}

	bool applyChangedImages(const Ctx& c, const DtgeOptions& o, const wstring& gameDir, const vector<wstring>& selected)
	{
		wstring imageRoot = imageOverrideRoot(o);
		for (size_t i = 0; i < selected.size(); i++) {
			wstring to = joinPath(joinPath(gameDir, L"image"), selected[i]);
			if (!CopyFileW(joinPath(imageRoot, selected[i]).c_str(), to.c_str(), FALSE)) {
				c.say(3, 3, string(u8"이미지를 덮어쓰지 못했습니다: ") + utf8(selected[i]));
				return false;
			}
		}
		if (!selected.empty()) {
			c.say(3, 0, string(u8"고친 이미지 ") + to_string(selected.size()) + u8"개를 반영했습니다");
		}
		return true;
	}

	// ---- 2. 한글 폰트 교체: 내장 TTF 와 binaries 의 TTF 를 한글 폰트로 바꾸고 LogFont 의 charset 을 129(한글)로
	bool stepFonts(const Ctx& c, const DtgeOptions& o, const wstring& gameDir)
	{
		for (int i = 0; i < CTT::Dtge::kDtgeFontSlotCount; i++) {
			wstring from = fontSourceOf(o, i);
			wstring to = joinPath(joinPath(gameDir, kFontSlots[i].embedded ? L"font\\ttf" : L"binaries"), getUniStr(kFontSlots[i].target, CodePage::CP_UTF_8));
			if (!fileExists(to)) {
				c.say(1, 3, string(u8"원본에 없는 파일이라 중단합니다 (dtge 0.1.4 가 아닌 것 같습니다): ") + (kFontSlots[i].embedded ? "font/ttf/" : "binaries/") + kFontSlots[i].target);
				return false;
			}
			if (!CopyFileW(from.c_str(), to.c_str(), FALSE)) {
				c.say(1, 3, string(u8"폰트를 복사하지 못했습니다: ") + utf8(from) + u8" -> " + (kFontSlots[i].embedded ? "font/ttf/" : "binaries/") + kFontSlots[i].target);
				return false;
			}
		}
		const wchar_t* jsonFiles[2] = { L"font\\ttf\\info.json", L"font\\logFontInfo.json" };
		for (int f = 0; f < 2; f++) {
			wstring path = joinPath(gameDir, jsonFiles[f]);
			Json::Value root;
			if (!readFromJson(root, path) || !root.isArray()) {
				c.say(1, 3, string(u8"글꼴 정보를 읽지 못했습니다: ") + utf8(jsonFiles[f]));
				return false;
			}
			for (Json::ArrayIndex i = 0; i < root.size(); i++) {
				root[i]["charset"] = 129;
			}
			if (!writeIntoJson(root, path)) {
				c.say(1, 3, string(u8"글꼴 정보를 저장하지 못했습니다: ") + utf8(jsonFiles[f]));
				return false;
			}
		}
		int custom = 0;
		for (int i = 0; i < CTT::Dtge::kDtgeFontSlotCount; i++) {
			custom += o.fontOverride[i].empty() ? 0 : 1;
		}
		c.say(1, 0, string(u8"폰트 14개를 바꾸고(직접 고른 것 ") + to_string(custom) + u8"개) 글꼴의 charset 을 129 로 맞췄습니다");
		return true;
	}

	// ---- 3-1. (시험용) 플레이어 HP 가 닳지 않게: 전투 프레임의 HP 카운터 빼기 동작(카운터 동작 82)을 0 으로
	void patchHpActions(Json::Value& node, int& count)
	{
		if (node.isObject()) {
			// 동작은 params, num, objectName 이 있고 ident 가 없다 (조건에는 ident 가 있다)
			if (node.isMember("params") && node.isMember("num") && node.isMember("objectName") && !node.isMember("ident")
				&& node.get("objectName", "").asString() == "HP" && node.get("objectType", 0).asInt() == 7 && node.get("num", 0).asInt() == 82) {
				Json::Value& params = node["params"];
				if (params.isArray() && params.size() == 1 && params[0u].get("kind", "").asString() == "expression") {
					Json::Value token(Json::objectValue);
					token["kind"] = "long";
					token["num"] = 0;
					token["objectType"] = -1;
					token["value"] = 0;
					Json::Value tokens(Json::arrayValue);
					tokens.append(token);
					params[0u]["tokens"] = tokens;
					params[0u]["text"] = "0";
					params[0u].removeMember("tail");
					count++;
				}
			}
			for (Json::Value::iterator it = node.begin(); it != node.end(); ++it) {
				if ((*it).isObject() || (*it).isArray()) {
					patchHpActions(*it, count);
				}
			}
		}
		else if (node.isArray()) {
			for (Json::ArrayIndex i = 0; i < node.size(); i++) {
				if (node[i].isObject() || node[i].isArray()) {
					patchHpActions(node[i], count);
				}
			}
		}
	}

	bool stepNoDamage(const Ctx& c, const wstring& gameDir)
	{
		wstring path = joinPath(gameDir, L"script\\frame\\000014.json");
		Json::Value root;
		if (!readFromJson(root, path)) {
			c.say(6, 3, u8"전투 프레임(000014.json)을 읽지 못했습니다");
			return false;
		}
		int count = 0;
		patchHpActions(root, count);
		if (count == 0) {
			c.say(6, 3, u8"HP 를 빼는 동작을 찾지 못했습니다 (dtge 0.1.4 가 아닌 것 같습니다)");
			return false;
		}
		if (!writeIntoJson(root, path, 700)) {
			c.say(6, 3, u8"전투 프레임을 저장하지 못했습니다");
			return false;
		}
		c.say(6, 0, string(u8"HP 빼기 동작 ") + to_string(count) + u8"개를 0 으로 바꿨습니다 (시험용)");
		return true;
	}

	// ---- 4. 대사 글자 이미지: 글자 오브젝트가 0.1.4 의 것인지 확인한 뒤 만든다
	bool checkLetterObjects(const Ctx& c, const wstring& gameDir)
	{
		for (int i = 0; i < 8; i++) {
			wchar_t name[32];
			swprintf_s(name, L"script\\object\\%06d.json", kLetterObjects[i]);
			wstring path = joinPath(gameDir, name);
			Json::Value root;
			if (!readFromJson(root, path)) {
				c.say(3, 3, string(u8"글자 오브젝트 ") + to_string(kLetterObjects[i]) + u8" 을(를) 읽지 못했습니다 (dtge 0.1.4 가 아닌 것 같습니다)");
				return false;
			}
			const string objectName = root.get("name", "").asString();
			const Json::Value& strings = root["alterableStrings"];
			const size_t charsetLength = (strings.isArray() && strings.size() > 0) ? strings[0u].asString().size() : 0;
			if ((objectName != "letter" && objectName != "letter bubble") || charsetLength != 95) {
				c.say(3, 3, string(u8"오브젝트 ") + to_string(kLetterObjects[i]) + u8" 이(가) 글자 오브젝트가 아닙니다 (이름 '" + objectName + u8"'). dtge 0.1.4 원본이 맞는지 확인하세요");
				return false;
			}
		}
		return true;
	}

	bool stepGlyphs(const Ctx& c, const wstring& workDir, const wstring& gameDir, const wstring& configPath)
	{
		if (!checkLetterObjects(c, gameDir)) {
			return false;
		}
		// 이미지 교체는 image 폴더의 PNG 를 모두 다시 읽으므로 원본 PNG 는 치운다. 000953.png(파티클 문구 글꼴 이미지)만 남긴다
		// (이 폴더는 이번 실행이 새로 만든 폴더 안의 추출 결과이다)
		wstring imageDir = joinPath(gameDir, L"image");
		int removed = 0;
		vector<wstring> pngs = listFiles(imageDir, L"0*.png");
		for (size_t i = 0; i < pngs.size(); i++) {
			if (pngs[i] != L"000953.png") {
				if (DeleteFileW(joinPath(imageDir, pngs[i]).c_str())) {
					removed++;
				}
			}
		}
		const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
		if (!writeBytes(configPath, kDtgeGlyphConfigJson, strlen(kDtgeGlyphConfigJson), bom, 3)) {
			c.say(3, 3, u8"글자 이미지 설정 파일을 쓰지 못했습니다");
			return false;
		}
		if (!CTT::Glyph::generateGlyphs(gameDir, configPath)) {
			c.say(3, 3, u8"글자 이미지를 만들지 못했습니다 (아래 로그와 콘솔 출력을 확인하세요)");
			return false;
		}
		wstring appendPath = joinPath(imageDir, L"appendImages.json");
		Json::Value append;
		int added = 0;
		if (readFromJson(append, appendPath) && append.isMember("images")) {
			added = (int)append["images"].size();
		}
		c.say(3, 0, string(u8"원본 이미지 ") + to_string(removed) + u8"장을 치우고 대사 글자 이미지 " + to_string(added) + u8"개를 만들었습니다");
		(void)workDir;
		return true;
	}

	// ---- 5. 파티클 문구
	bool stepParticles(const Ctx& c, const wstring& gameDir, const wstring& configPath)
	{
		if (!CTT::Glyph::patchTxtbltHangul(gameDir, configPath)) {
			c.say(4, 3, u8"파티클 문구용 모듈 패치에 실패했습니다");
			return false;
		}
		return true;
	}

	// ---- 6. 유니코드 파서 모듈 (대사 줄 정렬에 필요하다)
	bool stepParser(const Ctx& c, const wstring& gameDir)
	{
		wstring modulesDir = joinPath(gameDir, L"modules");
		vector<wstring> old = listFiles(modulesDir, L"0020_*_parser.mfx");
		for (size_t i = 0; i < old.size(); i++) {
			DeleteFileW(joinPath(modulesDir, old[i]).c_str());
		}
		if (!writeBytes(joinPath(modulesDir, L"0020_00000000_parser.mfx"), kDtgeParserModule, sizeof(kDtgeParserModule))) {
			c.say(5, 3, u8"파서 모듈을 쓰지 못했습니다");
			return false;
		}
		c.say(5, 0, string(u8"유니코드 파서 모듈을 넣었습니다 (") + to_string(sizeof(kDtgeParserModule)) + u8"바이트)");
		return true;
	}

	bool stepReplace(const Ctx& c, const wstring& workExe);

	// 재합성해서 나온 새 exe 를 알아보기 쉬운 이름으로 바꾼다 (결과 폴더 안에 새로 만든 파일이라 겹치지 않는다)
	bool makeKoreanExe(const Ctx& c, const wstring& workExe, const wstring& builtExe, const wstring& finalExe, wstring& outExe)
	{
		if (fileExists(builtExe) || fileExists(finalExe)) {
			c.say(6, 3, u8"결과 폴더에 이미 같은 이름의 파일이 있어 멈춥니다");
			return false;
		}
		if (!stepReplace(c, workExe)) {
			return false;
		}
		if (!fileExists(builtExe)) {
			c.say(6, 3, u8"새 exe 가 만들어지지 않았습니다");
			return false;
		}
		outExe = MoveFileW(builtExe.c_str(), finalExe.c_str()) ? finalExe : builtExe;
		return true;
	}

	// ---- 7. 재합성: 코드페이지 949 / 로케일 1042 로 새 exe (<이름>_new.exe) 를 만든다
	bool stepReplace(const Ctx& c, const wstring& workExe)
	{
		CTT::Archive::Archive* arc = openExe(workExe);
		if (arc == NULL) {
			c.say(6, 3, u8"작업용 exe 를 다시 열지 못했습니다");
			return false;
		}
		arc->setAppCodepageFromEditor(949);
		arc->setAppLocaleIdFromEditor(1042);
		vector<wstring> types;
		types.push_back(L"font");
		types.push_back(L"blobs");
		types.push_back(L"image");
		types.push_back(L"script");
		types.push_back(L"text");
		types.push_back(L"module");
		bool ok = arc->rewriteNewArchive(types);
		closeExe(arc);
		if (!ok) {
			c.say(6, 3, u8"재합성하지 못했습니다");
		}
		return ok;
	}

}


const char* CTT::Dtge::dtgeStepName(int step)
{
	return (step >= 0 && step < kDtgeStepCount) ? kStepNames[step] : "";
}


void CTT::Dtge::checkDtgeInputs(const DtgeOptions& o, vector<DtgeCheck>& checks)
{
	checks.assign(6, DtgeCheck());

	// 0. 원본 exe
	if (o.originalExe.empty()) {
		checks[0].level = 0;
		checks[0].text = u8"원본 dtge exe 를 고르세요";
	}
	else if (!fileExists(o.originalExe)) {
		checks[0].level = 0;
		checks[0].text = u8"파일이 없습니다";
	}
	else {
		INT64 size = fileSize(o.originalExe);
		if (size == kDtge014Size) {
			checks[0].level = 2;
			checks[0].text = u8"dtge 0.1.4 원본으로 보입니다 (크기가 같습니다)";
		}
		else {
			checks[0].level = 1;
			checks[0].text = string(u8"크기가 0.1.4 원본(107,693,336바이트)과 다릅니다 (") + to_string(size) + u8"바이트). 다른 버전이면 중간에 멈출 수 있습니다";
		}
	}

	// 1. 번역 폴더
	if (o.translationDir.empty()) {
		checks[1].level = 0;
		checks[1].text = u8"번역 폴더를 고르세요 (frameEventText.json, objectText.json 이 든 폴더)";
	}
	else {
		bool a = fileExists(joinPath(o.translationDir, L"frameEventText.json"));
		bool b = fileExists(joinPath(o.translationDir, L"objectText.json"));
		if (a && b) {
			checks[1].level = 2;
			checks[1].text = u8"frameEventText.json, objectText.json 이 있습니다";
		}
		else {
			checks[1].level = 0;
			checks[1].text = string(u8"없는 파일: ") + (a ? "" : "frameEventText.json ") + (b ? "" : "objectText.json");
		}
	}

	// 2. 한글 폰트: 자리 14개마다 쓸 파일이 있는지 (직접 고른 파일이 있으면 그것, 없으면 폰트 폴더의 기본 파일)
	{
		string missing;
		int found = 0;
		int custom = 0;
		for (int i = 0; i < kDtgeFontSlotCount; i++) {
			wstring source = fontSourceOf(o, i);
			custom += o.fontOverride[i].empty() ? 0 : 1;
			if (!source.empty() && fileExists(source)) {
				found++;
			}
			else {
				missing += string(kFontSlots[i].target) + " ";
			}
		}
		if (found == kDtgeFontSlotCount) {
			checks[2].level = 2;
			checks[2].text = string(u8"폰트 14개가 모두 있습니다") + (custom > 0 ? string(u8" (직접 고른 것 ") + to_string(custom) + u8"개)" : string());
		}
		else if (o.fontDir.empty() && custom == 0) {
			checks[2].level = 0;
			checks[2].text = u8"한글 폰트 폴더를 고르세요 (000000.ttf ~ 000009.ttf 가 든 폴더). 자리별로 직접 고를 수도 있습니다";
		}
		else {
			checks[2].level = 0;
			checks[2].text = string(u8"쓸 폰트가 없는 자리: ") + missing;
		}
	}

	// 4. 고친 스크립트 폴더 (선택)
	if (o.overrideDir.empty()) {
		checks[4].level = 2;
		checks[4].text = u8"선택 사항입니다. 비워 두면 원본 스크립트를 그대로 씁니다";
	}
	else if (!dirExists(o.overrideDir)) {
		checks[4].level = 0;
		checks[4].text = u8"폴더가 없습니다";
	}
	else if (!dirExists(joinPath(o.overrideDir, L"script")) && !dirExists(joinPath(o.overrideDir, L"image"))) {
		checks[4].level = 0;
		checks[4].text = u8"이 폴더 안에 script 폴더나 image 폴더가 없습니다 (추출한 폴더와 같은 구조: script\\frame\\000002.json, image\\000414.png)";
	}
	else {
		vector<wstring> files;
		collectFiles(joinPath(o.overrideDir, L"script"), wstring(), files);
		size_t scriptCount = files.size();
		size_t imageCount = listFiles(joinPath(o.overrideDir, L"image"), L"*.png").size();
		checks[4].level = (scriptCount + imageCount == 0) ? 1 : 2;
		checks[4].text = (scriptCount + imageCount == 0) ? string(u8"script 와 image 폴더에 파일이 없습니다")
			: string(u8"script 파일 ") + to_string(scriptCount) + u8"개 (원본과 다른 것만 덮어씁니다), image PNG " + to_string(imageCount) + u8"개를 반영합니다 (번역 문구는 스크립트 위에 들어갑니다)";
	}

	// 5. 고친 이미지 폴더 (선택)
	if (o.imageDir.empty()) {
		checks[5].level = 2;
		checks[5].text = u8"선택 사항입니다. 비워 두면 원본 이미지를 그대로 씁니다";
	}
	else if (!dirExists(o.imageDir)) {
		checks[5].level = 0;
		checks[5].text = u8"폴더가 없습니다";
	}
	else {
		size_t count = listFiles(o.imageDir, L"*.png").size();
		checks[5].level = (count == 0) ? 1 : 2;
		checks[5].text = (count == 0) ? string(u8"PNG 파일이 없습니다 (000414.png 처럼 번호가 이름인 PNG 를 이 폴더에 바로 두세요)")
			: string(u8"PNG ") + to_string(count) + u8"개 중 원본과 다른 것을 반영합니다";
	}

	// 3. 출력 위치
	if (o.outputParent.empty()) {
		checks[3].level = 0;
		checks[3].text = u8"결과를 만들 위치를 고르세요";
	}
	else if (!dirExists(o.outputParent)) {
		checks[3].level = 1;
		checks[3].text = u8"폴더가 없어서 새로 만듭니다";
	}
	else {
		checks[3].level = 2;
		checks[3].text = string(u8"이 안에 새 폴더 '") + utf8(baseNameOf(o.originalExe.empty() ? L"dtge" : o.originalExe)) + u8"_ko' 를 만듭니다. 이미 있는 파일과 폴더는 건드리지 않습니다";
	}
}


bool CTT::Dtge::runDtgeBuild(const DtgeOptions& o, DtgeLogFn log, void* user, wstring& resultExe, wstring& resultDir)
{
	Ctx c;
	c.log = log;
	c.user = user;

	vector<DtgeCheck> checks;
	checkDtgeInputs(o, checks);
	for (size_t i = 0; i < checks.size(); i++) {
		if (checks[i].level == 0) {
			c.say(-1, 3, string(u8"입력을 확인하세요: ") + checks[i].text);
			return false;
		}
	}

	// 새 결과 폴더: 이미 있는 폴더는 절대 쓰지 않는다 (CreateDirectory 는 이미 있으면 실패한다)
	if (!dirExists(o.outputParent) && !createDirectoryRecursively(o.outputParent)) {
		c.say(-1, 3, u8"결과를 만들 위치를 만들지 못했습니다");
		return false;
	}
	wstring workDir;
	bool created = false;
	for (int attempt = 0; attempt < 50 && !created; attempt++) {
		workDir = chooseNewFolder(o.outputParent, baseNameOf(o.originalExe) + L"_ko");
		created = CreateDirectoryW(workDir.c_str(), NULL) != 0;
	}
	if (!created) {
		c.say(-1, 3, u8"결과 폴더를 새로 만들지 못했습니다");
		return false;
	}
	c.say(-1, 0, string(u8"결과 폴더: ") + utf8(workDir));

	// 원본은 읽기만 한다: 작업 폴더에 dtge.exe 로 복사해서 그 사본으로 작업한다
	wstring workExe = joinPath(workDir, L"dtge.exe");
	if (!CopyFileW(o.originalExe.c_str(), workExe.c_str(), TRUE)) {
		c.say(-1, 3, u8"원본 exe 를 작업 폴더로 복사하지 못했습니다");
		return false;
	}
	wstring gameDir = joinPath(workDir, L"dtge");
	wstring configPath = joinPath(workDir, L"glyphConfig.json");

	c.say(0, 1, u8"원본 exe 를 읽어 폰트, 이미지, 스크립트, 문구, 모듈을 꺼냅니다 (1분쯤 걸립니다)");
	if (!stepExtract(c, workExe)) {
		return false;
	}
	c.say(0, 2, u8"추출했습니다");

	c.say(1, 1, u8"폰트를 한글 폰트로 바꿉니다");
	if (!stepFonts(c, o, gameDir)) {
		return false;
	}
	c.say(1, 2, u8"폰트를 바꿨습니다");

	c.say(2, 1, u8"번역 파일을 넣습니다");
	if (!o.overrideDir.empty() && !stepOverrides(c, o, gameDir)) {
		return false;
	}
	if (!CopyFileW(joinPath(o.translationDir, L"frameEventText.json").c_str(), joinPath(gameDir, L"text\\frameEventText.json").c_str(), FALSE)
		|| !CopyFileW(joinPath(o.translationDir, L"objectText.json").c_str(), joinPath(gameDir, L"text\\objectText.json").c_str(), FALSE)) {
		c.say(2, 3, u8"번역 파일을 복사하지 못했습니다");
		return false;
	}
	c.say(2, 2, u8"번역을 넣었습니다");

	c.say(3, 1, u8"번역문에 쓰인 글자를 대사 글꼴별로 그립니다");
	vector<wstring> changedImages;
	if (!o.overrideDir.empty() || !o.imageDir.empty()) {
		selectChangedImages(c, o, gameDir, changedImages);
	}
	if (!stepGlyphs(c, workDir, gameDir, configPath)) {
		return false;
	}
	if (!applyChangedImages(c, o, gameDir, changedImages)) {
		return false;
	}
	c.say(3, 2, u8"글자 이미지를 만들었습니다");

	c.say(4, 1, u8"파티클 문구(전투 중 비난 문구)가 한글을 그리게 모듈을 고칩니다");
	if (!stepParticles(c, gameDir, configPath)) {
		return false;
	}
	c.say(4, 2, u8"모듈을 고쳤습니다");

	c.say(5, 1, u8"대사 줄 정렬에 필요한 유니코드 파서 모듈을 넣습니다");
	if (!stepParser(c, gameDir)) {
		return false;
	}
	c.say(5, 2, u8"파서 모듈을 넣었습니다");

	c.say(6, 1, u8"코드페이지 949 / 로케일 1042 로 새 exe 를 만듭니다 (1분쯤 걸립니다)");
	wstring builtExe = joinPath(workDir, L"dtge_new.exe");
	wstring korExe;
	if (!makeKoreanExe(c, workExe, builtExe, joinPath(workDir, baseNameOf(o.originalExe) + L"_kor.exe"), korExe)) {
		return false;
	}
	c.say(6, 0, string(u8"한글판 완성: ") + utf8(korExe));

	// 시험용 HP 무감소판: 같은 추출 폴더의 전투 프레임만 고쳐서 한 번 더 합친다 (한글판은 그대로 둔다)
	if (o.noDamage) {
		c.say(6, 0, u8"HP 무감소판을 이어서 만듭니다 (1분쯤 더 걸립니다)");
		if (!stepNoDamage(c, gameDir)) {
			return false;
		}
		wstring nodamageExe;
		if (!makeKoreanExe(c, workExe, builtExe, joinPath(workDir, baseNameOf(o.originalExe) + L"_kor_nodamage.exe"), nodamageExe)) {
			return false;
		}
		c.say(6, 0, string(u8"HP 무감소판 완성: ") + utf8(nodamageExe));
	}
	c.say(6, 2, u8"완성했습니다");

	resultExe = korExe;
	resultDir = workDir;
	return true;
}


const char* CTT::Dtge::dtgeFontSlotTarget(int slot)
{
	return (slot >= 0 && slot < kDtgeFontSlotCount) ? kFontSlots[slot].target : "";
}


const char* CTT::Dtge::dtgeFontSlotRole(int slot)
{
	return (slot >= 0 && slot < kDtgeFontSlotCount) ? kFontSlots[slot].role : "";
}


const char* CTT::Dtge::dtgeFontSlotDefault(int slot)
{
	return (slot >= 0 && slot < kDtgeFontSlotCount) ? kFontSlots[slot].defaultSource : "";
}
