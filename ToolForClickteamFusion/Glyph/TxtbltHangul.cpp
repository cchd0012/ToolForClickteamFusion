#include "TxtbltHangul.h"
#include "FtFont.h"
#include "TxtbltFoldCode.h"
#include "../Util/CommonUtil.h"
#include "../Util/JsonUtil.h"
#include "../Util/PngUtil.h"

#include <map>
#include <set>

using namespace CTT::Glyph;
using namespace CTT::CommonUtil;
using namespace CTT::JsonUtil;
using namespace CTT::PngUtil;

namespace {

	typedef vector<unsigned char> Bytes;

	const int kSyllableCount = 25 * 94;				// KS X 1001 한글 음절 (앞바이트 B0~C8 x 뒤바이트 A1~FE)

	string utf8(const wstring& s) { return getAnsiStr(s, CodePage::CP_UTF_8); }
	wstring fromUtf8(const string& s) { return getUniStr(s, CodePage::CP_UTF_8); }

	int floorDiv(int a, int b)
	{
		int q = a / b;
		if (((a % b) != 0) && ((a < 0) != (b < 0))) {
			q--;
		}
		return q;
	}


	// 코드페이지 949 에서 두 바이트이고 KS X 1001 한글 음절이면 번호(0~2349), 아니면 -1
	int syllableIndex(wchar_t ch)
	{
		char b[4] = { 0, 0, 0, 0 };
		BOOL usedDefault = FALSE;
		int n = WideCharToMultiByte(949, WC_NO_BEST_FIT_CHARS, &ch, 1, b, 4, NULL, &usedDefault);
		if (n != 2 || usedDefault) {
			return -1;
		}
		unsigned lead = (unsigned char)b[0], trail = (unsigned char)b[1];
		if (lead < 0xB0 || lead > 0xC8 || trail < 0xA1 || trail > 0xFE) {
			return -1;
		}
		return (int)(lead - 0xB0) * 94 + (int)(trail - 0xA1);
	}


	// ---- 이벤트 JSON 에서 오브젝트가 받는 문자열 모으기

	void collectStringTokens(const Json::Value& node, set<string>& out)
	{
		vector<const Json::Value*> stack;
		stack.push_back(&node);
		while (!stack.empty()) {
			const Json::Value* n = stack.back();
			stack.pop_back();
			if (n->isObject()) {
				if ((*n)["kind"].isString() && (*n)["kind"].asString() == "string" && (*n)["value"].isString()) {
					out.insert((*n)["value"].asString());
				}
				for (Json::ValueConstIterator it = n->begin(); it != n->end(); ++it) {
					if (it->isObject() || it->isArray()) {
						stack.push_back(&(*it));
					}
				}
			}
			else if (n->isArray()) {
				for (Json::ArrayIndex i = 0; i < n->size(); i++) {
					stack.push_back(&(*n)[i]);
				}
			}
		}
	}


	// 지정한 오브젝트의 액션에 쓰인 문자열의 번역에서 한글 음절 번호를 모은다 (번호 -> 글자)
	bool collectSyllables(const wstring& gameDir, const string& objectName, const vector<int>& frames, map<int, wchar_t>& syllables, size_t& stringCount)
	{
		// 원문 -> 번역
		map<string, string> translations;
		{
			wstring textPath = gameDir + L"/text/frameEventText.json";
			Json::Value table;
			if (isFileOrDir(textPath) != FSOType::FSO_FILE) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": text/frameEventText.json 이 없습니다." << endl;
				return false;
			}
			if (!readFromJson(table, textPath)) {
				return false;
			}
			const Json::Value& texts = table["texts"];
			for (Json::ArrayIndex i = 0; i < texts.size(); i++) {
				if (texts[i]["original"].isString() && texts[i]["translation"].isString() && !texts[i]["translation"].asString().empty()) {
					translations[texts[i]["original"].asString()] = texts[i]["translation"].asString();
				}
			}
		}

