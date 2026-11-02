#include "FontAtlas.h"
#include "../Util/CommonUtil.h"
#include "../Util/JsonUtil.h"
#include "../Util/PngUtil.h"

#include <algorithm>

#include "FtFont.h"

using namespace CTT::Glyph;
using namespace CTT::CommonUtil;
using namespace CTT::JsonUtil;
using namespace CTT::PngUtil;

// 설정 json (buildFontAtlas). 경로는 설정 파일이 있는 폴더 기준이다
//   {
//     "fontAtlas": {
//       "name": "galmuri9",                 아틀라스 이름 (출력 파일 이름)
//       "font": "fonts/galmuri9.ttf",
//       "size": 9,                          폰트 픽셀 크기
//       "mode": "hangul",                   hangul | extendedHangul
//       "lineHeight": 0,                    0 이면 폰트 지표(ascent + descent)
//       "dy": 0,                            글자를 그릴 세로 보정
//       "threshold": 128,                   0 이면 안티앨리어싱 유지, 1~255 이면 그 이상만 불투명 (기본 128)
//       "color": [255, 255, 255],           글자 색 (기본 흰색)
//       "outlineSize": 0,                   외곽선 두께 (픽셀). 1 이상이면 글자 둘레를 outlineColor 로 두르고 줄 높이가 2 x 두께만큼 늘어난다
//       "outlineColor": [0, 0, 0],
//       "letterSpacing": 0,                 모든 글자의 폭에 더하는 값
//       "spaceWidth": 0,                    0 이면 폰트 값
//       "missingAdvance": 0,                글자표에 없는 코드의 폭. 0 이면 공백 폭
//       "advanceOverrides": { "I": 3 },     글자별 폭 지정
//       "atlasWidth": 2048,
//       "outputDir": "out"
//     }
//   }

namespace {

	// 한 글자의 코드(CP949 1~2바이트)를 유니코드로 바꾼다. 쓸 수 없는 코드면 false
	bool codeToWide(UINT16 code, wchar_t& out)
	{
		char bytes[2];
		int length = 0;
		if (code < 0x100) {
			bytes[0] = (char)code;
			length = 1;
		}
		else {
			bytes[0] = (char)(code >> 8);
			bytes[1] = (char)(code & 0xFF);
			length = 2;
		}
		wchar_t wide[2] = { 0, 0 };
		if (MultiByteToWideChar(949, MB_ERR_INVALID_CHARS, bytes, length, wide, 2) != 1) {
			return false;
		}
		if (wide[0] < 0x20 || (wide[0] >= 0xE000 && wide[0] <= 0xF8FF) || wide[0] == 0xFFFD) {
			return false;
		}
		out = wide[0];
		return true;
	}


	// 모드가 다루는 2바이트 영역인지
	bool inMode(FontAtlasMode mode, BYTE lead, BYTE trail)
	{
		if (lead < 0x81 || lead > 0xFE) {
			return false;
		}
		if (mode == FontAtlasMode::Hangul) {
			return (lead >= 0xA1) && (trail >= 0xA1) && (trail <= 0xFE);
		}
		return ((trail >= 0x41) && (trail <= 0x5A)) || ((trail >= 0x61) && (trail <= 0x7A)) || ((trail >= 0x81) && (trail <= 0xFE));
	}


	string utf8(const wstring& s) { return getAnsiStr(s, CodePage::CP_UTF_8); }
	wstring fromUtf8(const string& s) { return getUniStr(s, CodePage::CP_UTF_8); }


	bool writeFile(const wstring& path, const void* data, size_t size)
	{
		HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file == INVALID_HANDLE_VALUE) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 파일을 만들지 못했습니다: " << path << endl;
			return false;
		}
		DWORD written = 0;
		bool ok = (WriteFile(file, data, (DWORD)size, &written, NULL) != FALSE) && (written == size);
		CloseHandle(file);
		if (!ok) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 파일을 쓰지 못했습니다: " << path << endl;
		}
		return ok;
	}


	// 상대 경로는 기준 폴더에 붙여서 돌려준다
	wstring resolvePath(const wstring& baseDir, const wstring& path)
	{
		if (path.size() >= 2 && (path[1] == L':' || path[0] == L'/' || path[0] == L'\\')) {
			return path;
		}
		return baseDir + L"/" + path;
	}


	// 글자표의 위치가 PNG 안에 들어가는지
	bool glyphFitsAtlas(const FontAtlasGlyph& g, UINT32 atlasWidth, UINT32 atlasHeight)
	{
		return ((UINT32)g.x + g.width <= atlasWidth) && ((UINT32)g.y + g.height <= atlasHeight);
	}

}


