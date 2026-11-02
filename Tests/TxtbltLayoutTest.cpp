// txtblt 레이아웃 헬퍼(Extensions/TxtbltFont/txtblt_font.c 의 kfont_layout)의 자체 테스트
//
// 모든 글자의 폭(advance)을 칸 폭(셀 폭 + 간격)의 배수로 맞춘 글자표를 쓰면, 줄 폭 합산 결과는 원본 레이아웃(글자 = 칸 수)과
// 같아야 한다. 그래서 원본 알고리즘(txtblt.mfx 의 FUN_10002a73)을 칸 수 기준으로 옮긴 기준 구현과 무작위 문자열로 비교한다.
// 한글(2바이트)은 한 글자로 다루므로, 기준 구현도 2바이트 쌍을 한 글자(칸 2개)로 본다 (원본은 쌍 사이에서 줄이 끊길 수 있다).
// 32비트로 빌드한다 (build_txtblt_layout_test.bat).

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

typedef void* (__cdecl *NewNodeFn)(void* prev, const unsigned char* start, int count, int x, int zero);
typedef void (__cdecl *DelNodeFn)(void* node);

extern "C" {
	void __cdecl kfont_layout(unsigned char* rd, NewNodeFn newNode, DelNodeFn delNode, const unsigned char* emptyStr);
}

struct Node
{
	void* next;
	const unsigned char* start;
	int count;
	int index;
	int x;
	int y;
};

struct Line
{
	int offset;			// 문자열 안의 시작 위치 (머리 노드는 -1)
	int count;
	int x;
	bool operator==(const Line& o) const { return offset == o.offset && count == o.count && x == o.x; }
};

static const unsigned char* gTextBase = NULL;
static const unsigned char* gEmpty = NULL;
static std::vector<Line> gLines;
static std::vector<Node*> gAllocated;

static void* __cdecl fakeNewNode(void* prev, const unsigned char* start, int count, int x, int zero)
{
	(void)zero;
	Node* n = new Node();
	n->next = NULL;
	n->start = start;
	n->count = count;
	n->index = prev ? ((Node*)prev)->index + 1 : 0;
	n->x = x;
	n->y = 0;
	if (prev != NULL) {
		((Node*)prev)->next = n;
	}
	Line l;
	l.offset = (start == gEmpty) ? -1 : (int)(start - gTextBase);
	l.count = count;
	l.x = x;
	gLines.push_back(l);
	gAllocated.push_back(n);
	return n;
}

static void __cdecl fakeDeleteNode(void* node)
{
	(void)node;			// 시험에서는 gAllocated 로 한꺼번에 지운다
}

static int failureCount = 0;

static void check(bool ok, const char* what)
{
	if (!ok) {
		failureCount++;
		printf("  FAIL: %s\n", what);
	}
}

// ---- 글자표(KFT1) 만들기: 아스키 advance = step, 한글 쌍 advance = 2 * step, 그 밖의 코드(빠진 글자) = step
static std::vector<unsigned char> makeTable(int step)
{
	const size_t pages = 1 + (0xC8 - 0xB0 + 1);
	std::vector<unsigned char> t(32 + 256 * 4 + pages * 256 * 8, 0);
	unsigned int magic = 0x3154464B;
	memcpy(&t[0], &magic, 4);
	unsigned short v = 1; memcpy(&t[4], &v, 2);
	unsigned short miss = (unsigned short)step; memcpy(&t[14], &miss, 2);
	size_t next = 32 + 256 * 4;
	unsigned int off = (unsigned int)next;
	memcpy(&t[32], &off, 4);
	for (int c = 0x20; c < 0x7F; c++) { t[next + c * 8 + 7] = (unsigned char)step; }
	next += 256 * 8;
	for (int lead = 0xB0; lead <= 0xC8; lead++) {
		unsigned int o = (unsigned int)next;
		memcpy(&t[32 + lead * 4], &o, 4);
		for (int trail = 0xA1; trail <= 0xFE; trail++) { t[next + trail * 8 + 7] = (unsigned char)(2 * step); }
		next += 256 * 8;
	}
	return t;
}

// ---- 기준 구현: 원본 알고리즘 (칸 수 기준, 한글 쌍은 한 글자 = 칸 2개)
struct RefParams { int width, step, flags, align, mode; };

static int refAlign(const RefParams& p, int cols)
{
	int x = 0;
	if (p.mode == 0 || p.mode == 1 || p.mode == 2) {
		int v = p.width - p.step * cols;
		if (p.align == 1) { x = (v - (v >> 31)) >> 1; }
		if (p.align == 2) { x = v; }
	}
	return x;
}

