// txtblt 한글 접기 헬퍼(Extensions/TxtbltKorean/txtblt_fold.c)의 자체 테스트
//
// 헬퍼는 txtblt.mfx 에 심는 32비트 코드이므로 32비트로 빌드한다 (build_txtblt_fold_test.bat).
// 실제 개체 대신 필요한 필드만 채운 메모리를 넘겨 접기 동작을 확인한다
//   개체 필드: +0x2ac 문자열, +0x2b0 길이, +0x2f0 글자 표(0x2000 바이트), +0x2c4 셀 폭, +0x2c8 셀 높이, +0x2dc 이미지 높이

#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

extern "C" {
	void __cdecl fold_hangul(unsigned char* o, const unsigned short* tbl);
	int __cdecl atlas_height(unsigned char* o, const unsigned short* tbl);
}

static int failureCount = 0;

static void check(bool ok, const char* what)
{
	if (!ok) {
		failureCount++;
		printf("  FAIL: %s\n", what);
	}
}

static const int kCellW = 16, kCellH = 32, kImageH = 224, kPerRow = 32;

// 개체 한 개를 흉내 낸다
struct FakeObject
{
	std::vector<unsigned char> obj;
	std::vector<unsigned char> text;
	std::vector<unsigned char> table;

	FakeObject(const unsigned char* bytes, int len) : obj(0x500, 0), text(bytes, bytes + len + 1), table(0x2000, 0)
	{
		put(0x2ac, (unsigned int)(size_t)&text[0]);
		put(0x2b0, (unsigned int)len);
		put(0x2f0, (unsigned int)(size_t)&table[0]);
		put(0x2c4, kCellW);
		put(0x2c8, kCellH);
		put(0x2dc, kImageH);
	}
	void put(int off, unsigned int v) { memcpy(&obj[off], &v, 4); }
	int tableX(int code) { int v; memcpy(&v, &table[code * 16], 4); return v; }
	int tableY(int code) { int v; memcpy(&v, &table[code * 16 + 4], 4); return v; }
	// 레이아웃 함수는 부를 때마다 글자 표(앞쪽 0x1000)를 지우고 다시 채운다
	void clearTable() { memset(&table[0], 0, 0x1000); }
	void fold(const std::vector<unsigned short>& tbl) { fold_hangul(&obj[0], &tbl[0]); }
};

// 음절 표: 가(B0A1)=0, 나(B3AA)=1, 다(B4D9)=2 만 담는다 (압축 번호 순)
static std::vector<unsigned short> makeTable()
{
	std::vector<unsigned short> t(2 + 2350, 0xFFFF);
	t[0] = kPerRow;
	t[1] = 3;
	t[2 + 0] = 0;							// 가: 앞 B0 뒤 A1 -> 번호 0
	t[2 + (0xB3 - 0xB0) * 94 + (0xAA - 0xA1)] = 1;		// 나
	t[2 + (0xB4 - 0xB0) * 94 + (0xD9 - 0xA1)] = 2;		// 다
	return t;
}

static bool isFoldedCode(unsigned char c) { return c >= 0x80 && !(c >= 0xB0 && c <= 0xC8); }