bool CTT::Glyph::serializeFontAtlasTable(const FontAtlasData& atlas, vector<BYTE>& out)
{
	// 글자가 있는 페이지(상위 바이트)를 찾는다
	bool hasPage[256] = { false };
	for (map<UINT16, FontAtlasGlyph>::const_iterator it = atlas.glyphs.begin(); it != atlas.glyphs.end(); ++it) {
		hasPage[it->first >> 8] = true;
	}

	size_t pageCount = 0;
	for (int i = 0; i < 256; i++) {
		if (hasPage[i]) {
			pageCount++;
		}
	}

	const size_t pageListOffset = kFontAtlasHeaderSize;
	const size_t pagesOffset = pageListOffset + 256 * 4;
	out.assign(pagesOffset + pageCount * 256 * 8, 0);

	UINT32* header = (UINT32*)&out[0];
	header[0] = kFontAtlasMagic;
	UINT16* header16 = (UINT16*)&out[4];
	header16[0] = 1;									// 버전
	header16[1] = atlas.flags;
	header16[2] = atlas.lineHeight;
	header16[3] = atlas.atlasWidth;
	header16[4] = atlas.atlasHeight;
	header16[5] = atlas.missingAdvance;
	header[4] = (UINT32)atlas.glyphs.size();

	UINT32* pageList = (UINT32*)&out[pageListOffset];
	size_t nextPage = pagesOffset;
	for (int i = 0; i < 256; i++) {
		if (hasPage[i]) {
			pageList[i] = (UINT32)nextPage;
			nextPage += 256 * 8;
		}
	}

	for (map<UINT16, FontAtlasGlyph>::const_iterator it = atlas.glyphs.begin(); it != atlas.glyphs.end(); ++it) {
		const FontAtlasGlyph& g = it->second;
		if (g.width == 0 && g.advance == 0) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 코드 0x" << hex << it->first << dec << L" 의 글자가 비어 있습니다 (폭과 advance 가 모두 0)." << endl;
			return false;
		}
		BYTE* record = &out[pageList[it->first >> 8] + (it->first & 0xFF) * 8];
		*(UINT16*)(record + 0) = g.x;
		*(UINT16*)(record + 2) = g.y;
		record[4] = g.width;
		record[5] = g.height;
		record[6] = (BYTE)g.offsetX;
		record[7] = g.advance;
	}
	return true;
}


bool CTT::Glyph::lookupFontAtlasGlyph(const BYTE* data, size_t size, UINT16 code, FontAtlasGlyph& glyph)
{
	if (size < kFontAtlasHeaderSize + 256 * 4 || *(const UINT32*)data != kFontAtlasMagic) {
		return false;
	}
	UINT32 page = *(const UINT32*)(data + kFontAtlasHeaderSize + (code >> 8) * 4);
	if (page == 0 || (size_t)page + 256 * 8 > size) {
		return false;
	}
	const BYTE* record = data + page + (code & 0xFF) * 8;
	glyph.x = *(const UINT16*)(record + 0);
	glyph.y = *(const UINT16*)(record + 2);
	glyph.width = record[4];
	glyph.height = record[5];
	glyph.offsetX = (INT8)record[6];
	glyph.advance = record[7];
	return (glyph.width != 0) || (glyph.advance != 0);
}


bool CTT::Glyph::parseFontAtlasTable(const BYTE* data, size_t size, FontAtlasData& atlas)
{
	if (size < kFontAtlasHeaderSize + 256 * 4 || *(const UINT32*)data != kFontAtlasMagic) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": KFT1 글자표가 아닙니다." << endl;
		return false;
	}
	const UINT16* header16 = (const UINT16*)(data + 4);
	if (header16[0] != 1) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 지원하지 않는 글자표 버전입니다: " << dec << header16[0] << endl;
		return false;
	}
	atlas.flags = header16[1];
	atlas.lineHeight = header16[2];
	atlas.atlasWidth = header16[3];
	atlas.atlasHeight = header16[4];
	atlas.missingAdvance = header16[5];
	atlas.glyphs.clear();
	for (UINT32 code = 0; code < 0x10000; code++) {
		FontAtlasGlyph g;
		if (lookupFontAtlasGlyph(data, size, (UINT16)code, g)) {
			atlas.glyphs[(UINT16)code] = g;
		}
	}
	const UINT32 expected = *(const UINT32*)(data + 16);
	if (expected != atlas.glyphs.size()) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 글자 수가 헤더와 다릅니다: " << dec << expected << L" / " << atlas.glyphs.size() << endl;
		return false;
	}
	return true;
}


