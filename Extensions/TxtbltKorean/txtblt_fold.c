/* txtblt_fold.c : txtblt.mfx(Text Blitter) 에 한글을 그리게 하는 패치용 헬퍼
 *
 * Text Blitter 는 글자 하나가 1바이트이고, 폰트 이미지의 고정 크기 셀을 글자 코드로 찾는 확장이다.
 * 코드페이지 949 에서 ANSI 로 넘어온 한글(KS X 1001 음절, 앞바이트 B0~C8 + 뒤바이트 A1~FE)을
 * 반셀 코드 두 개로 바꿔 같은 문자열 안에 되돌려 쓴다. 한 음절 = 두 칸이라 줄 계산과 그리기 코드는 그대로 둔다
 *  - 코드는 80~AF, C9~FF 에서 고른다 (앞바이트 범위 B0~C8 을 피하므로 바꾼 문자열을 다시 훑어도 쌍으로 오인되지 않는다)
 *  - 바꾼 코드 -> 아틀라스 칸 번호 표는 글자 표(+0x2f0) 뒤쪽 확장 영역에 둔다. 글자 표 할당 크기를 0x1000 -> 0x2000 으로 늘려 둔다
 *  - 아틀라스: 원래 폰트 이미지 아래쪽에 이어 붙인다. 번역에 쓰이는 음절만 담는다 (표 tbl 이 음절 번호 -> 압축 번호를 알려 준다)
 *    표 tbl: [0]=한 줄 칸 수(perRow), [1]=음절 수, [2+s]=음절 s(KS X 1001 순서 0~2349)의 압축 번호 c (없으면 0xFFFF)
 *    음절의 왼쪽 반셀 = 칸 2c, 오른쪽 = 칸 2c+1, 칸 k 의 위치 = ( (k % perRow) * 셀폭 , 이미지높이 + (k / perRow) * 셀높이 )
 * 레이아웃 함수(0x10008cf0)가 글자 표를 채운 직후에 호출된다. 개체 필드 오프셋은 txtblt.mfx 분석 결과이다
 * 컴파일: cl /c /Oi- /GS- /Zl /Gy- (CRT 를 쓰지 않는 위치 독립 코드)
 */
/* 초기화 루프가 memset 호출로 바뀌지 않도록 최적화를 끈다 (CRT 를 쓰지 않는 코드여야 한다) */
#pragma optimize("", off)

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define FIELD(o, off, T) (*(T*)((u8*)(o) + (off)))

#define MAGIC 0x314C4748u		/* 'HGL1' */

void __cdecl fold_hangul(u8* o, const u16* tbl)
{
	u8* text = FIELD(o, 0x2ac, u8*);
	int len = FIELD(o, 0x2b0, int);
	u8* table = FIELD(o, 0x2f0, u8*);
	int cw = FIELD(o, 0x2c4, int);
	int chh = FIELD(o, 0x2c8, int);
	int ih = FIELD(o, 0x2dc, int);
	u32* st;
	u16* map;
	u32 h;
	int i, c, k;
	u8 used[256];

	if (text == 0 || table == 0 || cw <= 0 || tbl[0] == 0) {
		return;
	}
	st = (u32*)(table + 0x1000);			/* [0]=문자열 해시, [1]=길이, [2]=초기화 표시 */
	map = (u16*)(table + 0x1010);			/* 코드 -> (칸 번호 + 1), 0 이면 안 씀 */

	if (st[2] != MAGIC) {
		for (c = 0; c < 256; c++) { map[c] = 0; }
		st[0] = 0;
		st[1] = 0xFFFFFFFFu;
		st[2] = MAGIC;
	}

	h = 2166136261u;
	for (i = 0; i < len; i++) { h = (h ^ text[i]) * 16777619u; }

	if (h != st[0] || (u32)len != st[1]) {
		/* 새 문자열: 쌍을 반셀 코드로 바꾼다 */
		for (c = 0; c < 256; c++) { map[c] = 0; used[c] = 0; }
		for (i = 0; i < len; ) {
			u8 b = text[i];
			if (b >= 0xB0 && b <= 0xC8 && i + 1 < len && text[i + 1] >= 0xA1 && text[i + 1] <= 0xFE) { i += 2; }
			else { used[b] = 1; i++; }
		}
		for (i = 0; i < len; ) {
			u8 b = text[i];
			if (b >= 0xB0 && b <= 0xC8 && i + 1 < len && text[i + 1] >= 0xA1 && text[i + 1] <= 0xFE) {
				int s = (b - 0xB0) * 94 + (text[i + 1] - 0xA1);
				u16 syl = tbl[2 + s];
				int half;
				for (half = 0; half < 2; half++) {
					int code = 0;
					if (syl != 0xFFFF) {
						u16 want = (u16)(syl * 2 + half + 1);
						for (c = 0x80; c < 0x100; c++) { if (map[c] == want) { code = c; break; } }
						if (code == 0) {
							for (c = 0x80; c < 0x100; c++) {
								if (c >= 0xB0 && c <= 0xC8) { continue; }
								if (map[c] != 0 || used[c]) { continue; }
								map[c] = want;
								code = c;
								break;
							}
						}
					}
					text[i + half] = (u8)(code != 0 ? code : '?');
				}
				i += 2;
			}
			else { i++; }
		}
		h = 2166136261u;
		for (i = 0; i < len; i++) { h = (h ^ text[i]) * 16777619u; }
		st[0] = h;
		st[1] = (u32)len;
	}

	/* 글자 표는 레이아웃을 할 때마다 다시 채워지므로, 바꾼 코드의 칸 위치를 매번 넣어 준다 */
	k = tbl[0];
	for (c = 0x80; c < 0x100; c++) {
		if (map[c] != 0) {
			int cell = map[c] - 1;
			FIELD(table, c * 16, int) = (cell % k) * cw;
			FIELD(table, c * 16 + 4, int) = ih + (cell / k) * chh;
		}
	}
}


/* 폰트 표면(생성자가 편집 데이터의 크기로 만든다)을 아틀라스까지 담도록 높이를 돌려준다
 * CreateRunObject 의 표면 생성 호출(0x100095bb)에서 높이 인자로 쓰인다. 폭은 패치할 때 정해 넣는다 */
int __cdecl atlas_height(u8* o, const u16* tbl)
{
	int chh = FIELD(o, 0x2c8, int);
	int ih = FIELD(o, 0x2dc, int);
	int per = tbl[0];
	int rows;

	if (per <= 0) {
		return ih;
	}
	rows = (tbl[1] * 2 + per - 1) / per;
	return ih + rows * chh;
}
