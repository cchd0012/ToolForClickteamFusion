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
	count = *(const u32*)(reg + 4);
	for (i = 0; i < count; i++) {
		const u32* entry = (const u32*)(reg + 8 + i * 8);
		if (entry[0] == id) {
			*(const u8**)(rd + 0x418) = reg + entry[1];
			return;
		}
	}
}

/* 코드 하나의 글자 정보 8바이트를 out 에 채운다. 글자표에 없으면 폭/높이 0 + 빠진 글자 advance */
void __stdcall kfont_get(const u8* t, u32 code, u8* out)
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