bool CTT::Glyph::saveFontAtlas(const FontAtlasData& atlas, const wstring& directory)
{
	if (!createDirectoryRecursively(directory)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 폴더를 만들지 못했습니다: " << directory << endl;
		return false;
	}

	vector<BYTE> table;
	if (!serializeFontAtlasTable(atlas, table)) {
		return false;
	}
	if (!writeFile(directory + L"/" + atlas.name + L".kft", &table[0], table.size())) {
		return false;
	}

	if (!atlas.bgra.empty()) {
		vector<BYTE> pixels(atlas.bgra);
		BYTE* buffer = &pixels[0];
		UINT32 w = atlas.atlasWidth, h = atlas.atlasHeight, bitCount = 32;
		if (!writeToPng(buffer, directory + L"/" + atlas.name + L".png", w, h, bitCount)) {
			return false;
		}
	}

	Json::Value meta(Json::objectValue);
	meta["type"] = "fontAtlas";
	meta["version"] = 1;
	meta["name"] = utf8(atlas.name);
	meta["lineHeight"] = (Json::UInt)atlas.lineHeight;
	meta["atlasWidth"] = (Json::UInt)atlas.atlasWidth;
	meta["atlasHeight"] = (Json::UInt)atlas.atlasHeight;
	meta["missingAdvance"] = (Json::UInt)atlas.missingAdvance;
	meta["glyphCount"] = (Json::UInt)atlas.glyphs.size();
	meta["flags"] = (Json::UInt)atlas.flags;
	wstring metaPath = directory + L"/" + atlas.name + L".json";
	return writeIntoJson(meta, metaPath);
}


bool CTT::Glyph::loadFontAtlas(FontAtlasData& atlas, const wstring& directory, const wstring& name)
{
	shared_ptr<vector<unsigned char> > table = FtFont::readFile(directory + L"/" + name + L".kft");
	if (!table || !parseFontAtlasTable(&(*table)[0], table->size(), atlas)) {
		return false;
	}
	atlas.name = name;

	wstring pngPath = directory + L"/" + name + L".png";
	if (isFileOrDir(pngPath) == FSOType::FSO_FILE) {
		BYTE* buffer = NULL;
		UINT32 w = 0, h = 0, bitCount = 0;
		if (!readFromPng(buffer, pngPath, w, h, bitCount) || bitCount != 32) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 아틀라스 PNG(32비트)를 읽지 못했습니다: " << pngPath << endl;
			if (buffer != NULL) { free(buffer); }
			return false;
		}
		atlas.bgra.assign(buffer, buffer + (size_t)w * h * 4);
		free(buffer);
		if (w != atlas.atlasWidth || h != atlas.atlasHeight) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": PNG 크기가 글자표와 다릅니다: " << dec << w << L"x" << h << endl;
			return false;
		}
	}
	return true;
}


