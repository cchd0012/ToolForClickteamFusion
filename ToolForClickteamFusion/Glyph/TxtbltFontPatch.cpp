#include "TxtbltFontPatch.h"
#include "TxtbltFontCode.h"
#include "FtFont.h"
#include "../Util/CommonUtil.h"
#include "../Util/JsonUtil.h"

#include <map>
#include <set>

using namespace CTT::Glyph;
using namespace CTT::CommonUtil;
using namespace CTT::JsonUtil;

namespace {

	typedef vector<unsigned char> Bytes;

	string utf8(const wstring& s) { return getAnsiStr(s, CodePage::CP_UTF_8); }
	wstring fromUtf8(const string& s) { return getUniStr(s, CodePage::CP_UTF_8); }

	UINT32 get32(const Bytes& d, size_t o) { UINT32 v; memcpy(&v, &d[o], 4); return v; }
	UINT16 get16(const Bytes& d, size_t o) { UINT16 v; memcpy(&v, &d[o], 2); return v; }
	void put32(Bytes& d, size_t o, UINT32 v) { memcpy(&d[o], &v, 4); }
	void put16(Bytes& d, size_t o, UINT16 v) { memcpy(&d[o], &v, 2); }
	void append32(Bytes& d, UINT32 v) { unsigned char b[4]; memcpy(b, &v, 4); d.insert(d.end(), b, b + 4); }

	UINT32 align4(UINT32 v) { return (v + 3) & ~3u; }

	const UINT32 kImageBase = 0x10000000;
	const size_t kOriginalSize = 37376;

	// 원본 txtblt.mfx(37376바이트 판)의 패치 지점
	//  1) GetRunObjectDataSize: mov ax,0x418 -> 0x420 (런타임 개체에 글자표 포인터 자리 8바이트를 더한다)
	const UINT32 kSizeVa = 0x10002f16;
	const unsigned char kSizeOrg[7] = { 0x66, 0xb8, 0x18, 0x04, 0xc2, 0x08, 0x00 };
	//  2) CreateRunObject 시작 부분: 글자표를 찾아 개체에 연결한다 (ebx = 개체, esi = 편집 데이터)
	const UINT32 kBindVa = 0x10002f2b;
	const unsigned char kBindOrg[8] = { 0x8b, 0x43, 0x08, 0x57, 0x0f, 0xbf, 0x4e, 0x18 };	// mov eax,[ebx+8] / push edi / movsx ecx,word [esi+0x18]
	const UINT32 kBindRet = 0x10002f33;
	//  3) DisplayRunObject 의 일반 글자 하나 그리기 (68바이트). 끝(0x10004463)은 글자 포인터/남은 수를 갱신하는 공통 꼬리
	const UINT32 kDrawVa = 0x1000441f;
	const unsigned char kDrawOrg[68] = {
		0x8b, 0x45, 0xe0, 0x6a, 0x00, 0xff, 0x75, 0xc0, 0x8a, 0x00, 0x2a, 0x45, 0x90, 0xff, 0x75, 0xc4, 0x0f, 0xb6, 0xc0, 0xff, 0x75, 0xbc,
		0xc1, 0xe0, 0x04, 0x03, 0x45, 0xdc, 0xff, 0x75, 0xe4, 0x53, 0x8b, 0x48, 0x04, 0x8b, 0x00, 0x03, 0x4d, 0x98, 0x03, 0x45, 0xa4, 0x51,
		0x50, 0xff, 0x75, 0xfc, 0x8b, 0x4d, 0xe8, 0xff, 0x75, 0xec, 0xff, 0x75, 0xd4, 0xff, 0xd7, 0x8b, 0x45, 0xb8, 0x01, 0x45, 0xec, 0xff,
		0x45, 0xf0 };
	const UINT32 kDrawRet = 0x10004463;

	struct PeSection { string name; UINT32 vs, va, rs, ro; };


	// 아주 작은 어셈블러: 바이트 나열 + 라벨 + rel32 분기/호출
	class Asm
	{
	public:
		Asm(UINT32 baseVa) : baseVa(baseVa) {}

		Bytes bytes;

