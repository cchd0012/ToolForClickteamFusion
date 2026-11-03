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

	//  4) 레이아웃 함수 FUN_10002a73(개체) 의 시작: 글자표와 폭 합산 플래그가 있는 오브젝트는 kfont_layout 으로 줄을 나눈다
	const UINT32 kLayoutVa = 0x10002a73;
	const unsigned char kLayoutOrg[9] = { 0x55, 0x8b, 0xec, 0x81, 0xec, 0x34, 0x01, 0x00, 0x00 };		// push ebp / mov ebp,esp / sub esp,0x134
	const UINT32 kLayoutRet = 0x10002a7c;
	const UINT32 kLayoutNewNodeVa = 0x10002a3b;		// 줄 노드를 만드는 함수 (cdecl)
	const UINT32 kLayoutDeleteVa = 0x10004706;		// operator delete 썽크 (cdecl)
	const UINT32 kLayoutEmptyVa = 0x10006c84;		// 머리 노드가 가리키는 빈 문자열

	//  5) 동작 93 "파일에서 폰트 이미지 불러오기" 핸들러(stdcall, 인자 3개)의 시작: 글자표가 있는 오브젝트는 이미지를 바꾸지 않는다
	//     (게임이 실행 중에 원래 폰트 PNG 를 다시 불러와도 글자표가 가리키는 글자판이 그대로 남는다)
	const UINT32 kLoadImageVa = 0x100015b5;
	const unsigned char kLoadImageOrg[7] = { 0x55, 0x8b, 0xec, 0x56, 0x8b, 0x75, 0x08 };		// push ebp / mov ebp,esp / push esi / mov esi,[ebp+8]
	const UINT32 kLoadImageRet = 0x100015bc;

	//  6) DisplayRunObject 시작 부분의 "글자별 이벤트" 플래그(0x800) 계산: 글자표가 있는 오브젝트는 이 플래그를 0 으로 본다
	//     (글자별 이벤트 경로는 글자를 칸 표로 그려서 한글을 그릴 수 없다. 대신 일반 글자 경로로 그리며 글자별 이벤트는 오지 않는다)
	const UINT32 kCallbackVa = 0x1000364a;
	const unsigned char kCallbackOrg[6] = { 0x81, 0xe1, 0x00, 0x08, 0x00, 0x00 };			// and ecx,0x800
	const UINT32 kCallbackRet = 0x10003650;
	//  7) 일반 글자 경로의 탭 처리 시작: 글자표가 있는 오브젝트는 칸 표의 공백 대신 공백 칸 수만큼 가로 위치만 옮긴다
	const UINT32 kTabVa = 0x100043b2;
	const unsigned char kTabOrg[9] = { 0x8b, 0x45, 0xf0, 0x8b, 0x8e, 0xbc, 0x02, 0x00, 0x00 };	// mov eax,[ebp-0x10] / mov ecx,[esi+0x2bc]
	const UINT32 kTabRet = 0x100043bb;
	const UINT32 kTabDone = 0x10004463;															// 글자 하나를 처리한 뒤의 공통 꼬리

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


	// 레이아웃 후크: 글자표가 있고(rd+0x418) 폭 합산 플래그(rd+0x41c bit0)가 켜져 있고 상자 플래그(0x20000)가 없으면 kfont_layout,
	// 아니면 원래 함수를 그대로 실행한다. 노드 함수 / 해제 함수 / 빈 문자열의 주소는 위치 독립으로 구한다
	Bytes buildLayoutTrampoline(UINT32 startVa, UINT32 layoutFuncVa)
	{
		Asm a(startVa);
		a.op({ 0x8b, 0x44, 0x24, 0x04 });												// mov eax,[esp+4]            개체
		a.op({ 0x83, 0xb8, 0x18, 0x04, 0x00, 0x00, 0x00 });								// cmp dword [eax+0x418],0
		a.branch({ 0x0f, 0x84 }, "orig");												// je 원래 함수
		a.op({ 0xf6, 0x80, 0x1c, 0x04, 0x00, 0x00, 0x01 });								// test byte [eax+0x41c],1
		a.branch({ 0x0f, 0x84 }, "orig");												// jz 원래 함수
		a.op({ 0xf7, 0x80, 0xe8, 0x02, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00 });			// test dword [eax+0x2e8],0x20000
		a.branch({ 0x0f, 0x85 }, "orig");												// jnz 원래 함수
		a.op({ 0xe8, 0, 0, 0, 0 });														// call $+5
		const UINT32 land = startVa + (UINT32)a.bytes.size();							// pop ecx 의 주소
		a.op({ 0x59 });																	// pop ecx
		a.op({ 0x8d, 0x91 }); append32(a.bytes, kLayoutEmptyVa - land);					// lea edx,[ecx+<빈 문자열까지>]
		a.op({ 0x52 });																	// push edx
		a.op({ 0x8d, 0x91 }); append32(a.bytes, kLayoutDeleteVa - land);				// lea edx,[ecx+<해제 함수까지>]
		a.op({ 0x52 });
		a.op({ 0x8d, 0x91 }); append32(a.bytes, kLayoutNewNodeVa - land);				// lea edx,[ecx+<노드 함수까지>]
		a.op({ 0x52 });
		a.op({ 0x50 });																	// push eax (개체)
		a.jumpVa(0xe8, layoutFuncVa);													// call kfont_layout (cdecl)
		a.op({ 0x83, 0xc4, 0x10 });														// add esp,16
		a.op({ 0xc3 });																	// ret
		a.label("orig");
		a.raw(kLayoutOrg, sizeof(kLayoutOrg));											// 밀려난 명령 9바이트
		a.jumpVa(0xe9, kLayoutRet);
		a.resolve();
		return a.bytes;
	}

	// 폰트 이미지 불러오기 후크: 글자표가 있으면(rd+0x418) 아무것도 하지 않고 돌아가고, 없으면 원래 함수를 그대로 실행한다
	Bytes buildLoadImageTrampoline(UINT32 startVa)
	{
		Asm a(startVa);
		a.op({ 0x8b, 0x44, 0x24, 0x04 });												// mov eax,[esp+4]            개체
		a.op({ 0x83, 0xb8, 0x18, 0x04, 0x00, 0x00, 0x00 });								// cmp dword [eax+0x418],0
		a.branch({ 0x0f, 0x84 }, "orig");												// je 원래 함수
		a.op({ 0x66, 0x33, 0xc0 });														// xor ax,ax                  (원래 함수의 반환값과 같다)
		a.op({ 0xc2, 0x0c, 0x00 });														// ret 0xc
		a.label("orig");
		a.raw(kLoadImageOrg, sizeof(kLoadImageOrg));									// 밀려난 명령 7바이트
		a.jumpVa(0xe9, kLoadImageRet);
		a.resolve();
		return a.bytes;
	}


	// 글자별 이벤트 플래그 후크: 원래 계산(and ecx,0x800)을 하고, 글자표가 있으면 결과를 0 으로 만든다 (esi = 개체)
	Bytes buildCallbackTrampoline(UINT32 startVa)
	{
		Asm a(startVa);
		a.raw(kCallbackOrg, sizeof(kCallbackOrg));										// and ecx,0x800
		a.op({ 0x83, 0xbe, 0x18, 0x04, 0x00, 0x00, 0x00 });							// cmp dword [esi+0x418],0
		a.branch({ 0x0f, 0x84 }, "done");												// je 그대로
		a.op({ 0x33, 0xc9 });															// xor ecx,ecx
		a.label("done");
		a.jumpVa(0xe9, kCallbackRet);
		a.resolve();
		return a.bytes;
	}


	// 탭 후크: 글자표가 있으면 원본처럼 공백 칸 수(탭 칸 - 글자 번호 % 탭 칸)를 구해 가로 위치와 글자 번호만 옮긴다 (공백은 비어 있다)
	Bytes buildTabTrampoline(UINT32 startVa)
	{
		Asm a(startVa);
		a.op({ 0x83, 0xbe, 0x18, 0x04, 0x00, 0x00, 0x00 });							// cmp dword [esi+0x418],0
		a.branch({ 0x0f, 0x84 }, "orig");												// je 원래 코드
		a.op({ 0x8b, 0x45, 0xf0 });														// mov eax,[ebp-0x10]         글자 번호
		a.op({ 0x8b, 0x8e, 0xbc, 0x02, 0x00, 0x00 });									// mov ecx,[esi+0x2bc]        탭 칸
		a.op({ 0x99 });																	// cdq
		a.op({ 0xf7, 0xf9 });															// idiv ecx
		a.op({ 0x2b, 0xca });															// sub ecx,edx                공백 칸 수
		a.op({ 0x85, 0xc9 });															// test ecx,ecx
		a.branch({ 0x0f, 0x8e }, "tail");												// jle 옮길 것 없음
		a.op({ 0x8b, 0x45, 0xb8 });														// mov eax,[ebp-0x48]         칸 하나의 가로 이동
		a.op({ 0x0f, 0xaf, 0xc1 });														// imul eax,ecx
		a.op({ 0x01, 0x45, 0xec });														// add [ebp-0x14],eax         그릴 x
		a.op({ 0x01, 0x4d, 0xf0 });														// add [ebp-0x10],ecx         글자 번호
		a.label("tail");
		a.jumpVa(0xe9, kTabDone);
		a.label("orig");
		a.raw(kTabOrg, sizeof(kTabOrg));													// 밀려난 명령 9바이트
		a.jumpVa(0xe9, kTabRet);
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
	Site sites[7] = { { kSizeVa, kSizeOrg, sizeof(kSizeOrg), 0 }, { kBindVa, kBindOrg, sizeof(kBindOrg), 0 }, { kDrawVa, kDrawOrg, sizeof(kDrawOrg), 0 }, { kLayoutVa, kLayoutOrg, sizeof(kLayoutOrg), 0 },
		{ kLoadImageVa, kLoadImageOrg, sizeof(kLoadImageOrg), 0 }, { kCallbackVa, kCallbackOrg, sizeof(kCallbackOrg), 0 }, { kTabVa, kTabOrg, sizeof(kTabOrg), 0 } };
	for (int i = 0; i < 7; i++) {
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

	// 새 섹션 .kfont: [CreateRunObject 후크][글자 그리기][레이아웃][헬퍼 코드][등록 표][글자표들]
	const UINT32 newVa = (sizeOfImage + secAlign - 1) & ~(secAlign - 1);
	const UINT32 baseVa = kImageBase + newVa;
	const UINT32 codeLen = (UINT32)sizeof(kTxtbltFontCode);
	UINT32 bindTrampolineLen = (UINT32)buildBindTrampoline(0, 0, 0).size();
	UINT32 drawTrampolineLen = (UINT32)buildDrawTrampoline(0, 0).size();
	UINT32 layoutTrampolineLen = (UINT32)buildLayoutTrampoline(0, 0).size();
	UINT32 loadImageTrampolineLen = (UINT32)buildLoadImageTrampoline(0).size();
	UINT32 callbackTrampolineLen = (UINT32)buildCallbackTrampoline(0).size();
	UINT32 tabTrampolineLen = (UINT32)buildTabTrampoline(0).size();
	UINT32 bindRel = 0;
	UINT32 drawRel = align4(bindRel + bindTrampolineLen);
	UINT32 layoutRel = align4(drawRel + drawTrampolineLen);
	UINT32 loadImageRel = align4(layoutRel + layoutTrampolineLen);
	UINT32 callbackRel = align4(loadImageRel + loadImageTrampolineLen);
	UINT32 tabRel = align4(callbackRel + callbackTrampolineLen);
	UINT32 codeRel = align4(tabRel + tabTrampolineLen);
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
	Bytes layoutCode = buildLayoutTrampoline(baseVa + layoutRel, baseVa + codeRel + kTxtbltFontLayoutOffset);
	Bytes loadImageCode = buildLoadImageTrampoline(baseVa + loadImageRel);
	Bytes callbackCode = buildCallbackTrampoline(baseVa + callbackRel);
	Bytes tabCode = buildTabTrampoline(baseVa + tabRel);
	if (bindCode.size() != bindTrampolineLen || drawCode.size() != drawTrampolineLen || layoutCode.size() != layoutTrampolineLen || loadImageCode.size() != loadImageTrampolineLen
		|| callbackCode.size() != callbackTrampolineLen || tabCode.size() != tabTrampolineLen) {
		error = L"후크 코드 크기가 예상과 다릅니다.";
		return false;
	}

	Bytes section(newRs, 0xcc);
	memcpy(&section[bindRel], &bindCode[0], bindCode.size());
	memcpy(&section[drawRel], &drawCode[0], drawCode.size());
	memcpy(&section[layoutRel], &layoutCode[0], layoutCode.size());
	memcpy(&section[loadImageRel], &loadImageCode[0], loadImageCode.size());
	memcpy(&section[callbackRel], &callbackCode[0], callbackCode.size());
	memcpy(&section[tabRel], &tabCode[0], tabCode.size());
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
	data[sites[3].off] = 0xe9;										// 레이아웃 함수: 9바이트 -> jmp + nop
	put32(data, sites[3].off + 1, (baseVa + layoutRel) - (sites[3].va + 5));
	memset(&data[sites[3].off + 5], 0x90, sizeof(kLayoutOrg) - 5);
	data[sites[4].off] = 0xe9;										// 폰트 이미지 불러오기: 7바이트 -> jmp + nop
	put32(data, sites[4].off + 1, (baseVa + loadImageRel) - (sites[4].va + 5));
	memset(&data[sites[4].off + 5], 0x90, sizeof(kLoadImageOrg) - 5);
	data[sites[5].off] = 0xe9;										// 글자별 이벤트 플래그: 6바이트 -> jmp + nop
	put32(data, sites[5].off + 1, (baseVa + callbackRel) - (sites[5].va + 5));
	memset(&data[sites[5].off + 5], 0x90, sizeof(kCallbackOrg) - 5);
	data[sites[6].off] = 0xe9;										// 탭 처리: 9바이트 -> jmp + nop
	put32(data, sites[6].off + 1, (baseVa + tabRel) - (sites[6].va + 5));
	memset(&data[sites[6].off + 5], 0x90, sizeof(kTabOrg) - 5);
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