bool CTT::Glyph::buildFontAtlas(const wstring& configPath)
{
	Json::Value root;
	wstring configFile = configPath;
	if (!readFromJson(root, configFile)) {
		return false;
	}
	const Json::Value& c = root["fontAtlas"];
	if (!c.isObject() || !c["name"].isString() || !c["font"].isString() || !c["size"].isInt() || !c["outputDir"].isString()) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 설정에 fontAtlas 블록(name, font, size, outputDir)이 필요합니다." << endl;
		return false;
	}

	const wstring baseDir = getOnlyDirPath(configFile);
	FontAtlasMode mode = FontAtlasMode::Hangul;
	const string modeText = c.get("mode", "hangul").asString();
	if (modeText == "extendedHangul") {
		mode = FontAtlasMode::ExtendedHangul;
	}
	else if (modeText != "hangul") {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": mode 는 hangul 또는 extendedHangul 이어야 합니다." << endl;
		return false;
	}

	FtLibrary freeType;					// font 보다 먼저 선언해서 나중에 해제한다
	FtFont font;
	if (!freeType.isOpen()) {
		return false;
	}
	bool opened = font.open(freeType.get(), FtFont::readFile(resolvePath(baseDir, fromUtf8(c["font"].asString()))), c["size"].asInt());
	if (!opened) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 폰트를 열지 못했습니다." << endl;
		return false;
	}

	FontAtlasData atlas;
	atlas.name = fromUtf8(c["name"].asString());
	atlas.flags = (mode == FontAtlasMode::ExtendedHangul) ? 1 : 0;
	const int ascent = font.ascent();
	const int lineHeight = c.get("lineHeight", 0).asInt() > 0 ? c["lineHeight"].asInt() : (ascent + font.descent());
	const int dy = c.get("dy", 0).asInt();
	const int threshold = c.get("threshold", 128).asInt();
	const int outlineSize = c.get("outlineSize", 0).asInt();
	BYTE fillColor[3] = { 255, 255, 255 }, outlineColor[3] = { 0, 0, 0 };		// B, G, R
	for (int k = 0; k < 3; k++) {
		if (c["color"].isArray() && c["color"].size() == 3) {
			fillColor[2 - k] = (BYTE)c["color"][k].asInt();
		}
		if (c["outlineColor"].isArray() && c["outlineColor"].size() == 3) {
			outlineColor[2 - k] = (BYTE)c["outlineColor"][k].asInt();
		}
	}
	// 외곽선이 있으면 글자 칸의 높이가 늘어난다
	const int cellHeight = lineHeight + 2 * outlineSize;
	const int letterSpacing = c.get("letterSpacing", 0).asInt();
	const int atlasWidth = c.get("atlasWidth", 2048).asInt();
	if (lineHeight <= 0 || cellHeight > 255 || outlineSize < 0 || outlineSize > 8 || atlasWidth < 64 || atlasWidth > 0xFFFF) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 줄 높이(1~255) 또는 아틀라스 폭이 올바르지 않습니다: " << dec << lineHeight << L", " << atlasWidth << endl;
		return false;
	}

	// 글자별 폭 지정 (키는 UTF-8 한 글자)
	map<wchar_t, int> advanceOverride;
	const Json::Value& overrides = c["advanceOverrides"];
	if (overrides.isObject()) {
		for (Json::Value::const_iterator it = overrides.begin(); it != overrides.end(); ++it) {
			wstring key = fromUtf8(it.name());
			if (key.size() == 1 && it->isInt()) {
				advanceOverride[key[0]] = it->asInt();
			}
		}
	}

	// 글자 그리기: 코드 순서대로 (아스키 -> CP949 2바이트)
	struct Cell {
		UINT16 code;
		int left;
		int width;
		int advance;
		vector<BYTE> bgra;			// width x cellHeight x 4 (B, G, R, A)
	};
	vector<Cell> cells;
	size_t missingInFont = 0;
	size_t blankGlyphs = 0;
	wstring blankSamples;

	vector<UINT16> codes;
	for (UINT32 b = 0x20; b <= 0x7E; b++) {
		codes.push_back((UINT16)b);
	}
	for (UINT32 lead = 0x81; lead <= 0xFE; lead++) {
		for (UINT32 trail = 0x41; trail <= 0xFE; trail++) {
			if (inMode(mode, (BYTE)lead, (BYTE)trail)) {
				codes.push_back((UINT16)((lead << 8) | trail));
			}
		}
	}

	for (size_t i = 0; i < codes.size(); i++) {
		wchar_t wide = 0;
		if (!codeToWide(codes[i], wide)) {
			continue;
		}
		GlyphBitmap bitmap;
		if (!font.hasChar(wide) || !font.render(wide, bitmap)) {
			missingInFont++;
			continue;
		}

		int advance = bitmap.advance + letterSpacing;
		map<wchar_t, int>::const_iterator ov = advanceOverride.find(wide);
		if (ov != advanceOverride.end()) {
			advance = ov->second;
		}
		else if (wide == L' ' && c.get("spaceWidth", 0).asInt() > 0) {
			advance = c["spaceWidth"].asInt();
		}
		advance = (std::max)(0, (std::min)(255, advance));

		// 공백이 아닌데 그려진 픽셀이 없는 글자는 폰트가 그 크기에서 글리프를 비워 둔 것이다 (경고용으로 센다)
		if ((bitmap.buffer == NULL || bitmap.width <= 0 || bitmap.rows <= 0) && wide != L' ' && wide != 0x00A0 && wide != 0x3000) {
			blankGlyphs++;
			if (blankSamples.size() < 8) {
				blankSamples.push_back(wide);
			}
		}

		Cell cell;
		cell.code = codes[i];
		cell.advance = advance;
		cell.left = 0;
		cell.width = 0;
		if (bitmap.buffer != NULL && bitmap.width > 0 && bitmap.rows > 0) {
			// 글자만 그린 알파 (폭 x 줄 높이)
			vector<BYTE> alpha((size_t)bitmap.width * lineHeight, 0);
			const int originY = dy + ascent - bitmap.top;
			for (int gy = 0; gy < bitmap.rows; gy++) {
				const int py = originY + gy;
				if (py < 0 || py >= lineHeight) {
					continue;
				}
				for (int gx = 0; gx < bitmap.width; gx++) {
					int v = bitmap.buffer[gy * bitmap.pitch + gx];
					if (threshold > 0) {
						v = (v >= threshold) ? 255 : 0;
					}
					alpha[(size_t)py * bitmap.width + gx] = (BYTE)v;
				}
			}
			// 칸: 글자 둘레 outlineSize 픽셀을 더하고, 글자 픽셀에서 가까운 곳을 외곽선 색으로 채운다
			const int pw = bitmap.width + 2 * outlineSize;
			cell.left = bitmap.left - outlineSize;
			cell.width = pw;
			cell.bgra.assign((size_t)pw * cellHeight * 4, 0);
			for (int py = 0; py < cellHeight; py++) {
				for (int px = 0; px < pw; px++) {
					const int gx = px - outlineSize, gy = py - outlineSize;
					BYTE* out = &cell.bgra[((size_t)py * pw + px) * 4];
					const int own = (gx >= 0 && gx < bitmap.width && gy >= 0 && gy < lineHeight) ? alpha[(size_t)gy * bitmap.width + gx] : 0;
					if (own > 0) {
						out[0] = fillColor[0]; out[1] = fillColor[1]; out[2] = fillColor[2]; out[3] = (BYTE)own;
						continue;
					}
					bool nearInk = false;
					for (int oy = -outlineSize; oy <= outlineSize && !nearInk; oy++) {
						for (int ox = -outlineSize; ox <= outlineSize && !nearInk; ox++) {
							const int ax = gx + ox, ay = gy + oy;
							if (ax >= 0 && ax < bitmap.width && ay >= 0 && ay < lineHeight && alpha[(size_t)ay * bitmap.width + ax] > 0) {
								nearInk = true;
							}
						}
					}
					if (nearInk && outlineSize > 0) {
						out[0] = outlineColor[0]; out[1] = outlineColor[1]; out[2] = outlineColor[2]; out[3] = 255;
					}
				}
			}
		}
		if (cell.width > 255 || cell.left < -128 || cell.left > 127) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 글자 0x" << hex << codes[i] << dec << L" 의 크기/보정이 형식 범위를 넘습니다." << endl;
			return false;
		}
		cells.push_back(cell);
	}

	// 줄 단위로 채워 넣는다 (글자 사이 1픽셀 간격)
	int x = 0, y = 0;
	vector<FontAtlasGlyph> placed(cells.size());
	for (size_t i = 0; i < cells.size(); i++) {
		FontAtlasGlyph g;
		memset(&g, 0, sizeof(g));
		g.advance = (BYTE)cells[i].advance;
		if (cells[i].width > 0) {
			if (x + cells[i].width > atlasWidth) {
				x = 0;
				y += cellHeight + 1;
			}
			g.x = (UINT16)x;
			g.y = (UINT16)y;
			g.width = (BYTE)cells[i].width;
			g.height = (BYTE)cellHeight;
			g.offsetX = (INT8)cells[i].left;
			x += cells[i].width + 1;
		}
		placed[i] = g;
	}
	const int atlasHeight = y + cellHeight;
	if (atlasHeight > 4096) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 아틀라스 높이가 4096 을 넘습니다 (" << dec << atlasHeight << L"). atlasWidth 를 늘리세요." << endl;
		return false;
	}

	atlas.lineHeight = (UINT16)cellHeight;
	atlas.atlasWidth = (UINT16)atlasWidth;
	atlas.atlasHeight = (UINT16)atlasHeight;
	atlas.bgra.assign((size_t)atlasWidth * atlasHeight * 4, 0);
	for (size_t i = 0; i < cells.size(); i++) {
		atlas.glyphs[cells[i].code] = placed[i];
		for (int row = 0; row < cellHeight && cells[i].width > 0; row++) {
			for (int col = 0; col < cells[i].width; col++) {
				memcpy(&atlas.bgra[((size_t)(placed[i].y + row) * atlasWidth + placed[i].x + col) * 4], &cells[i].bgra[((size_t)row * cells[i].width + col) * 4], 4);
			}
		}
	}

	// 글자표에 없는 코드의 폭
	map<UINT16, FontAtlasGlyph>::const_iterator space = atlas.glyphs.find(0x20);
	int missing = c.get("missingAdvance", 0).asInt();
	if (missing <= 0) {
		missing = (space != atlas.glyphs.end()) ? space->second.advance : c["size"].asInt() / 2;
	}
	atlas.missingAdvance = (UINT16)(std::max)(1, missing);


	if (!saveFontAtlas(atlas, resolvePath(baseDir, fromUtf8(c["outputDir"].asString())))) {
		return false;
	}
	wcout << infoPrefix << atlas.name << L": 글자 " << dec << atlas.glyphs.size() << L"개, 줄 높이 " << cellHeight << L", 아틀라스 " << atlasWidth << L"x" << atlasHeight << L" (폰트에 없던 코드 " << missingInFont << L"개)" << endl;
	if (blankGlyphs > 0) {
		wcout << infoPrefix << L"주의: 폰트가 이 크기에서 그림을 비워 둔 글자가 " << dec << blankGlyphs << L"개 있습니다 (빈 칸으로 출력됩니다). 예: " << blankSamples << L" ... 크기를 바꾸거나 다른 폰트를 쓰세요." << endl;
	}
	return true;
}