static std::vector<Line> reference(const std::string& s, const RefParams& p)
{
	std::vector<Line> out;
	const bool crlf = (p.flags & 0x40) != 0, wrap = (p.flags & 0x20) != 0, esc = (p.flags & 2) != 0;
	const int maxCols = p.width / p.step;
	const int len = (int)s.size();
	const unsigned char* b = (const unsigned char*)s.c_str();
	int headX = 0;
	if (p.mode == 0 || p.mode == 1 || p.mode == 2) {
		if (p.align == 1) { headX = (p.width - (p.width >> 31)) >> 1; }
		if (p.align == 2) { headX = p.width; }
	}
	Line head; head.offset = -1; head.count = 1; head.x = headX;
	out.push_back(head);

	int ptr = 0, start = 0, cols = 0, idx = 0;
	auto emit = [&](int count) {
		Line l; l.offset = start; l.count = count; l.x = refAlign(p, cols);
		out.push_back(l);
	};
	auto unitAt = [&](int q) -> int { return (b[q] >= 0x80 && b[q + 1] != 0) ? 2 : 1; };

	while (b[ptr] != 0 && idx < len) {
		unsigned char c = b[ptr];
		if (c == 9) {
			ptr++; cols++;
		}
		else if (c == 10) {
			ptr++;
			if (!crlf) { cols++; }
		}
		else if (c == 13) {
			ptr++;
			if (!crlf) {
				cols++;
				if (b[ptr] == 10) { cols++; }
			}
			if (b[ptr] == 10) { ptr++; }
			if (crlf) {
				emit(cols);
				cols = 0;
				if (wrap && b[ptr] == 0x20) { ptr++; }
				start = ptr;
			}
		}
		else if (c == 0x5c) {
			ptr++; cols++;
			if (esc) {
				if (b[ptr] == 0x5c) {
					ptr++; cols++;
				}
				else if (b[ptr] == 0x6e) {
					ptr++; cols--;
					if (!crlf) {
						cols += 2;
					}
					else {
						emit(cols);
						cols = 0;
						if (wrap && b[ptr] == 0x20) { ptr++; }
						start = ptr;
					}
				}
			}
		}
		else if (c == 0x20) {
			ptr++; cols++;
			if (wrap) {
				int q = ptr, k = 0;
				bool fits = true;
				for (;;) {
					if (cols + k > maxCols) { fits = false; break; }
					unsigned char ch = b[q];
					if (ch == 13 || ch == 10 || ch == 0x20 || ch == 0) { break; }
					int u = unitAt(q);
					k += u; q += u;
				}
				if (!fits) {
					emit(cols);
					cols = 0;
					if (b[ptr] == 0x20) { ptr++; }
					start = ptr;
				}
			}
		}
		else {
			int u = unitAt(ptr);
			if (wrap && cols + u > maxCols) {
				emit(cols);
				cols = 0;
				start = ptr;
			}
			ptr += u; cols += u;
		}
		idx++;
	}
	emit(cols);
	return out;
}

// ---- 시험 대상 호출
static std::vector<Line> runLayout(const std::string& s, const RefParams& p, const std::vector<unsigned char>& table, int cellW)
{
	std::vector<unsigned char> rd(0x500, 0);
	std::vector<unsigned char> tbl(0x1000, 0);
	std::string charmap;
	for (int c = 0x20; c < 0x7F; c++) { charmap.push_back((char)c); }
	auto put = [&](int off, int v) { memcpy(&rd[off], &v, 4); };
	auto putp = [&](int off, const void* ptr) { memcpy(&rd[off], &ptr, sizeof(void*)); };
	put(0x60, p.width); put(0x314, 0); put(0x30c, 0);
	put(0x2c4, cellW); put(0x2cc, p.step - cellW); put(0x2c8, 12); put(0x2d8, 800);
	put(0x2e8, p.flags); put(0x31c, p.align); rd[0x32c] = (unsigned char)p.mode;
	putp(0x2ac, s.c_str()); put(0x2b0, (int)s.size());
	putp(0x2f0, &tbl[0]); putp(0x2ec, charmap.c_str());
	putp(0x418, &table[0]);
	gTextBase = (const unsigned char*)s.c_str();
	static const unsigned char emptyString[2] = { 0, 0 };
	gEmpty = emptyString;
	gLines.clear();
	kfont_layout(&rd[0], fakeNewNode, fakeDeleteNode, emptyString);
	// 줄 수 필드 확인
	int lines; memcpy(&lines, &rd[0x308], 4);
	check(lines == (int)gLines.size() - 1, "줄 수(+0x308)가 노드 수와 다름");
	for (size_t i = 0; i < gAllocated.size(); i++) { delete gAllocated[i]; }
	gAllocated.clear();
	return gLines;
}