		// 대상 프레임 목록 (지정하지 않으면 script/frame 의 모든 파일)
		vector<wstring> files;
		if (!frames.empty()) {
			for (size_t i = 0; i < frames.size(); i++) {
				wchar_t name[64];
				swprintf(name, 64, L"%06d.json", frames[i]);
				files.push_back(gameDir + L"/script/frame/" + name);
			}
		}
		else {
			WIN32_FIND_DATAW fd;
			HANDLE hFind = FindFirstFileW((gameDir + L"/script/frame/*.json").c_str(), &fd);
			if (hFind != INVALID_HANDLE_VALUE) {
				do {
					files.push_back(gameDir + L"/script/frame/" + fd.cFileName);
				} while (FindNextFileW(hFind, &fd));
				FindClose(hFind);
			}
		}

		set<string> originals;
		for (size_t f = 0; f < files.size(); f++) {
			if (isFileOrDir(files[f]) != FSOType::FSO_FILE) {
				continue;
			}
			Json::Value frame;
			if (!readFromJson(frame, files[f])) {
				return false;
			}
			const Json::Value& blocks = frame["events"]["blocks"];
			for (Json::ArrayIndex b = 0; b < blocks.size(); b++) {
				const Json::Value& groups = blocks[b]["groups"];
				for (Json::ArrayIndex g = 0; g < groups.size(); g++) {
					const Json::Value& actions = groups[g]["actions"];
					for (Json::ArrayIndex a = 0; a < actions.size(); a++) {
						if (actions[a]["objectName"].isString() && actions[a]["objectName"].asString() == objectName) {
							collectStringTokens(actions[a], originals);
						}
					}
				}
			}
		}