namespace {

	// 레거시 FONT_INFO 의 글자 하나 (txtblt_fix_kor.dll 의 TC_IDX)
#pragma pack(push, 1)
	struct LegacyRecord
	{
		UINT16 code;
		BYTE advance;
		BYTE unknown;
		UINT32 xStart;
		UINT32 yStart;
		UINT32 xEnd;
		UINT32 yEnd;
		INT32 xDiff;
		INT32 yDiff;
	};
#pragma pack(pop)


	// "경로|리소스형식" 이면 DLL 의 리소스를, 아니면 리소스 내용만 담은 파일을 읽는다
	bool readLegacyResource(const wstring& spec, vector<BYTE>& out)
	{
		size_t bar = spec.find(L'|');
		if (bar == wstring::npos) {
			shared_ptr<vector<unsigned char> > data = FtFont::readFile(spec);
			if (!data) {
				return false;
			}
			out.assign(data->begin(), data->end());
			return true;
		}

		wstring dllPath = spec.substr(0, bar);
		wstring typeName = spec.substr(bar + 1);
		HMODULE module = LoadLibraryExW(dllPath.c_str(), NULL, LOAD_LIBRARY_AS_DATAFILE);
		if (module == NULL) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": DLL 을 열지 못했습니다: " << dllPath << endl;
			return false;
		}