		void raw(const unsigned char* p, size_t n) { bytes.insert(bytes.end(), p, p + n); }
		void op(std::initializer_list<unsigned char> list) { bytes.insert(bytes.end(), list.begin(), list.end()); }
		void label(const string& name) { labels[name] = bytes.size(); }
		// rel32 분기: 앞에 붙는 연산 코드(들) 뒤에 4바이트 상대 거리를 둔다
		void branch(std::initializer_list<unsigned char> opcode, const string& target)
		{
			op(opcode);
			fixups.push_back(make_pair(bytes.size(), target));
			append32(bytes, 0);
		}
		// 절대 주소로 가는 jmp/call
		void jumpVa(unsigned char opcode, UINT32 targetVa)
		{
			bytes.push_back(opcode);
			append32(bytes, targetVa - (baseVa + (UINT32)bytes.size() + 4));
		}
		void resolve()
		{
			for (size_t i = 0; i < fixups.size(); i++) {
				UINT32 rel = (UINT32)labels[fixups[i].second] - (UINT32)(fixups[i].first + 4);
				put32(bytes, fixups[i].first, rel);
			}
		}

	private:
		UINT32 baseVa;
		map<string, size_t> labels;
		vector<pair<size_t, string> > fixups;
	};


	// CreateRunObject 후크: kfont_bind(개체, 편집 데이터, 등록 표) 를 부른 뒤 밀려난 명령을 실행하고 되돌아간다
	Bytes buildBindTrampoline(UINT32 startVa, UINT32 bindVa, UINT32 registryVa)
	{
		Asm a(startVa);
		a.op({ 0x52 });																	// push edx
		a.op({ 0xe8, 0, 0, 0, 0 });														// call $+5
		UINT32 land = startVa + (UINT32)a.bytes.size();									// pop eax 의 주소
		a.op({ 0x58 });																	// pop eax
		a.op({ 0x05 }); append32(a.bytes, registryVa - land);							// add eax,<등록 표까지>
		a.op({ 0x50, 0x56, 0x53 });														// push eax / push esi / push ebx
		a.jumpVa(0xe8, bindVa);															// call kfont_bind (stdcall: 인자 정리는 함수가 한다)
		a.op({ 0x5a });																	// pop edx
		a.raw(kBindOrg, sizeof(kBindOrg) - 4);											// mov eax,[ebx+8] / push edi (밀려난 명령)
		a.raw(kBindOrg + 4, 4);															// movsx ecx,word [esi+0x18]
		a.jumpVa(0xe9, kBindRet);														// jmp 되돌아갈 곳
		return a.bytes;
	}