int main()
{
	std::vector<unsigned short> tbl = makeTable();

	printf("ASCII 문자열은 그대로 둔다\n");
	{
		const unsigned char s[] = "Hello, world!\r\nA B";
		FakeObject o(s, sizeof(s) - 1);
		o.fold(tbl);
		check(memcmp(&o.text[0], s, sizeof(s) - 1) == 0, "ASCII 가 바뀜");
	}

	printf("한글 쌍을 같은 길이의 반셀 코드 두 개로 접는다\n");
	{
		const unsigned char s[] = { 'A', 0xB0, 0xA1, 0xB3, 0xAA, 'B', 0 };		// A 가 나 B
		FakeObject o(s, 6);
		o.fold(tbl);
		check(o.text[0] == 'A' && o.text[5] == 'B' && o.text[6] == 0, "ASCII 위치/끝이 바뀜");
		check(isFoldedCode(o.text[1]) && isFoldedCode(o.text[2]) && isFoldedCode(o.text[3]) && isFoldedCode(o.text[4]), "접은 코드가 앞바이트 범위(B0~C8)이거나 80 미만");
		check(o.text[1] != o.text[2] && o.text[3] != o.text[1], "코드가 겹침");
		// 가: 압축 0 -> 칸 0, 1 / 나: 압축 1 -> 칸 2, 3
		check(o.tableX(o.text[1]) == 0 * kCellW && o.tableY(o.text[1]) == kImageH, "가 왼쪽 반셀 위치");
		check(o.tableX(o.text[2]) == 1 * kCellW && o.tableY(o.text[2]) == kImageH, "가 오른쪽 반셀 위치");
		check(o.tableX(o.text[3]) == 2 * kCellW && o.tableY(o.text[3]) == kImageH, "나 왼쪽 반셀 위치");
		check(o.tableX(o.text[4]) == 3 * kCellW && o.tableY(o.text[4]) == kImageH, "나 오른쪽 반셀 위치");
	}

	printf("같은 음절은 같은 코드를 쓴다\n");
	{
		const unsigned char s[] = { 0xB0, 0xA1, 0xB0, 0xA1, 0 };
		FakeObject o(s, 4);
		o.fold(tbl);
		check(o.text[0] == o.text[2] && o.text[1] == o.text[3], "같은 음절의 코드가 다름");
	}

	printf("접은 문자열을 다시 훑어도 그대로이고, 지워진 글자 표를 다시 채운다\n");
	{
		const unsigned char s[] = { 'A', 0xB0, 0xA1, 0xB3, 0xAA, 0 };
		FakeObject o(s, 5);
		o.fold(tbl);
		std::vector<unsigned char> folded(o.text.begin(), o.text.end());
		int x1 = o.tableX(o.text[1]), y1 = o.tableY(o.text[1]);
		o.clearTable();
		o.fold(tbl);
		check(o.text == folded, "다시 훑었더니 문자열이 바뀜");
		check(o.tableX(o.text[1]) == x1 && o.tableY(o.text[1]) == y1, "글자 표가 복원되지 않음");
	}

	printf("새 문자열이 들어오면 처음부터 다시 접는다\n");
	{
		const unsigned char s[] = { 0xB0, 0xA1, 0 };
		FakeObject o(s, 2);
		o.fold(tbl);
		unsigned char first = o.text[0];
		const unsigned char s2[] = { 0xB3, 0xAA, 0 };		// 나
		memcpy(&o.text[0], s2, 3);
		o.clearTable();
		o.fold(tbl);
		check(isFoldedCode(o.text[0]) && isFoldedCode(o.text[1]), "새 문자열을 접지 못함");
		check(o.tableX(o.text[0]) == 2 * kCellW, "새 문자열의 칸 위치가 틀림");
		(void)first;
	}

	printf("표에 없는 음절은 ? 두 개로 둔다\n");
	{
		const unsigned char s[] = { 0xB0, 0xA2, 0 };		// 각 (표에 없음)
		FakeObject o(s, 2);
		o.fold(tbl);
		check(o.text[0] == '?' && o.text[1] == '?', "표에 없는 음절이 ? 로 바뀌지 않음");
	}

	printf("코드가 모자라면 ? 로 대신하고 죽지 않는다\n");
	{
		// 서로 다른 음절 60개 (코드 51쌍을 넘는다). 표에는 모두 있다고 친다
		std::vector<unsigned short> big(2 + 2350, 0xFFFF);
		big[0] = kPerRow;
		big[1] = 60;
		std::vector<unsigned char> s;
		for (int i = 0; i < 60; i++) {
			unsigned char lead = (unsigned char)(0xB0 + i / 94), trail = (unsigned char)(0xA1 + i % 94);
			s.push_back(lead);
			s.push_back(trail);
			big[2 + i] = (unsigned short)i;
		}
		FakeObject o(&s[0], (int)s.size());
		o.fold(big);
		int questions = 0, folded = 0;
		for (size_t i = 0; i < s.size(); i++) {
			if (o.text[i] == '?') { questions++; }
			else if (isFoldedCode(o.text[i])) { folded++; }
		}
		check(folded > 0 && questions > 0 && folded + questions == (int)s.size(), "코드 부족 시 동작이 이상함");
		printf("  접힌 바이트 %d, ? %d\n", folded, questions);
	}

	printf("아틀라스 표면 높이 = 이미지 높이 + 필요한 줄 수 * 셀 높이\n");
	{
		const unsigned char s[] = { 'A', 0 };
		FakeObject o(s, 1);
		// 음절 3개 -> 반셀 6칸 -> 한 줄 32칸이므로 1줄
		check(atlas_height(&o.obj[0], &tbl[0]) == kImageH + 1 * kCellH, "높이 계산이 틀림");
		std::vector<unsigned short> many = tbl;
		many[1] = 100;										// 반셀 200칸 -> 7줄
		check(atlas_height(&o.obj[0], &many[0]) == kImageH + 7 * kCellH, "여러 줄 높이 계산이 틀림");
	}

	if (failureCount == 0) {
		printf("ALL PASSED\n");
		return 0;
	}
	printf("%d FAILURES\n", failureCount);
	return 1;
}