		struct Finder {
			static BOOL CALLBACK callback(HMODULE, LPCWSTR, LPWSTR name, LONG_PTR param)
			{
				*(LPWSTR*)param = name;
				return FALSE;
			}
		};
		LPWSTR resourceName = NULL;
		EnumResourceNamesW(module, typeName.c_str(), (ENUMRESNAMEPROCW)Finder::callback, (LONG_PTR)&resourceName);
		HRSRC resource = (resourceName != NULL) ? FindResourceW(module, resourceName, typeName.c_str()) : NULL;
		HGLOBAL loaded = (resource != NULL) ? LoadResource(module, resource) : NULL;
		const BYTE* bytes = (loaded != NULL) ? (const BYTE*)LockResource(loaded) : NULL;
		if (bytes == NULL) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 리소스를 찾지 못했습니다: " << typeName << endl;
			FreeLibrary(module);
			return false;
		}
		out.assign(bytes, bytes + SizeofResource(module, resource));
		FreeLibrary(module);
		return true;
	}

}


namespace {

	// BMFont 텍스트 .fnt 의 "char id=.. x=.. y=.." 줄에서 (x, y) -> 유니코드 번호를 읽는다
	bool readBmFontIds(const wstring& fntPath, map<UINT64, UINT32>& ids)
	{
		shared_ptr<vector<unsigned char> > data = FtFont::readFile(fntPath);
		if (!data) {
			return false;
		}
		string text((const char*)&(*data)[0], data->size());
		size_t pos = 0;
		while ((pos = text.find("char id=", pos)) != string::npos) {
			size_t end = text.find('\n', pos);
			string line = text.substr(pos, (end == string::npos) ? string::npos : end - pos);
			pos = (end == string::npos) ? text.size() : end;
			int id = -1, x = -1, y = -1;
			size_t p1 = line.find("id="), p2 = line.find(" x="), p3 = line.find(" y=");
			if (p1 == string::npos || p2 == string::npos || p3 == string::npos) {
				continue;
			}
			id = atoi(line.c_str() + p1 + 3);
			x = atoi(line.c_str() + p2 + 3);
			y = atoi(line.c_str() + p3 + 3);
			if (id >= 0 && x >= 0 && y >= 0) {
				ids[((UINT64)(UINT32)x << 32) | (UINT32)y] = (UINT32)id;
			}
		}
		if (ids.empty()) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": .fnt 에서 글자를 읽지 못했습니다 (텍스트 형식이어야 합니다): " << fntPath << endl;
			return false;
		}
		return true;
	}

}