	// 일반 글자 그리기: 글자표가 있으면 1~2바이트 글자를 글자표대로 그리고, 없으면 원래 코드를 그대로 실행한다
	//   스택 구성(블릿 인자 11개를 쌓는 동안 글자 정보 8바이트는 esp 쪽에 있다): 오프셋은 쌓은 개수에 따라 달라진다
	Bytes buildDrawTrampoline(UINT32 startVa, UINT32 getVa)
	{
		Asm a(startVa);
		a.op({ 0x8b, 0x86, 0x18, 0x04, 0x00, 0x00 });									// mov eax,[esi+0x418]   글자표
		a.op({ 0x85, 0xc0 });															// test eax,eax
		a.branch({ 0x0f, 0x84 }, "orig");												// jz 원래 코드
		a.op({ 0x8b, 0x4d, 0xe0 });														// mov ecx,[ebp-0x20]    글자 포인터
		a.op({ 0x0f, 0xb6, 0x11 });														// movzx edx,byte [ecx]
		a.op({ 0x81, 0xfa, 0x80, 0x00, 0x00, 0x00 });									// cmp edx,0x80
		a.branch({ 0x0f, 0x82 }, "have");												// jb 한 바이트 글자
		a.op({ 0x83, 0x7d, 0xd8, 0x01 });												// cmp dword [ebp-0x28],1   남은 바이트
		a.branch({ 0x0f, 0x8e }, "have");												// jle 마지막 한 바이트는 짝이 없다
		a.op({ 0xff, 0x45, 0xf0 });														// inc dword [ebp-0x10]  글자 번호 (한 쌍은 2로 센다)
		a.op({ 0x41 });																	// inc ecx
		a.op({ 0xff, 0x4d, 0xd8 });														// dec dword [ebp-0x28]
		a.op({ 0x89, 0x4d, 0xe0 });														// mov [ebp-0x20],ecx
		a.op({ 0xc1, 0xe2, 0x08 });														// shl edx,8
		a.op({ 0x0f, 0xb6, 0x09 });														// movzx ecx,byte [ecx]
		a.op({ 0x0b, 0xd1 });															// or edx,ecx            CP949 코드
		a.label("have");
		a.op({ 0x83, 0xec, 0x08 });														// sub esp,8             글자 정보 자리
		a.op({ 0x8b, 0xcc });															// mov ecx,esp
		a.op({ 0x51, 0x52, 0x50 });														// push ecx / push edx / push eax
		a.jumpVa(0xe8, getVa);															// call kfont_get(글자표, 코드, 출력)
		a.op({ 0x0f, 0xb6, 0x44, 0x24, 0x04 });										// movzx eax,byte [esp+4]  폭
		a.op({ 0x85, 0xc0 });															// test eax,eax
		a.branch({ 0x0f, 0x84 }, "after");												// jz 그릴 것이 없다
		a.op({ 0x6a, 0x00 });															// push 0
		a.op({ 0xff, 0x75, 0xc0 });														// push [ebp-0x40]
		a.op({ 0xff, 0x75, 0xc4 });														// push [ebp-0x3c]
		a.op({ 0xff, 0x75, 0xbc });														// push [ebp-0x44]
		a.op({ 0x0f, 0xb6, 0x44, 0x24, 0x15 });										// movzx eax,byte [esp+0x15]  높이
		a.op({ 0x50 });
		a.op({ 0x0f, 0xb6, 0x44, 0x24, 0x18 });										// movzx eax,byte [esp+0x18]  폭
		a.op({ 0x50 });
		a.op({ 0x0f, 0xb7, 0x44, 0x24, 0x1a });										// movzx eax,word [esp+0x1a]  원본 y
		a.op({ 0x03, 0x45, 0x98 });														// add eax,[ebp-0x68]
		a.op({ 0x50 });
		a.op({ 0x0f, 0xb7, 0x44, 0x24, 0x1c });										// movzx eax,word [esp+0x1c]  원본 x
		a.op({ 0x03, 0x45, 0xa4 });														// add eax,[ebp-0x5c]
		a.op({ 0x50 });
		a.op({ 0xff, 0x75, 0xfc });														// push [ebp-4]               그릴 y
		a.op({ 0x0f, 0xbe, 0x44, 0x24, 0x2a });										// movsx eax,byte [esp+0x2a]  가로 보정
		a.op({ 0x03, 0x45, 0xec });														// add eax,[ebp-0x14]         그릴 x
		a.op({ 0x50 });
		a.op({ 0xff, 0x75, 0xd4 });														// push [ebp-0x2c]            그릴 표면
		a.op({ 0x8b, 0x4d, 0xe8 });														// mov ecx,[ebp-0x18]         폰트 표면
		a.op({ 0xff, 0xd7 });															// call edi                   블릿
		a.label("after");
		a.op({ 0x0f, 0xb6, 0x44, 0x24, 0x07 });										// movzx eax,byte [esp+7]     advance
		a.op({ 0x83, 0xc4, 0x08 });														// add esp,8
		a.op({ 0x01, 0x45, 0xec });														// add [ebp-0x14],eax
		a.op({ 0xff, 0x45, 0xf0 });														// inc dword [ebp-0x10]
		a.jumpVa(0xe9, kDrawRet);														// jmp 공통 꼬리
		a.label("orig");
		a.raw(kDrawOrg, sizeof(kDrawOrg));												// 글자표가 없는 오브젝트: 원래 코드
		a.jumpVa(0xe9, kDrawRet);
		a.resolve();
		return a.bytes;
	}

}


