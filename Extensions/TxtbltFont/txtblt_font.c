/* txtblt_font.c : txtblt.mfx(Text Blitter) 의 Hangul 방식 헬퍼 (폰트 아틀라스 글자표 KFT1 조회)
 *
 * 모듈에 심는 32비트 코드이다. 도구(Glyph/FontAtlas)가 만든 글자표를 모듈 안에 넣어 두고,
 * 오브젝트가 만들어질 때 편집 데이터의 글자표 번호로 글자표를 찾아 런타임 개체에 연결한다 (kfont_bind).
 * 글자를 그릴 때는 코드(1바이트 또는 CP949 2바이트)로 글자 정보를 찾는다 (kfont_get).
 *
 * 개체(rd = 런타임 개체, ed = 편집 데이터) 필드:
 *   rd+0x418 : 이 오브젝트의 글자표 포인터 (없으면 0 -> 원래 방식으로 그린다). GetRunObjectDataSize 를 0x420 으로 늘려 둔다
 *   ed+0x2030, ed+0x2034 : 글자표 번호를 담는 편집 데이터 꼬리 (u32 'KFNT' = 0x544E464B, u32 번호). 원본 오브젝트는 이 영역이 0 이다
 *                          (폰트 이미지 핸들 ed+0x5d0 은 런타임이 다른 값으로 바꿔서 쓰기 때문에 열쇠로 쓸 수 없다)
 *   ed+0 : 편집 데이터 블록 크기 (u32). 꼬리가 없는 짧은 블록은 연결하지 않는다
 *
 *   ed+0x2038 : 레이아웃 플래그 (u32, 블록 크기 0x203C 이상일 때). bit0 = 줄 폭을 글자 폭(advance)의 합으로 계산한다 -> rd+0x41c
 *
 * 등록 표 KFR1: u32 'KFR1', u32 개수, { u32 글자표 번호, u32 글자표 위치(등록 표 시작 기준) } x 개수
 * 글자표 KFT1 (Glyph/FontAtlas.h 설명 참고): 헤더 32바이트(+14 = 빠진 글자 advance u16), +32 = 페이지 목록 u32 x 256,
 *   페이지 = 글자 x 256, 글자 8바이트 { u16 x, u16 y, u8 폭, u8 높이, s8 가로 보정, u8 advance }
 * 컴파일: cl /c /Oi- /GS- /Zl /Gy- (CRT 를 쓰지 않는 위치 독립 코드)
 */
/* 초기화 루프가 memset 호출로 바뀌지 않도록 최적화를 끈다 (CRT 를 쓰지 않는 코드여야 한다) */
#pragma optimize("", off)

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define KFR_MAGIC 0x3152464Bu		/* 'KFR1' */
#define KFNT_MAGIC 0x544E464Bu		/* 'KFNT' */

/* 오브젝트가 만들어질 때 호출된다: 편집 데이터에 적힌 번호의 글자표를 rd+0x418 에 둔다 (없으면 0) */
void __stdcall kfont_bind(u8* rd, const u8* ed, const u8* reg)
{
	u32 id;
	u32 count;
	u32 i;

	*(const u8**)(rd + 0x418) = 0;
	*(u32*)(rd + 0x41c) = 0;
	if (*(const u32*)ed < 0x2038 || *(const u32*)(ed + 0x2030) != KFNT_MAGIC || *(const u32*)reg != KFR_MAGIC) {
		return;
	}
	id = *(const u32*)(ed + 0x2034);
	if (*(const u32*)ed >= 0x203C) {
		*(u32*)(rd + 0x41c) = *(const u32*)(ed + 0x2038);
	}
	count = *(const u32*)(reg + 4);
	for (i = 0; i < count; i++) {
		const u32* entry = (const u32*)(reg + 8 + i * 8);
		if (entry[0] == id) {
			*(const u8**)(rd + 0x418) = reg + entry[1];
			return;
		}
	}
}