bool CTT::Glyph::importLegacyFontInfo(const wstring& resourcePath, const wstring& pngPath, const wstring& name, const wstring& directory, bool lastWins, const wstring& fntPath)
{
	vector<BYTE> res;
	if (!readLegacyResource(resourcePath, res)) {
		return false;
	}

	// 헤더: u32 길이 + 이름, u32 폭, u32 높이, u32 길이 + 이미지 파일 이름, u32 글자 수
	size_t p = 0;
	UINT32 height = 0, count = 0;
	auto readU32 = [&](UINT32& v) -> bool {
		if (p + 4 > res.size()) { return false; }
		v = *(UINT32*)&res[p];
		p += 4;
		return true;
	};
	UINT32 length = 0, widthField = 0;
	if (!readU32(length) || p + length > res.size()) { return false; }
	p += length;
	if (!readU32(widthField) || !readU32(height) || !readU32(length) || p + length > res.size()) { return false; }
	p += length;
	if (!readU32(count) || p + (size_t)count * sizeof(LegacyRecord) != res.size()) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": FONT_INFO 크기가 글자 수와 맞지 않습니다." << endl;
		return false;
	}

	FontAtlasData atlas;
	atlas.name = name;
	atlas.lineHeight = (UINT16)height;
	wstring pngPathCopy = pngPath;
	if (isFileOrDir(pngPathCopy) == FSOType::FSO_FILE) {
		BYTE* buffer = NULL;
		UINT32 w = 0, h = 0, bitCount = 0;
		if (!readFromPng(buffer, pngPath, w, h, bitCount) || bitCount != 32 || w > 0xFFFF || h > 0xFFFF) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 글자 이미지(32비트 PNG)를 읽지 못했습니다: " << pngPath << endl;
			if (buffer != NULL) { free(buffer); }
			return false;
		}
		atlas.bgra.assign(buffer, buffer + (size_t)w * h * 4);
		free(buffer);
		atlas.atlasWidth = (UINT16)w;
		atlas.atlasHeight = (UINT16)h;
	}

	const LegacyRecord* records = (const LegacyRecord*)&res[p];
	UINT32 maxX = 0, maxY = 0;
	size_t duplicated = 0, oversize = 0;

	// 같은 코드가 여러 번 나오는 경우(악센트 글자를 아스키로 바꾼 흔적)에는 어느 항목을 쓸지 정한다.
	// 원본 DLL 은 마지막 항목이 남아서 'A' 가 'À' 로 그려졌다. 기본은 첫 항목(일반 글자)이다
	// BMFont .fnt 가 주어지면 (x, y) -> 유니코드를 알 수 있어서, 코드가 가리키는 원래 글자의 항목을 고른다
	map<UINT64, UINT32> fntId;
	if (!fntPath.empty() && !readBmFontIds(fntPath, fntId)) {
		return false;
	}
	map<UINT16, vector<UINT32> > candidates;
	for (UINT32 i = 0; i < count; i++) {
		candidates[records[i].code].push_back(i);
	}
	map<UINT16, UINT32> winner;
	size_t resolvedByFnt = 0;
	for (map<UINT16, vector<UINT32> >::const_iterator c = candidates.begin(); c != candidates.end(); ++c) {
		const vector<UINT32>& list = c->second;
		UINT32 chosen = lastWins ? list.back() : list.front();
		if (list.size() > 1) {
			duplicated += list.size() - 1;
			wchar_t wanted = 0;
			if (!fntId.empty() && codeToWide(c->first, wanted)) {
				for (size_t k = 0; k < list.size(); k++) {
					const LegacyRecord& r = records[list[k]];
					map<UINT64, UINT32>::const_iterator f = fntId.find(((UINT64)r.xStart << 32) | r.yStart);
					if (f != fntId.end() && f->second == (UINT32)wanted) {
						chosen = list[k];
						resolvedByFnt++;
						break;
					}
				}
			}
		}
		winner[c->first] = chosen;
	}
	auto usable = [](const LegacyRecord& r) -> bool {
		return !(r.xEnd < r.xStart || r.yEnd < r.yStart || r.xEnd - r.xStart > 255 || r.yEnd - r.yStart > 255 || r.xEnd > 0xFFFF || r.yEnd > 0xFFFF);
	};
	for (map<UINT16, UINT32>::const_iterator w = winner.begin(); w != winner.end(); ++w) {
		const LegacyRecord& r = records[w->second];
		if (!usable(r)) {
			oversize++;
			continue;
		}
		FontAtlasGlyph g;
		g.x = (UINT16)r.xStart;
		g.y = (UINT16)r.yStart;
		g.width = (BYTE)(r.xEnd - r.xStart);
		g.height = (BYTE)(r.yEnd - r.yStart);
		g.offsetX = 0;
		g.advance = r.advance;
		if (g.width == 0 && g.advance == 0) {
			continue;
		}
		atlas.glyphs[r.code] = g;
		maxX = (std::max)(maxX, (UINT32)g.x + g.width);
		maxY = (std::max)(maxY, (UINT32)g.y + g.height);
	}
	if (atlas.bgra.empty()) {
		atlas.atlasWidth = (UINT16)maxX;
		atlas.atlasHeight = (UINT16)maxY;
	}
	map<UINT16, FontAtlasGlyph>::const_iterator space = atlas.glyphs.find(0x20);
	atlas.missingAdvance = (space != atlas.glyphs.end() && space->second.advance > 0) ? space->second.advance : 4;

	// 이미지 범위를 벗어나는 글자 확인
	size_t outside = 0;
	for (map<UINT16, FontAtlasGlyph>::const_iterator it = atlas.glyphs.begin(); it != atlas.glyphs.end(); ++it) {
		if (!glyphFitsAtlas(it->second, atlas.atlasWidth, atlas.atlasHeight)) {
			outside++;
		}
	}

	// 직렬화한 글자표에서 다시 찾아 원본과 비교한다
	vector<BYTE> table;
	if (!serializeFontAtlasTable(atlas, table)) {
		return false;
	}
	size_t mismatched = 0;
	for (map<UINT16, UINT32>::const_iterator w = winner.begin(); w != winner.end(); ++w) {
		const LegacyRecord& r = records[w->second];
		FontAtlasGlyph g;
		bool found = lookupFontAtlasGlyph(&table[0], table.size(), r.code, g);
		bool expectPresent = usable(r) && !((r.xEnd - r.xStart) == 0 && r.advance == 0);
		if (found != expectPresent) {
			mismatched++;
		}
		else if (found && (g.x != r.xStart || g.y != r.yStart || g.width != r.xEnd - r.xStart || g.height != r.yEnd - r.yStart || g.advance != r.advance)) {
			mismatched++;
		}
	}

	if (!saveFontAtlas(atlas, directory)) {
		return false;
	}
	wcout << infoPrefix << name << L": 레거시 글자 " << dec << count << L"개 -> 글자표 " << atlas.glyphs.size() << L"개 (중복 항목 " << duplicated << L", 그중 .fnt 로 원래 글자를 고른 코드 " << resolvedByFnt << L", 형식 범위 초과 " << oversize << L", 이미지 밖 " << outside << L", 조회 불일치 " << mismatched << L")" << endl;
	return mismatched == 0;
}


