#pragma once

#include "../Common.h"
#include <memory>

// 폰트 래스터라이저: FreeType (includes/src/freetype-2.13.2)
#include <ft2build.h>
#include FT_FREETYPE_H

namespace CTT {

	namespace Glyph {

		using namespace std;

		// 렌더한 글자 비트맵 (같은 폰트에서 다음 render 를 부르기 전까지만 유효하다)
		struct GlyphBitmap
		{
			int left;					// 펜 위치 기준 비트맵 왼쪽 (FreeType bitmap_left)
			int top;					// 기준선 위로 비트맵 위쪽까지의 높이 (FreeType bitmap_top)
			int width;
			int rows;
			int pitch;
			const unsigned char* buffer;	// 8비트 안티앨리어싱 커버리지
			int advance;				// 다음 글자까지의 픽셀 거리
		};

		// 크기를 정한 TrueType 폰트 하나. Pillow(파이썬 시제품)와 같은 방식으로 글자를 그린다:
		// 내장 비트맵은 쓰지 않고(FT_LOAD_NO_BITMAP), 기본 힌팅 + 8비트 안티앨리어싱
		class FtFont
		{
		public:
			FtFont();
			~FtFont();

			// 폰트 파일을 읽는다 (같은 파일을 여러 크기로 열 때 내용을 공유해도 된다). 실패하면 NULL
			static shared_ptr<vector<unsigned char> > readFile(const wstring& path);

			bool open(FT_Library library, shared_ptr<vector<unsigned char> > data, int pixelSize);

			bool hasChar(wchar_t ch);
			// 글자를 그린다. 폰트에 글자가 없으면 notdef 가 아니라 false
			bool render(wchar_t ch, GlyphBitmap& out);

			int ascent() const { return ascentPx; }
			int descent() const { return descentPx; }
			// 폰트가 가진 한글 음절 (코드 포인트 순). 처음 부를 때 한 번 만든다
			const vector<wchar_t>& hangulSyllables();

		private:
			FtFont(const FtFont&);
			FtFont& operator=(const FtFont&);

			shared_ptr<vector<unsigned char> > data;
			FT_Face face;
			int ascentPx;
			int descentPx;
			vector<wchar_t> syllables;
			bool syllablesBuilt;
		};

	}

}
