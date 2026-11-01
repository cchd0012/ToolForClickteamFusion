#include "FtFont.h"
#include "../Util/CommonUtil.h"

using namespace CTT::Glyph;
using namespace CTT::CommonUtil;


namespace {

	// 26.6 고정소수점을 픽셀로 반올림한다 (Pillow 의 PIXEL 매크로와 같다)
	int pixel26_6(long value) { return (int)((value + 32) >> 6); }

}


FtFont::FtFont() : face(NULL), ascentPx(0), descentPx(0), syllablesBuilt(false)
{
}


FtFont::~FtFont()
{
	if (face != NULL) {
		FT_Done_Face(face);
		face = NULL;
	}
}


shared_ptr<vector<unsigned char> > FtFont::readFile(const wstring& path)
{
	HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 폰트 파일을 열지 못했습니다: " << path << endl;
		return shared_ptr<vector<unsigned char> >();
	}
	shared_ptr<vector<unsigned char> > data(new vector<unsigned char>());
	DWORD size = GetFileSize(hFile, NULL);
	DWORD readBytes = 0;
	data->resize(size);
	bool ok = (size > 0) && (ReadFile(hFile, &(*data)[0], size, &readBytes, NULL) != FALSE) && (readBytes == size);
	CloseHandle(hFile);
	if (!ok) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 폰트 파일을 읽지 못했습니다: " << path << endl;
		return shared_ptr<vector<unsigned char> >();
	}
	return data;
}


bool FtFont::open(FT_Library library, shared_ptr<vector<unsigned char> > fontData, int pixelSize)
{
	if (face != NULL) {
		FT_Done_Face(face);
		face = NULL;
	}
	data = fontData;
	if (!data || data->empty()
		|| FT_New_Memory_Face(library, &(*data)[0], (FT_Long)data->size(), 0, &face) != 0
		|| FT_Set_Pixel_Sizes(face, 0, pixelSize) != 0) {
		if (face != NULL) {
			FT_Done_Face(face);
			face = NULL;
		}
		return false;
	}
	ascentPx = pixel26_6(face->size->metrics.ascender);
	descentPx = pixel26_6(-face->size->metrics.descender);
	syllables.clear();
	syllablesBuilt = false;
	return true;
}


bool FtFont::hasChar(wchar_t ch)
{
	return (face != NULL) && (FT_Get_Char_Index(face, (FT_ULong)ch) != 0);
}


bool FtFont::render(wchar_t ch, GlyphBitmap& out)
{
	if (face == NULL) {
		return false;
	}
	FT_UInt glyphIndex = FT_Get_Char_Index(face, (FT_ULong)ch);
	if (glyphIndex == 0) {
		return false;
	}
	if (FT_Load_Glyph(face, glyphIndex, FT_LOAD_NO_BITMAP) != 0 || FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
		return false;
	}
	FT_GlyphSlot slot = face->glyph;
	out.left = slot->bitmap_left;
	out.top = slot->bitmap_top;
	out.width = (int)slot->bitmap.width;
	out.rows = (int)slot->bitmap.rows;
	out.pitch = slot->bitmap.pitch;
	out.buffer = (slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) ? slot->bitmap.buffer : NULL;
	out.advance = pixel26_6(slot->metrics.horiAdvance);
	return true;
}


const vector<wchar_t>& FtFont::hangulSyllables()
{
	if (!syllablesBuilt) {
		for (UINT32 cp = 0xAC00; cp <= 0xD7A3; cp++) {
			if (hasChar((wchar_t)cp)) {
				syllables.push_back((wchar_t)cp);
			}
		}
		syllablesBuilt = true;
	}
	return syllables;
}