int CTT::Glyph::runFontAtlasCommand(int argc, WCHAR** argv)
{
	// argv[0] = exe, argv[1] = --font-atlas, argv[2] = 하위 명령
	if (argc >= 4 && wcscmp(argv[2], L"build") == 0) {
		return buildFontAtlas(argv[3]) ? 0 : 1;
	}
	if (argc >= 7 && wcscmp(argv[2], L"legacy") == 0) {
		// PNG 인자가 "-" 이면 이미지 없이 글자표만 만든다
		wstring pngPath = (wcscmp(argv[4], L"-") == 0) ? wstring() : wstring(argv[4]);
		bool lastWins = false;
		wstring fntPath;
		for (int i = 7; i < argc; i++) {
			wstring option = argv[i];
			if (option == L"last") {
				lastWins = true;
			}
			else {
				fntPath = option;
			}
		}
		return importLegacyFontInfo(argv[3], pngPath, argv[5], argv[6], lastWins, fntPath) ? 0 : 1;
	}
	if (argc >= 5 && wcscmp(argv[2], L"check") == 0) {
		FontAtlasData atlas;
		if (!loadFontAtlas(atlas, argv[3], argv[4])) {
			return 1;
		}
		shared_ptr<vector<unsigned char> > table = FtFont::readFile(wstring(argv[3]) + L"/" + argv[4] + L".kft");
		size_t bad = 0;
		for (UINT32 code = 0; code < 0x10000; code++) {
			FontAtlasGlyph g;
			bool found = lookupFontAtlasGlyph(&(*table)[0], table->size(), (UINT16)code, g);
			map<UINT16, FontAtlasGlyph>::const_iterator it = atlas.glyphs.find((UINT16)code);
			if (found != (it != atlas.glyphs.end())) {
				bad++;
			}
			else if (found && (g.x != it->second.x || g.y != it->second.y || g.width != it->second.width || g.height != it->second.height || g.offsetX != it->second.offsetX || g.advance != it->second.advance)) {
				bad++;
			}
			else if (found && !atlas.bgra.empty() && !glyphFitsAtlas(g, atlas.atlasWidth, atlas.atlasHeight)) {
				bad++;
			}
		}
		wcout << infoPrefix << argv[4] << L": 글자 " << dec << atlas.glyphs.size() << L"개, 불일치 " << bad << L"개" << endl;
		return (bad == 0) ? 0 : 1;
	}

	wcout << L"사용법:" << endl;
	wcout << L"  --font-atlas build <설정 json>" << endl;
	wcout << L"  --font-atlas legacy <FONT_INFO 파일 또는 DLL경로|리소스형식> <글자 이미지 PNG 또는 -> <이름> <출력 폴더> [last]" << endl;
	wcout << L"      같은 코드가 여러 번 나오면 BMFont .fnt 경로를 주었을 때는 원래 글자의 항목을, 아니면 첫 항목을 쓴다 (last 는 원본 DLL 처럼 마지막 항목)" << endl;
	wcout << L"  --font-atlas check <폴더> <이름>" << endl;
	return 1;
}
