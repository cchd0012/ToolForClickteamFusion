// txtblt 글자표 헬퍼(Extensions/TxtbltFont/txtblt_font.c)의 자체 테스트
//
// 헬퍼는 txtblt.mfx 에 심는 32비트 코드이므로 32비트로 빌드한다 (build_txtblt_font_test.bat).
// 인자: [패치된 txtblt.mfx] [글자표 .kft ...]
//   - 모듈이 주어지면 .kfont 섹션의 등록 표를 읽어, 등록된 모든 글자표 번호가 kfont_bind 로 올바른 글자표에 연결되는지 확인한다
//   - .kft 가 주어지면 65536개 코드를 kfont_get 으로 조회해 형식 설명대로 읽은 값과 같은지 확인한다

#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

extern "C" {
	void __stdcall kfont_bind(unsigned char* rd, const unsigned char* ed, const unsigned char* reg);
	void __stdcall kfont_get(const unsigned char* t, unsigned int code, unsigned char* out);
}

static int failureCount = 0;

static void check(bool ok, const char* what)
{
	if (!ok) {
		failureCount++;
		printf("  FAIL: %s\n", what);
	}
}

static bool readFile(const char* path, std::vector<unsigned char>& out)
{
	FILE* f = fopen(path, "rb");
	if (f == NULL) {
		return false;
	}
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	out.resize((size_t)size);
	bool ok = (size == 0) || (fread(&out[0], 1, (size_t)size, f) == (size_t)size);
	fclose(f);
	return ok;
}

static unsigned int u32(const unsigned char* p) { unsigned int v; memcpy(&v, p, 4); return v; }
static unsigned short u16(const unsigned char* p) { unsigned short v; memcpy(&v, p, 2); return v; }

// 형식 설명(Glyph/FontAtlas.h)대로 글자를 읽는다. 없으면 false
static bool referenceGlyph(const std::vector<unsigned char>& t, unsigned int code, unsigned char* out)
{
	unsigned int page = u32(&t[32 + ((code >> 8) & 0xFF) * 4]);
	if (page == 0) {
		return false;
	}
	const unsigned char* rec = &t[page + (code & 0xFF) * 8];
	if (rec[4] == 0 && rec[7] == 0) {
		return false;
	}
	memcpy(out, rec, 8);
	return true;
}

static void testTable(const char* path)
{
	std::vector<unsigned char> t;
	if (!readFile(path, t)) {
		printf("  글자표를 열지 못했습니다: %s\n", path);
		failureCount++;
		return;
	}
	printf("글자표 %s (%u바이트)\n", path, (unsigned)t.size());
	unsigned int bad = 0, present = 0;
	unsigned char missingAdvance = (u16(&t[14]) > 255) ? 255 : (unsigned char)u16(&t[14]);
	for (unsigned int code = 0; code < 0x10000; code++) {
		unsigned char expected[8], got[8];
		memset(got, 0xAA, 8);
		kfont_get(&t[0], code, got);
		if (referenceGlyph(t, code, expected)) {
			present++;
			if (memcmp(expected, got, 8) != 0) { bad++; }
		}
		else {
			unsigned char none[8] = { 0, 0, 0, 0, 0, 0, 0, missingAdvance };
			if (memcmp(none, got, 8) != 0) { bad++; }
		}
	}
	printf("  글자 %u개, 불일치 %u개\n", present, bad);
	check(bad == 0, "kfont_get 결과가 형식 설명과 다름");
}