		stringCount = 0;
		for (set<string>::iterator it = originals.begin(); it != originals.end(); ++it) {
			map<string, string>::iterator tr = translations.find(*it);
			if (tr == translations.end()) {
				continue;
			}
			stringCount++;
			wstring text = fromUtf8(tr->second);
			for (size_t i = 0; i < text.size(); i++) {
				int idx = syllableIndex(text[i]);
				if (idx >= 0) {
					syllables[idx] = text[i];
				}
			}
		}
		return true;
	}


	// ---- 모듈(PE) 패치

	UINT32 get32(const Bytes& d, size_t o) { UINT32 v; memcpy(&v, &d[o], 4); return v; }
	UINT16 get16(const Bytes& d, size_t o) { UINT16 v; memcpy(&v, &d[o], 2); return v; }
	void put32(Bytes& d, size_t o, UINT32 v) { memcpy(&d[o], &v, 4); }
	void put16(Bytes& d, size_t o, UINT16 v) { memcpy(&d[o], &v, 2); }
	void append32(Bytes& d, UINT32 v) { unsigned char b[4]; memcpy(b, &v, 4); d.insert(d.end(), b, b + 4); }
	void appendRel32(Bytes& d, UINT32 fromEndVa, UINT32 toVa) { append32(d, toVa - fromEndVa); }

	struct PeSection { string name; UINT32 vs, va, rs, ro; };

	const UINT32 kImageBase = 0x10000000;

	// 레이아웃 함수가 글자 표를 채운 직후 / 글자 표 할당 / 폰트 표면 높이, 폭 인자 (txtblt 8424C67E 판)
	const UINT32 kHookVa = 0x10008e49;
	const unsigned char kHookOrg[6] = { 0x8a, 0x8e, 0x2c, 0x03, 0x00, 0x00 };			// mov cl,[esi+0x32c]
	const UINT32 kHookRet = 0x10008e4f;
	const UINT32 kAllocVa = 0x10009337;
	const unsigned char kAllocOrg[9] = { 0x68, 0x00, 0x10, 0x00, 0x00, 0x89, 0x85, 0xec, 0x02 };		// push 0x1000 / mov [ebp+0x2ec],eax
	const UINT32 kSurfHeightVa = 0x100095bb;
	const unsigned char kSurfHeightOrg[6] = { 0x8b, 0x8d, 0xdc, 0x02, 0x00, 0x00 };		// mov ecx,[ebp+0x2dc]
	const UINT32 kSurfHeightRet = 0x100095c1;
	const UINT32 kSurfWidthVa = 0x100095c2;
	const unsigned char kSurfWidthOrg[6] = { 0x8b, 0x95, 0xd8, 0x02, 0x00, 0x00 };		// mov edx,[ebp+0x2d8]


	// 후크 코드: pushad / call $+5 / pop eax / add eax,<표까지> / push eax / push 개체 / call 함수 / add esp,8 / [mov [esp+0x18],eax] / popad / 밀려난 명령 / jmp 되돌아갈 곳
	Bytes buildTrampoline(UINT32 startVa, UINT32 tableVa, UINT32 funcVa, unsigned char pushObjectReg, bool returnsEcx, const Bytes& displaced, UINT32 retVa)
	{
		Bytes t;
		t.push_back(0x60);																// pushad
		t.push_back(0xe8); append32(t, 0);												// call $+5
		UINT32 land = startVa + (UINT32)t.size();										// pop eax 의 주소
		t.push_back(0x58);																// pop eax
		t.push_back(0x05); append32(t, tableVa - land);									// add eax,<표까지>
		t.push_back(0x50);																// push eax (음절 표)
		t.push_back(pushObjectReg);														// push esi/ebp (개체)
		t.push_back(0xe8); appendRel32(t, startVa + (UINT32)t.size() + 4, funcVa);		// call 함수
		t.push_back(0x83); t.push_back(0xc4); t.push_back(0x08);						// add esp,8
		if (returnsEcx) {
			t.push_back(0x89); t.push_back(0x44); t.push_back(0x24); t.push_back(0x18);	// mov [esp+0x18],eax (pushad 로 저장한 ECX 자리)
		}
		t.push_back(0x61);																// popad
		t.insert(t.end(), displaced.begin(), displaced.end());
		t.push_back(0xe9); appendRel32(t, startVa + (UINT32)t.size() + 4, retVa);		// jmp
		return t;
	}


	// data 는 추출해 둔 원본 txtblt.mfx. 성공하면 패치된 내용으로 바뀐다
	bool applyModulePatch(Bytes& data, const Bytes& table, int perRow, int cellWidth, wstring& error)
	{
		if (data.size() < 0x400 || data[0] != 'M' || data[1] != 'Z') {
			error = L"PE 파일이 아닙니다.";
			return false;
		}
		UINT32 e = get32(data, 0x3c);
		if (e + 0x100 > data.size() || get32(data, e) != 0x00004550) {
			error = L"PE 헤더를 읽지 못했습니다.";
			return false;
		}
		UINT16 nsec = get16(data, e + 6);
		UINT16 optSize = get16(data, e + 20);
		size_t opt = e + 24;
		size_t sec0 = opt + optSize;
		vector<PeSection> secs;
		for (UINT16 i = 0; i < nsec; i++) {
			size_t o = sec0 + i * 40;
			PeSection s;
			s.name = string((const char*)&data[o], strnlen((const char*)&data[o], 8));
			s.vs = get32(data, o + 8);
			s.va = get32(data, o + 12);
			s.rs = get32(data, o + 16);
			s.ro = get32(data, o + 20);
			secs.push_back(s);
			if (s.name == ".kfold") {
				error = L"이미 한글 패치가 적용된 모듈입니다.";
				return false;
			}
		}

		// VA -> 파일 위치
		struct Local
		{
			static bool va2off(const vector<PeSection>& secs, UINT32 va, size_t& off)
			{
				UINT32 rva = va - kImageBase;
				for (size_t i = 0; i < secs.size(); i++) {
					UINT32 span = (secs[i].vs > secs[i].rs) ? secs[i].vs : secs[i].rs;
					if (rva >= secs[i].va && rva < secs[i].va + span) {
						off = secs[i].ro + (rva - secs[i].va);
						return true;
					}
				}
				return false;
			}
		};

		struct Site { UINT32 va; const unsigned char* org; size_t len; size_t off; };
		Site sites[4] = {
			{ kHookVa, kHookOrg, 6, 0 }, { kAllocVa, kAllocOrg, 9, 0 }, { kSurfHeightVa, kSurfHeightOrg, 6, 0 }, { kSurfWidthVa, kSurfWidthOrg, 6, 0 }
		};
		for (int i = 0; i < 4; i++) {
			if (!Local::va2off(secs, sites[i].va, sites[i].off) || sites[i].off + sites[i].len > data.size() || memcmp(&data[sites[i].off], sites[i].org, sites[i].len) != 0) {
				error = L"원본 txtblt.mfx(8424C67E 판)가 아닙니다.";
				return false;
			}
		}

		// 새 섹션 .kfold: [후크1][후크2][코드][음절 표]
		const UINT32 secAlign = 0x1000, fileAlign = 0x200;
		UINT32 sizeOfImage = get32(data, opt + 56);
		UINT32 sizeOfHeaders = get32(data, opt + 60);
		size_t hdrNew = sec0 + (size_t)nsec * 40;
		bool headerFree = (hdrNew + 40 <= sizeOfHeaders);
		for (size_t i = 0; headerFree && i < 40; i++) { if (data[hdrNew + i] != 0) { headerFree = false; } }
		if (!headerFree) {
			error = L"섹션 헤더를 더할 자리가 없습니다.";
			return false;
		}
		UINT32 lastEnd = 0;
		for (size_t i = 0; i < secs.size(); i++) { if (secs[i].ro + secs[i].rs > lastEnd) { lastEnd = secs[i].ro + secs[i].rs; } }
		if (lastEnd != data.size()) {
			error = L"파일 끝에 섹션 밖의 데이터(오버레이)가 있습니다.";
			return false;
		}

		const UINT32 codeLen = (UINT32)sizeof(kTxtbltFoldCode);
		const UINT32 t1Len = 1 + 5 + 1 + 5 + 1 + 1 + 5 + 3 + 1 + 6 + 5;
		const UINT32 t2Len = 1 + 5 + 1 + 5 + 1 + 1 + 5 + 3 + 4 + 1 + 5;
		UINT32 codeRel = (t1Len + t2Len + 3) & ~3u;
		UINT32 tableRel = (codeRel + codeLen + 3) & ~3u;
		UINT32 need = tableRel + (UINT32)table.size();
		UINT32 newVa = (sizeOfImage + secAlign - 1) & ~(secAlign - 1);
		UINT32 newRo = ((UINT32)data.size() + fileAlign - 1) & ~(fileAlign - 1);
		UINT32 newRs = (need + fileAlign - 1) & ~(fileAlign - 1);
		UINT32 baseVa = kImageBase + newVa;
		UINT32 tableVa = baseVa + tableRel;

		Bytes t1 = buildTrampoline(baseVa, tableVa, baseVa + codeRel + kTxtbltFoldHangulOffset, 0x56, false, Bytes(kHookOrg, kHookOrg + 6), kHookRet);
		Bytes t2 = buildTrampoline(baseVa + t1Len, tableVa, baseVa + codeRel + kTxtbltAtlasHeightOffset, 0x55, true, Bytes(), kSurfHeightRet);
		if (t1.size() != t1Len || t2.size() != t2Len) {
			error = L"후크 코드 크기가 예상과 다릅니다.";
			return false;
		}

		Bytes section(newRs, 0);
		memcpy(&section[0], &t1[0], t1.size());
		memcpy(&section[t1Len], &t2[0], t2.size());
		memcpy(&section[codeRel], kTxtbltFoldCode, codeLen);
		memcpy(&section[tableRel], &table[0], table.size());
		data.resize(newRo, 0);
		data.insert(data.end(), section.begin(), section.end());

		// 섹션 헤더, 헤더 값
		memcpy(&data[hdrNew], ".kfold", 6);
		put32(data, hdrNew + 8, need);
		put32(data, hdrNew + 12, newVa);
		put32(data, hdrNew + 16, newRs);
		put32(data, hdrNew + 20, newRo);
		put32(data, hdrNew + 36, 0x60000020);
		put16(data, e + 6, (UINT16)(nsec + 1));
		put32(data, opt + 56, newVa + ((need + secAlign - 1) & ~(secAlign - 1)));
		put32(data, opt + 4, get32(data, opt + 4) + newRs);					// SizeOfCode

		// 후크: 원래 6바이트 -> jmp + nop
		for (int i = 0; i < 2; i++) {
			Site& s = (i == 0) ? sites[0] : sites[2];
			UINT32 target = (i == 0) ? baseVa : baseVa + t1Len;
			data[s.off] = 0xe9;
			put32(data, s.off + 1, target - (s.va + 5));
			data[s.off + 5] = 0x90;
		}
		// 폰트 표면 폭: mov edx,<아틀라스 폭>
		data[sites[3].off] = 0xba;
		put32(data, sites[3].off + 1, (UINT32)(perRow * cellWidth));
		data[sites[3].off + 5] = 0x90;
		// 글자 표 할당 크기 0x1000 -> 0x2000
		put32(data, sites[1].off + 1, 0x2000);
		return true;
	}


	bool readWholeFile(const wstring& path, Bytes& out)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (h == INVALID_HANDLE_VALUE) {
			return false;
		}
		DWORD size = GetFileSize(h, NULL), read = 0;
		out.resize(size);
		bool ok = (size == 0) || (ReadFile(h, &out[0], size, &read, NULL) && read == size);
		CloseHandle(h);
		return ok;
	}

	bool writeWholeFile(const wstring& path, const Bytes& data)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (h == INVALID_HANDLE_VALUE) {
			return false;
		}
		DWORD written = 0;
		bool ok = WriteFile(h, &data[0], (DWORD)data.size(), &written, NULL) && written == data.size();
		CloseHandle(h);
		return ok;
	}

}