static unsigned int rng = 12345;
static unsigned int rnd() { rng = rng * 1664525u + 1013904223u; return rng >> 8; }

static std::string randomText(bool withHangul)
{
	static const char* pieces[] = { "a", "b", "W", "i", "m", " ", " ", " ", "  ", ".", ",", "1", "2", "\t", "\r\n", "\r", "\n", "\\n", "\\\\", "\\", "n", "word", "longerword", "Hello", "x" };
	std::string s;
	int n = 1 + rnd() % 40;
	for (int i = 0; i < n; i++) {
		if (withHangul && rnd() % 4 == 0) {
			s.push_back((char)(0xB0 + rnd() % 25));
			s.push_back((char)(0xA1 + rnd() % 94));
		}
		else if (rnd() % 3 == 0) {
			s += pieces[rnd() % (sizeof(pieces) / sizeof(pieces[0]))];
		}
		else {
			s.push_back((char)('a' + rnd() % 26));
		}
	}
	if (withHangul && rnd() % 6 == 0) {
		s.push_back((char)(0xB0 + rnd() % 25));			// 짝이 없는 앞바이트로 끝나는 경우
	}
	return s;
}

int main()
{
	int cases = 0, bad = 0;
	for (int mode = 0; mode < 2; mode++) {
		const bool hangul = (mode == 1);
		for (int iter = 0; iter < 60000; iter++) {
			std::string s = randomText(hangul);
			RefParams p;
			p.step = 4 + rnd() % 8;
			p.width = 20 + rnd() % 200;
			p.flags = ((rnd() % 4 != 0) ? 0x20 : 0) | ((rnd() % 3 != 0) ? 0x40 : 0) | ((rnd() % 2) ? 0x02 : 0) | 0x01;
			p.align = rnd() % 3;
			p.mode = rnd() % 4;
			std::vector<unsigned char> table = makeTable(p.step);
			std::vector<Line> got = runLayout(s, p, table, p.step);
			std::vector<Line> expected = reference(s, p);
			cases++;
			if (got.size() != expected.size() || !std::equal(got.begin(), got.end(), expected.begin())) {
				if (bad < 6) {
					printf("  불일치: 문자열 [%s] 폭 %d 칸 %d 플래그 %x 정렬 %d 모드 %d\n", s.c_str(), p.width, p.step, p.flags, p.align, p.mode);
					printf("    기대:"); for (size_t i = 0; i < expected.size(); i++) { printf(" (%d,%d,%d)", expected[i].offset, expected[i].count, expected[i].x); } printf("\n");
					printf("    실제:"); for (size_t i = 0; i < got.size(); i++) { printf(" (%d,%d,%d)", got[i].offset, got[i].count, got[i].x); } printf("\n");
				}
				bad++;
			}
		}
	}
	printf("무작위 비교 %d건, 불일치 %d건\n", cases, bad);
	check(bad == 0, "칸 폭 글자표에서 원본 알고리즘 결과와 다름");

	// 비례폭: 한글 쌍은 둘로 나뉘지 않고, 줄 폭 합이 폭을 넘지 않는다
	{
		RefParams p; p.step = 5; p.width = 60; p.flags = 0x20 | 0x40; p.align = 0; p.mode = 0;
		std::vector<unsigned char> table = makeTable(5);
		// 한글 쌍 15개 (쌍 하나 = 10px): 폭 60 이면 한 줄에 6개
		std::string s;
		for (int i = 0; i < 15; i++) { s.push_back((char)0xB0); s.push_back((char)(0xA1 + i)); }
		std::vector<Line> got = runLayout(s, p, table, 5);
		bool pairsIntact = true;
		for (size_t i = 1; i < got.size(); i++) { if ((got[i].count % 2) != 0) { pairsIntact = false; } }
		check(pairsIntact, "한글 쌍이 줄 사이에서 끊김");
		check(got.size() == 1 + 3 && got[1].count == 12 && got[2].count == 12 && got[3].count == 6, "한글 15자(쌍 10px) 줄바꿈이 6자씩이 아님");
	}

	if (failureCount == 0) {
		printf("ALL PASSED\n");
		return 0;
	}
	printf("%d FAILURES\n", failureCount);
	return 1;
}