static void testModule(const char* path)
{
	std::vector<unsigned char> m;
	if (!readFile(path, m)) {
		printf("모듈을 열지 못했습니다: %s\n", path);
		failureCount++;
		return;
	}
	unsigned int e = u32(&m[0x3c]);
	unsigned int nsec = u16(&m[e + 6]);
	unsigned int sec0 = e + 24 + u16(&m[e + 20]);
	const unsigned char* kfont = NULL;
	unsigned int kfontSize = 0;
	for (unsigned int i = 0; i < nsec; i++) {
		const unsigned char* s = &m[sec0 + i * 40];
		if (memcmp(s, ".kfont", 6) == 0) {
			kfont = &m[u32(s + 20)];
			kfontSize = u32(s + 16);
		}
	}
	if (kfont == NULL) {
		printf("모듈에 .kfont 섹션이 없습니다\n");
		failureCount++;
		return;
	}
	// 등록 표 찾기: 'KFR1' 으로 시작하는 위치 (4바이트 정렬)
	const unsigned char* reg = NULL;
	for (unsigned int o = 0; o + 8 < kfontSize; o += 4) {
		if (u32(kfont + o) == 0x3152464B) { reg = kfont + o; break; }
	}
	if (reg == NULL) {
		printf("등록 표(KFR1)를 찾지 못했습니다\n");
		failureCount++;
		return;
	}
	unsigned int count = u32(reg + 4);
	printf("모듈 %s: 등록 핸들 %u개\n", path, count);

	std::vector<unsigned char> rd(0x500, 0xEE), ed(0x2044, 0);
	unsigned int size = 0x2044, magic = 0x544E464B;
	memcpy(&ed[0], &size, 4);
	memcpy(&ed[0x2030], &magic, 4);
	unsigned int bad = 0;
	for (unsigned int i = 0; i < count; i++) {
		unsigned int id = u32(reg + 8 + i * 8), offset = u32(reg + 8 + i * 8 + 4);
		memcpy(&ed[0x2034], &id, 4);
		memset(&rd[0], 0xEE, rd.size());
		kfont_bind(&rd[0], &ed[0], reg);
		const unsigned char* got = (const unsigned char*)(size_t)u32(&rd[0x418]);
		if (got != reg + offset || u32(&rd[0x41c]) != 0) {
			if (bad < 5) { printf("  번호 %u: 기대 %p 실제 %p\n", id, (const void*)(reg + offset), (const void*)got); }
			bad++;
		}
		else if (u32(got) != 0x3154464B) {
			bad++;
		}
	}
	printf("  연결 불일치 %u개\n", bad);
	check(bad == 0, "등록된 핸들이 올바른 글자표에 연결되지 않음");

	// 등록되지 않은 번호, 꼬리 표식이 없는 데이터, 짧은 블록은 모두 0 이어야 한다
	unsigned int unknown = 12345;
	for (unsigned int i = 0; i < count; i++) { if (u32(reg + 8 + i * 8) == unknown) { unknown = 54321; } }
	memcpy(&ed[0x2034], &unknown, 4);
	memset(&rd[0], 0xEE, rd.size());
	kfont_bind(&rd[0], &ed[0], reg);
	check(u32(&rd[0x418]) == 0 && u32(&rd[0x41c]) == 0, "등록되지 않은 번호가 0 으로 연결되지 않음");

	unsigned int first = u32(reg + 8);
	memcpy(&ed[0x2034], &first, 4);
	unsigned int noMagic = 0;
	memcpy(&ed[0x2030], &noMagic, 4);
	memset(&rd[0], 0xEE, rd.size());
	kfont_bind(&rd[0], &ed[0], reg);
	check(u32(&rd[0x418]) == 0, "표식이 없는 편집 데이터가 연결됨");

	memcpy(&ed[0x2030], &magic, 4);
	unsigned int shortSize = 0x1c70;
	memcpy(&ed[0], &shortSize, 4);
	memset(&rd[0], 0xEE, rd.size());
	kfont_bind(&rd[0], &ed[0], reg);
	check(u32(&rd[0x418]) == 0, "짧은 편집 데이터 블록이 연결됨");
}

int main(int argc, char** argv)
{
	for (int i = 1; i < argc; i++) {
		size_t n = strlen(argv[i]);
		if (n > 4 && strcmp(argv[i] + n - 4, ".kft") == 0) {
			testTable(argv[i]);
		}
		else {
			testModule(argv[i]);
		}
	}
	if (failureCount == 0) {
		printf("ALL PASSED\n");
		return 0;
	}
	printf("%d FAILURES\n", failureCount);
	return 1;
}