/* 코드 하나의 글자 정보 8바이트를 out 에 채운다. 글자표에 없으면 폭/높이 0 + 빠진 글자 advance
 * (같은 파일 안의 다른 함수가 부르면 링커가 주소를 채워야 해서 위치 독립 코드가 아니게 되므로, 본문은 정적 함수에 두고
 *  공개 kfont_get 은 그것을 부르기만 한다. 이 파일의 다른 함수들도 정적 함수만 부른다) */
static void __stdcall kfont_get_impl(const u8* t, u32 code, u8* out)
{
	u32 page = *(const u32*)(t + 32 + ((code >> 8) & 0xFF) * 4);
	const u8* rec;
	int i;

	if (page != 0) {
		rec = t + page + (code & 0xFF) * 8;
		if (rec[4] != 0 || rec[7] != 0) {
			for (i = 0; i < 8; i++) {
				out[i] = rec[i];
			}
			return;
		}
	}
	for (i = 0; i < 8; i++) {
		out[i] = 0;
	}
	rec = t + 14;
	out[7] = (*(const u16*)rec > 255) ? 255 : (u8)*(const u16*)rec;
}

void __stdcall kfont_get(const u8* t, u32 code, u8* out)
{
	kfont_get_impl(t, code, out);
}


/* ---------------------------------------------------------------------------------------------
 * 레이아웃: 줄 폭을 글자 폭(advance)의 합으로 계산한다 (원본 배치 함수 FUN_10002a73 을 대체한다)
 *
 * 바뀌는 것은 줄 폭을 재는 기준 하나뿐이다.
 *   원본: 한 글자 = 한 칸 (바이트 수 x 셀 폭)
 *   여기: 글자표에 적힌 글자별 폭의 합 (영문은 좁고 한글은 넓다)
 * 이 폭으로 자동 줄바꿈과 가운데/오른쪽 정렬을 계산하는 방식은 원본과 같다. 글자마다 그려지는 폭이 다른데
 * 원본 방식으로 재면 계산한 폭과 그려지는 폭이 어긋나서 줄바꿈 위치가 틀어진다.
 *
 * 원본에는 폭 계산만 하는 별도 함수가 없고, 배치 함수 안에서 인라인으로 계산한다. 그래서 폭만 바꿀 수 있는
 * 후크 지점이 없어, 배치 함수 전체(이스케이프, CR/LF, 자동 줄바꿈, 정렬, 줄 노드 만들기)를 원본 로직 그대로
 * 다시 구현하고 폭 계산만 다르게 한다. 오브젝트의 layout 이 byte 이면 원본 배치 함수로 넘어간다.
 *
 * 줄마다 두 값을 따로 센다: bytes = 줄 노드에 담는 바이트 수(그릴 글자 수), width = 글자 폭의 합(픽셀).
 * 한글(2바이트)은 한 글자로 다루어서 줄이 쌍 사이에서 끊기지 않는다. 그 밖의 규칙(공백 뒤 단어 미리보기,
 * 단어 중간 강제 줄바꿈, 백슬래시 이스케이프와 CR/LF 줄바꿈 처리, 앞 공백 건너뛰기)은 원본과 같다.
 *
 * 원본 개체 필드: +0x2ac 문자열, +0x2b0 길이, +0x60/+0x314/+0x30c 폭 관련, +0x2c4/+0x2cc 셀 폭/간격, +0x2e8 플래그
 *   (0x02 = 백슬래시 이스케이프, 0x20 = 자동 줄바꿈, 0x40 = CR/LF 줄바꿈, 0x20000 = 상자), +0x31c 정렬(1 가운데, 2 오른쪽), +0x32c 출력 모드
 * 줄 노드: next(+0), 시작 포인터(+4), 바이트 수(+8), 가로 위치(+0x10). newNode 는 원본의 노드 만드는 함수(FUN_10002a3b)이다
 * --------------------------------------------------------------------------------------------- */
typedef void* (__cdecl *NewNodeFn)(void* prev, const u8* start, int count, int x, int zero);
typedef void (__cdecl *DelNodeFn)(void* node);