bool CTT::Glyph::patchTxtbltFont(vector<unsigned char>& data, const vector<TxtbltFontAssignment>& assignments, wstring& error)
{
	if (assignments.empty()) {
		error = L"글자표 지정이 없습니다.";
		return false;
	}
	if (data.size() != kOriginalSize || data[0] != 'M' || data[1] != 'Z') {
		error = L"원본 txtblt.mfx(37376바이트 판)가 아닙니다.";
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
		if (s.name == ".kfont") {
			error = L"이미 글자표 패치가 적용된 모듈입니다.";
			return false;
		}
	}

	// VA -> 파일 위치, 패치 지점의 원래 바이트 확인
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
	Site sites[3] = { { kSizeVa, kSizeOrg, sizeof(kSizeOrg), 0 }, { kBindVa, kBindOrg, sizeof(kBindOrg), 0 }, { kDrawVa, kDrawOrg, sizeof(kDrawOrg), 0 } };
	for (int i = 0; i < 3; i++) {
		if (!Local::va2off(secs, sites[i].va, sites[i].off) || sites[i].off + sites[i].len > data.size() || memcmp(&data[sites[i].off], sites[i].org, sites[i].len) != 0) {
			error = L"원본 txtblt.mfx(37376바이트 판)의 패치 지점이 맞지 않습니다.";
			return false;
		}
	}

	// 섹션 헤더를 더할 자리와 파일 끝 확인
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

	// 등록 표: 글자표 번호마다 글자표 위치를 가리킨다
	vector<UINT32> tableOffset(assignments.size(), 0);
	vector<pair<UINT32, size_t> > entries;				// 글자표 번호 -> 지정 번호
	set<UINT32> seenIds;
	for (size_t i = 0; i < assignments.size(); i++) {
		if (assignments[i].table.size() < 32 + 256 * 4 || get32(assignments[i].table, 0) != 0x3154464B) {
			error = L"KFT1 글자표가 아닙니다.";
			return false;
		}
		if (assignments[i].id == 0 || assignments[i].id > 0x7FFFFFFF) {
			error = L"글자표 번호는 1 이상이어야 합니다.";
			return false;
		}
		if (!seenIds.insert(assignments[i].id).second) {
			error = L"같은 글자표 번호가 두 번 지정되었습니다: " + to_wstring((int)assignments[i].id);
			return false;
		}
		entries.push_back(make_pair(assignments[i].id, i));
	}

	// 새 섹션 .kfont: [CreateRunObject 후크][글자 그리기][헬퍼 코드][등록 표][글자표들]
	const UINT32 newVa = (sizeOfImage + secAlign - 1) & ~(secAlign - 1);
	const UINT32 baseVa = kImageBase + newVa;
	const UINT32 codeLen = (UINT32)sizeof(kTxtbltFontCode);
	UINT32 bindTrampolineLen = (UINT32)buildBindTrampoline(0, 0, 0).size();
	UINT32 drawTrampolineLen = (UINT32)buildDrawTrampoline(0, 0).size();
	UINT32 bindRel = 0;
	UINT32 drawRel = align4(bindRel + bindTrampolineLen);
	UINT32 codeRel = align4(drawRel + drawTrampolineLen);
	UINT32 registryRel = align4(codeRel + codeLen);
	UINT32 registryLen = 8 + (UINT32)entries.size() * 8;
	UINT32 tablesRel = align4(registryRel + registryLen);
	UINT32 cursor = tablesRel;
	for (size_t i = 0; i < assignments.size(); i++) {
		tableOffset[i] = cursor - registryRel;
		cursor = align4(cursor + (UINT32)assignments[i].table.size());
	}
	const UINT32 need = cursor;
	const UINT32 newRo = ((UINT32)data.size() + fileAlign - 1) & ~(fileAlign - 1);
	const UINT32 newRs = (need + fileAlign - 1) & ~(fileAlign - 1);

	Bytes bindCode = buildBindTrampoline(baseVa + bindRel, baseVa + codeRel + kTxtbltFontBindOffset, baseVa + registryRel);
	Bytes drawCode = buildDrawTrampoline(baseVa + drawRel, baseVa + codeRel + kTxtbltFontGetOffset);
	if (bindCode.size() != bindTrampolineLen || drawCode.size() != drawTrampolineLen) {
		error = L"후크 코드 크기가 예상과 다릅니다.";
		return false;
	}

	Bytes section(newRs, 0xcc);
	memcpy(&section[bindRel], &bindCode[0], bindCode.size());
	memcpy(&section[drawRel], &drawCode[0], drawCode.size());
	memcpy(&section[codeRel], kTxtbltFontCode, codeLen);
	memset(&section[registryRel], 0, need - registryRel);
	put32(section, registryRel, 0x3152464B);						// 'KFR1'
	put32(section, registryRel + 4, (UINT32)entries.size());
	for (size_t i = 0; i < entries.size(); i++) {
		put32(section, registryRel + 8 + i * 8, entries[i].first);
		put32(section, registryRel + 8 + i * 8 + 4, tableOffset[entries[i].second]);
	}
	for (size_t i = 0; i < assignments.size(); i++) {
		memcpy(&section[registryRel + tableOffset[i]], &assignments[i].table[0], assignments[i].table.size());
	}
	data.resize(newRo, 0);
	data.insert(data.end(), section.begin(), section.end());

	// 섹션 헤더, 헤더 값
	memcpy(&data[hdrNew], ".kfont", 6);
	put32(data, hdrNew + 8, need);
	put32(data, hdrNew + 12, newVa);
	put32(data, hdrNew + 16, newRs);
	put32(data, hdrNew + 20, newRo);
	put32(data, hdrNew + 36, 0x60000020);
	put16(data, e + 6, (UINT16)(nsec + 1));
	put32(data, opt + 56, newVa + ((need + secAlign - 1) & ~(secAlign - 1)));
	put32(data, opt + 4, get32(data, opt + 4) + newRs);				// SizeOfCode

	// 패치 지점 적용
	data[sites[0].off + 2] = 0x20;									// GetRunObjectDataSize: 0x418 -> 0x420
	data[sites[1].off] = 0xe9;										// CreateRunObject: 8바이트 -> jmp + nop
	put32(data, sites[1].off + 1, (baseVa + bindRel) - (sites[1].va + 5));
	data[sites[1].off + 5] = data[sites[1].off + 6] = data[sites[1].off + 7] = 0x90;
	memset(&data[sites[2].off], 0xcc, sizeof(kDrawOrg));			// 일반 글자 그리기: 처음에 jmp, 나머지는 쓰이지 않는다
	data[sites[2].off] = 0xe9;
	put32(data, sites[2].off + 1, (baseVa + drawRel) - (sites[2].va + 5));
	return true;
}