bool CTT::Glyph::patchTxtbltHangul(const wstring& gameDir, const wstring& configPath)
{
	Json::Value config;
	wstring configCopy = configPath;
	if (!readFromJson(config, configCopy)) {
		return false;
	}
	const Json::Value& c = config["txtbltHangul"];
	if (!c.isObject() || !c["module"].isString() || !c["fontImage"].isString() || !c["objectName"].isString() || !c["font"].isString()
		|| !c["size"].isInt() || !c["cellWidth"].isInt() || !c["cellHeight"].isInt()) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 설정에 txtbltHangul 블록(module, fontImage, objectName, font, size, cellWidth, cellHeight)이 필요합니다." << endl;
		return false;
	}
	const int cellW = c["cellWidth"].asInt(), cellH = c["cellHeight"].asInt(), fontSize = c["size"].asInt();
	if (cellW <= 0 || cellH <= 0) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 셀 크기가 올바르지 않습니다." << endl;
		return false;
	}
	vector<int> frames;
	for (Json::ArrayIndex i = 0; i < c["frames"].size(); i++) {
		frames.push_back(c["frames"][i].asInt());
	}

	// 1) 번역에서 한글 음절 모으기
	map<int, wchar_t> syllables;
	size_t stringCount = 0;
	if (!collectSyllables(gameDir, c["objectName"].asString(), frames, syllables, stringCount)) {
		return false;
	}
	wcout << infoPrefix << fromUtf8(c["objectName"].asString()) << L" 오브젝트의 번역 문자열 " << dec << stringCount << L"개에서 한글 음절 " << syllables.size() << L"종을 모았습니다." << endl;

	// 2) 폰트 이미지 + 아틀라스
	wstring imagePath = gameDir + L"/" + fromUtf8(c["fontImage"].asString());
	BYTE* orgBuffer = NULL;
	UINT32 orgW = 0, orgH = 0, orgBits = 0;
	if (!readFromPng(orgBuffer, imagePath, orgW, orgH, orgBits) || orgBits != 32) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 폰트 이미지(32비트 PNG)를 읽지 못했습니다: " << imagePath << endl;
		if (orgBuffer != NULL) { free(orgBuffer); }
		return false;
	}
	const int perRow = c.isMember("perRow") ? c["perRow"].asInt() : (int)orgW / cellW;
	if (perRow <= 0 || (UINT32)(perRow * cellW) < orgW) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 한 줄 칸 수가 원본 폰트 이미지보다 좁습니다." << endl;
		free(orgBuffer);
		return false;
	}

	FtLibrary freeType;					// font 보다 먼저 선언해서 나중에 해제한다
	FtFont font;
	if (!freeType.isOpen() || !font.open(freeType.get(), FtFont::readFile(gameDir + L"/" + fromUtf8(c["font"].asString())), fontSize)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 한글 폰트를 열지 못했습니다." << endl;
		free(orgBuffer);
		return false;
	}

	const int cells = (int)syllables.size() * 2;
	const int rows = (cells + perRow - 1) / perRow;
	const UINT32 atlasW = (UINT32)(perRow * cellW), atlasH = orgH + (UINT32)(rows * cellH);
	Bytes atlas((size_t)atlasW * atlasH * 4, 0);
	for (UINT32 y = 0; y < orgH; y++) {
		memcpy(&atlas[(size_t)y * atlasW * 4], orgBuffer + (size_t)y * orgW * 4, (size_t)orgW * 4);
	}
	free(orgBuffer);

	// 음절 표: 번호 -> 압축 번호
	Bytes table;
	table.resize(4 + kSyllableCount * 2);
	put16(table, 0, (UINT16)perRow);
	put16(table, 2, (UINT16)syllables.size());
	for (int s = 0; s < kSyllableCount; s++) { put16(table, 4 + s * 2, 0xFFFF); }

	const int dy = floorDiv(cellH - (font.ascent() + font.descent()), 2);
	int compact = 0;
	wstring missing;
	for (map<int, wchar_t>::iterator it = syllables.begin(); it != syllables.end(); ++it, compact++) {
		put16(table, 4 + it->first * 2, (UINT16)compact);
		// 두 칸 폭(2*셀폭) 캔버스에 그린 뒤 반으로 나눠 넣는다 (문턱값 128, 흰색/투명)
		vector<unsigned char> canvas((size_t)cellW * 2 * cellH, 0);
		GlyphBitmap bm;
		if (!font.render(it->second, bm)) {
			missing.push_back(it->second);
			continue;
		}
		if (bm.buffer != NULL) {
			for (int gy = 0; gy < bm.rows; gy++) {
				int py = dy + font.ascent() - bm.top + gy;
				if (py < 0 || py >= cellH) { continue; }
				for (int gx = 0; gx < bm.width; gx++) {
					int px = bm.left + gx;
					if (px < 0 || px >= cellW * 2) { continue; }
					canvas[(size_t)py * cellW * 2 + px] = bm.buffer[gy * bm.pitch + gx];
				}
			}
		}
		for (int half = 0; half < 2; half++) {
			int k = compact * 2 + half;
			UINT32 ox = (UINT32)((k % perRow) * cellW), oy = orgH + (UINT32)((k / perRow) * cellH);
			for (int y = 0; y < cellH; y++) {
				for (int x = 0; x < cellW; x++) {
					if (canvas[(size_t)y * cellW * 2 + half * cellW + x] >= 128) {
						unsigned char* p = &atlas[((size_t)(oy + y) * atlasW + ox + x) * 4];
						p[0] = p[1] = p[2] = p[3] = 255;
					}
				}
			}
		}
	}
	if (!missing.empty()) {
		wcout << L"[*] warning: 폰트에 없는 음절 " << dec << missing.size() << L"종: " << missing << endl;
	}

	{
		BYTE* buffer = &atlas[0];
		UINT32 w = atlasW, h = atlasH, bits = 32;
		if (!writeToPng(buffer, imagePath, w, h, bits)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 아틀라스 PNG 를 쓰지 못했습니다." << endl;
			return false;
		}
		wcout << infoPrefix << L"폰트 이미지를 " << dec << orgW << L"x" << orgH << L" -> " << atlasW << L"x" << atlasH << L" 로 늘렸습니다 (음절 " << syllables.size() << L"종)." << endl;
	}

	// 3) 모듈 패치
	wstring modulePath = gameDir + L"/" + fromUtf8(c["module"].asString());
	Bytes module;
	if (!readWholeFile(modulePath, module)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 모듈을 읽지 못했습니다: " << modulePath << endl;
		return false;
	}
	wstring error;
	if (!applyModulePatch(module, table, perRow, cellW, error)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << error << endl;
		return false;
	}
	if (!writeWholeFile(modulePath, module)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 모듈을 쓰지 못했습니다: " << modulePath << endl;
		return false;
	}
	wcout << infoPrefix << L"txtblt 모듈에 한글 접기 코드를 심었습니다: " << modulePath << endl;
	return true;
}


int CTT::Glyph::runTxtbltHangulCommand(int argc, WCHAR** argv)
{
	if (argc < 4) {
		wcout << L"usage: --txtblt-limited-hangul <extracted folder> <glyph config json>" << endl;
		return 2;
	}
	return patchTxtbltHangul(argv[2], argv[3]) ? 0 : 1;
}