/* 코드 하나의 advance (글자표에 없으면 빠진 글자 폭) */
static int kfont_adv(const u8* t, u32 code)
{
	u8 rec[8];
	kfont_get_impl(t, code, rec);
	return rec[7];
}

/* 문자열 위치의 글자 하나: 2바이트(CP949)이면 2, 아니면 1을 돌려주고 code 를 채운다 */
static int kfont_unit(const u8* p, u32* code)
{
	if (p[0] >= 0x80 && p[1] != 0) {
		*code = ((u32)p[0] << 8) | p[1];
		return 2;
	}
	*code = p[0];
	return 1;
}

/* 줄의 가로 위치: 가운데 / 오른쪽 정렬 (원본 식. 출력 모드 0,1,2 에서만 정렬한다) */
static int kfont_align(int mode, int center, int right, int width, int lineWidth)
{
	int x = 0;
	if (mode == 0 || mode == 1 || mode == 2) {
		if (center) {
			int v = width - lineWidth;
			x = (v - (v >> 31)) >> 1;
		}
		if (right) {
			x = width - lineWidth;
		}
	}
	return x;
}

void __cdecl kfont_layout(u8* rd, NewNodeFn newNode, DelNodeFn delNode, const u8* emptyStr)
{
	const u8* t = *(const u8**)(rd + 0x418);
	const u8* text = *(const u8**)(rd + 0x2ac);
	int len = *(int*)(rd + 0x2b0);
	int width, step, flags, mode, align, center, right, crlf, wrap, esc;
	void* node;
	void* next;
	void* prev;
	const u8* start;
	const u8* p;
	int idx = 0;
	int bytes = 0;
	int lw = 0;
	int lines = 0;
	int maxBytes = 0;

	/* 기존 줄 노드 해제 */
	node = *(void**)(rd + 0x2a4);
	while (node != 0) {
		next = *(void**)node;
		delNode(node);
		node = next;
	}

	width = *(int*)(rd + 0x60) - *(int*)(rd + 0x314) - *(int*)(rd + 0x30c);
	step = *(int*)(rd + 0x2cc) + *(int*)(rd + 0x2c4);
	flags = *(int*)(rd + 0x2e8);
	mode = *(u8*)(rd + 0x32c);
	align = *(int*)(rd + 0x31c);
	center = (align == 1);
	right = (align == 2);
	crlf = flags & 0x40;
	wrap = flags & 0x20;
	esc = flags & 0x02;
	*(int*)(rd + 0x37c) = 0;

	/* 글자 위치표(+0x2f0): 탭 등 원래 방식으로 그리는 경로가 쓰므로 원본과 같이 채워 둔다 */
	{
		u8* table = *(u8**)(rd + 0x2f0);
		const u8* charmap = *(const u8**)(rd + 0x2ec);
		int cellW = *(int*)(rd + 0x2c4);
		int cellH = *(int*)(rd + 0x2c8);
		int cols = (cellW != 0) ? (*(int*)(rd + 0x2d8) / cellW) : 0;
		int i;
		for (i = 0; i < 0x1000; i++) {
			table[i] = 0;
		}
		if (cols > 0) {
			for (i = 0; ; i++) {
				u32 ch = charmap[i];
				*(int*)(table + ch * 16) = cellW * (i % cols);
				*(int*)(table + ch * 16 + 4) = cellH * (i / cols);
				if (charmap[i] == 0) {
					break;
				}
			}
		}
	}

	*(int*)(rd + 0x308) = 0;
	/* 머리 노드: 원본은 가운데 정렬이면 폭의 절반, 오른쪽 정렬이면 폭 전체를 가로 위치로 둔다 */
	{
		int headX = 0;
		if (mode == 0 || mode == 1 || mode == 2) {
			if (center) {
				headX = (width - (width >> 31)) >> 1;
			}
			if (right) {
				headX = width;
			}
		}
		prev = newNode(0, emptyStr, 1, headX, 0);
	}
	*(void**)(rd + 0x2a4) = prev;

	start = text;
	p = text;
	while (*p != 0 && idx < len) {
		u8 c = *p;
		int lineEnd = 0;		/* 이 글자에서 줄을 끝내는가 */
		int endBytes = 0, endWidth = 0;

		if (c == 9) {													/* 탭: 한 칸 */
			p++;
			bytes++;
			lw += step;
		}
		else if (c == 10) {												/* LF */
			p++;
			if (!crlf) {
				bytes++;
				lw += kfont_adv(t, 10);
			}
		}
		else if (c == 13) {												/* CR (+LF) */
			p++;
			if (!crlf) {
				bytes++;
				lw += kfont_adv(t, 13);
				if (*p == 10) {
					bytes++;
					lw += kfont_adv(t, 10);
				}
			}
			if (*p == 10) {
				p++;
			}
			if (crlf) {
				lineEnd = 1;
				endBytes = bytes;
				endWidth = lw;
			}
		}
		else if (c == 0x5c) {											/* 역슬래시 */
			p++;
			bytes++;
			lw += kfont_adv(t, 0x5c);
			if (esc) {
				if (*p == 0x5c) {										/* \ -> 두 칸 */
					p++;
					bytes++;
					lw += kfont_adv(t, 0x5c);
				}
				else if (*p == 0x6e) {									/* 
 */
					p++;
					bytes--;
					lw -= kfont_adv(t, 0x5c);
					if (!crlf) {
						bytes += 2;
						lw += kfont_adv(t, 0x5c) + kfont_adv(t, 0x6e);
					}
					else {
						lineEnd = 1;
						endBytes = bytes;
						endWidth = lw;
					}
				}
			}
		}
		else if (c == 0x20) {											/* 공백 */
			p++;
			bytes++;
			lw += kfont_adv(t, 0x20);
			if (wrap) {
				/* 다음 구분(CR, LF, 공백, 끝)까지의 단어가 이 줄에 들어가는지 미리 본다 */
				const u8* q = p;
				int wl = lw;
				int fits = 1;
				for (;;) {
					u32 code;
					int n;
					if (wl > width) {
						fits = 0;
						break;
					}
					if (*q == 13 || *q == 10 || *q == 0x20 || *q == 0) {
						break;
					}
					n = kfont_unit(q, &code);
					wl += kfont_adv(t, code);
					q += n;
				}
				if (!fits) {
					lineEnd = 1;
					endBytes = bytes;
					endWidth = lw;
				}
			}
		}
		else {															/* 그 밖의 글자 (1바이트 또는 CP949 2바이트) */
			u32 code;
			int n = kfont_unit(p, &code);
			int w = kfont_adv(t, code);
			if (wrap && lw + w > width) {
				/* 이 글자 앞에서 줄을 끊고, 이 글자는 새 줄의 첫 글자가 된다 */
				void* made = newNode(prev, start, bytes, kfont_align(mode, center, right, width, lw), 0);
				lines++;
				if (bytes > maxBytes) {
					maxBytes = bytes;
				}
				prev = made;
				start = p;
				bytes = 0;
				lw = 0;
			}
			p += n;
			bytes += n;
			lw += w;
		}

		if (lineEnd) {
			void* made = newNode(prev, start, endBytes, kfont_align(mode, center, right, width, endWidth), 0);
			lines++;
			if (endBytes > maxBytes) {
				maxBytes = endBytes;
			}
			prev = made;
			bytes = 0;
			lw = 0;
			if (wrap && *p == 0x20) {
				p++;
			}
			start = p;
		}
		idx++;
	}

	/* 마지막 줄 */
	{
		void* made = newNode(prev, start, bytes, kfont_align(mode, center, right, width, lw), 0);
		lines++;
		if (bytes > maxBytes) {
			maxBytes = bytes;
		}
		*(void**)(rd + 0x2a8) = made;
	}
	*(int*)(rd + 0x308) = lines;
	*(int*)(rd + 0x37c) = maxBytes;
}