int CTT::Glyph::runTxtbltFontCommand(int argc, WCHAR** argv)
{
	// argv[0] = exe, argv[1] = --txtblt-font, argv[2] = patch, argv[3] = 입력, argv[4] = 출력, argv[5] = 설정
	if (argc < 6 || wcscmp(argv[2], L"patch") != 0) {
		wcout << L"사용법: --txtblt-font patch <입력 txtblt.mfx> <출력 txtblt.mfx> <설정 json>" << endl;
		return 1;
	}

	Json::Value root;
	wstring configFile = argv[5];
	if (!readFromJson(root, configFile)) {
		return 1;
	}
	const wstring baseDir = getOnlyDirPath(configFile);
	const Json::Value& list = root["assignments"];
	if (!list.isArray() || list.empty()) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 설정에 assignments 배열이 필요합니다." << endl;
		return 1;
	}

	vector<TxtbltFontAssignment> assignments;
	for (Json::ArrayIndex i = 0; i < list.size(); i++) {
		const Json::Value& item = list[i];
		if (!item["atlasDir"].isString() || !item["atlas"].isString() || !item["id"].isInt() || item["id"].asInt() <= 0) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << dec << i << L" 번째 지정에 id(1 이상), atlasDir, atlas 가 필요합니다." << endl;
			return 1;
		}
		wstring dir = fromUtf8(item["atlasDir"].asString());
		if (!(dir.size() >= 2 && (dir[1] == L':' || dir[0] == L'/' || dir[0] == L'\\'))) {
			dir = baseDir + L"/" + dir;
		}
		shared_ptr<vector<unsigned char> > table = FtFont::readFile(dir + L"/" + fromUtf8(item["atlas"].asString()) + L".kft");
		if (!table) {
			return 1;
		}
		TxtbltFontAssignment a;
		a.id = (UINT32)item["id"].asInt();
		a.table = *table;
		assignments.push_back(a);
	}

	shared_ptr<vector<unsigned char> > module = FtFont::readFile(argv[3]);
	if (!module) {
		return 1;
	}
	Bytes data(*module);
	wstring error;
	if (!patchTxtbltFont(data, assignments, error)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << error << endl;
		return 1;
	}

	HANDLE file = CreateFileW(argv[4], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 출력 파일을 만들지 못했습니다: " << argv[4] << endl;
		return 1;
	}
	DWORD written = 0;
	bool ok = (WriteFile(file, &data[0], (DWORD)data.size(), &written, NULL) != FALSE) && (written == data.size());
	CloseHandle(file);
	if (!ok) {
		return 1;
	}
	wcout << infoPrefix << L"글자표 " << dec << assignments.size() << L"종을 심었습니다: " << argv[4] << endl;
	return 0;
}
