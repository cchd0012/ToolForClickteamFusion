#include "GlyphGenerator.h"
#include "../Util/CommonUtil.h"
#include "../Util/JsonUtil.h"
#include "../Util/PngUtil.h"

#include <map>
#include <set>

#include "FtFont.h"

using namespace CTT::Glyph;
using namespace CTT::CommonUtil;
using namespace CTT::JsonUtil;
using namespace CTT::PngUtil;

namespace {

	// 한글 음절 / 호환 자모 범위 (스타일의 hangulPad 를 더하는 글자)
	bool isHangul(wchar_t ch)
	{
		return ((ch >= 0xAC00) && (ch <= 0xD7A3)) || ((ch >= 0x3130) && (ch <= 0x318F));
	}

	int floorDiv(int a, int b)
	{
		int q = a / b;
		if (((a % b) != 0) && ((a < 0) != (b < 0))) {
			q--;
		}
		return q;
	}

	string utf8(const wstring& s) { return getAnsiStr(s, CodePage::CP_UTF_8); }
	wstring fromUtf8(const string& s) { return getUniStr(s, CodePage::CP_UTF_8); }


	// 하나의 글자 스타일: 폰트, 크기, 셀 높이 등
	struct Style
	{
		wstring name;
		FtFont font;
		int size;
		int cellHeight;
		bool hasDy;
		int dy;
		int dx;
		int threshold;
		int hangulPad;
		bool regenerateExisting;
		bool substituteHangulHash;
	};


	class GlyphGenerator
	{
	public:
		GlyphGenerator(const wstring& gameDir) : gameDir(gameDir), library(NULL), nextHandle(0) {}
		~GlyphGenerator();

		bool run(const wstring& configPath);

	private:
		wstring gameDir;
		Json::Value config;

		FT_Library library;
		vector<Style*> styles;
		map<wstring, shared_ptr<vector<unsigned char> > > fontData;			// 폰트 파일 내용 (스타일이 크기만 달라도 공유한다)
		map<pair<int, int>, Style*> styleOf;		// (오브젝트, 슬롯) -> 스타일

		UINT32 nextHandle;
		map<pair<wstring, wchar_t>, int64_t> handleCache;		// (스타일 이름, 글자) -> 핸들 (-1: 폰트에 글자가 없음, -2: 실패)
		map<string, UINT32> shapeCache;							// 같은 모양(너비, 높이, 픽셀)은 이미지를 공유한다
		Json::Value manifestImages;
		map<wstring, wstring> missing;							// 스타일 이름 -> 폰트에 없던 글자들
		wstring pngDir;

		wstring path(const wstring& relative) { return gameDir + L"/" + relative; }
		bool loadJson(const wstring& relative, Json::Value& value);
		bool saveJson(const wstring& relative, Json::Value& value);

		bool loadStyles();
		shared_ptr<vector<unsigned char> > getFontData(const wstring& relativePath);
		bool collectWantedChars(wstring& wanted);
		int64_t makeGlyph(Style* style, wchar_t ch);
		wchar_t substituteChar(Style* style, wchar_t ch);
	};


	GlyphGenerator::~GlyphGenerator()
	{
		for (size_t i = 0; i < styles.size(); i++) {
			delete styles[i];
		}
		if (library != NULL) {
			FT_Done_FreeType(library);
		}
	}


	bool GlyphGenerator::loadJson(const wstring& relative, Json::Value& value)
	{
		wstring p = path(relative);
		return readFromJson(value, p);
	}


	bool GlyphGenerator::saveJson(const wstring& relative, Json::Value& value)
	{
		wstring p = path(relative);
		return writeIntoJson(value, p);
	}


	shared_ptr<vector<unsigned char> > GlyphGenerator::getFontData(const wstring& relativePath)
	{
		map<wstring, shared_ptr<vector<unsigned char> > >::iterator it = fontData.find(relativePath);
		if (it != fontData.end()) {
			return it->second;
		}
		shared_ptr<vector<unsigned char> > data = FtFont::readFile(path(relativePath));
		if (data) {
			fontData[relativePath] = data;
		}
		return data;
	}


	bool GlyphGenerator::loadStyles()
	{
		const Json::Value& list = config["styles"];
		if (!list.isArray()) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 설정에 styles 배열이 없습니다." << endl;
			return false;
		}

		for (Json::ArrayIndex i = 0; i < list.size(); i++) {
			const Json::Value& s = list[i];
			if (!s["name"].isString() || !s["font"].isString() || !s["size"].isInt() || !s["cellHeight"].isInt() || !s["targets"].isArray()) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << dec << i << L" 번째 스타일에 name, font, size, cellHeight, targets 가 모두 있어야 합니다." << endl;
				return false;
			}

			Style* style = new Style();
			styles.push_back(style);
			style->name = fromUtf8(s["name"].asString());
			shared_ptr<vector<unsigned char> > data = getFontData(fromUtf8(s["font"].asString()));
			if (!data) {
				return false;
			}
			style->size = s["size"].asInt();
			style->cellHeight = s["cellHeight"].asInt();
			style->hasDy = s.isMember("dy");
			style->dy = s.get("dy", 0).asInt();
			style->dx = s.get("dx", 0).asInt();
			style->threshold = s.get("threshold", 128).asInt();
			style->hangulPad = s.get("hangulPad", 0).asInt();
			style->regenerateExisting = s.get("regenerateExisting", false).asBool();
			style->substituteHangulHash = (s.get("substitute", "").asString() == "hangulHash");

			if (!style->font.open(library, data, style->size)) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 스타일 " << style->name << L" 의 폰트를 열거나 크기를 지정하지 못했습니다." << endl;
				return false;
			}

			const Json::Value& targets = s["targets"];
			for (Json::ArrayIndex t = 0; t < targets.size(); t++) {
				if (!targets[t]["object"].isInt() || !targets[t]["slot"].isInt()) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 스타일 " << style->name << L" 의 대상에 object 와 slot 이 필요합니다." << endl;
					return false;
				}
				pair<int, int> key(targets[t]["object"].asInt(), targets[t]["slot"].asInt());
				if (styleOf.find(key) != styleOf.end()) {
					wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 오브젝트 " << dec << key.first << L" 슬롯 " << key.second << L" 에 스타일이 둘 이상 지정되었습니다." << endl;
					return false;
				}
				styleOf[key] = style;
			}
		}
		return true;
	}


	// 번역 json 들에서 "translation" 문자열에 쓰인 글자를 모은다 (코드 포인트 순으로 정렬)
	bool GlyphGenerator::collectWantedChars(wstring& wanted)
	{
		set<wchar_t> used;
		const Json::Value& sources = config["charsFrom"];
		for (Json::ArrayIndex i = 0; i < sources.size(); i++) {
			wstring relative = fromUtf8(sources[i].asString());
			wstring sourcePath = path(relative);
			if (isFileOrDir(sourcePath) != FSOType::FSO_FILE) {
				wcout << infoPrefix << L"(건너뜀: 없음) " << relative << endl;
				continue;
			}
			Json::Value data;
			if (!loadJson(relative, data)) {
				return false;
			}
			vector<const Json::Value*> stack;
			stack.push_back(&data);
			while (!stack.empty()) {
				const Json::Value* node = stack.back();
				stack.pop_back();
				if (node->isObject()) {
					if ((*node)["translation"].isString()) {
						wstring t = fromUtf8((*node)["translation"].asString());
						used.insert(t.begin(), t.end());
					}
					for (Json::ValueConstIterator it = node->begin(); it != node->end(); ++it) {
						if (it->isObject() || it->isArray()) {
							stack.push_back(&(*it));
						}
					}
				}
				else if (node->isArray()) {
					for (Json::ArrayIndex k = 0; k < node->size(); k++) {
						stack.push_back(&(*node)[k]);
					}
				}
			}
		}
		if (config["extraChars"].isString()) {
			wstring extra = fromUtf8(config["extraChars"].asString());
			used.insert(extra.begin(), extra.end());
		}

		wanted.clear();
		for (set<wchar_t>::iterator it = used.begin(); it != used.end(); ++it) {
			if (*it >= L' ' && *it != 0x7F) {
				wanted.push_back(*it);
			}
		}
		return true;
	}


	// 폰트에 없는 글자를 대신할 글자. hangulHash: 코드 포인트로 폰트의 한글 음절 하나를 골라 기호가 고르게 퍼지게 한다
	wchar_t GlyphGenerator::substituteChar(Style* style, wchar_t ch)
	{
		const vector<wchar_t>& syllables = style->font.hangulSyllables();
		if (style->substituteHangulHash && !syllables.empty()) {
			return syllables[(size_t)ch % syllables.size()];
		}
		return 0;
	}


	// 스타일로 글자 이미지를 만들어 핸들을 돌려준다. 폰트에 글자가 없으면 -1, 실패하면 -2
	int64_t GlyphGenerator::makeGlyph(Style* style, wchar_t ch)
	{
		pair<wstring, wchar_t> key(style->name, ch);
		map<pair<wstring, wchar_t>, int64_t>::iterator cached = handleCache.find(key);
		if (cached != handleCache.end()) {
			return cached->second;
		}

		wchar_t drawCh = ch;
		if (!style->font.hasChar(ch)) {
			wchar_t sub = substituteChar(style, ch);
			if (sub == 0) {
				missing[style->name].push_back(ch);
				handleCache[key] = -1;
				return -1;
			}
			drawCh = sub;
		}
		GlyphBitmap bitmap;
		if (!style->font.render(drawCh, bitmap)) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 스타일 " << style->name << L" 에서 글자(U+" << hex << (int)ch << dec << L")를 그리지 못했습니다." << endl;
			return -2;
		}

		const int cellHeight = style->cellHeight;
		const int dx = style->dx;
		const int ascent = style->font.ascent();
		const int dy = style->hasDy ? style->dy : floorDiv(cellHeight - (ascent + style->font.descent()), 2);

		int advancePx = bitmap.advance;
		int inkRight = (bitmap.width > 0) ? (bitmap.left + bitmap.width) : 0;
		int bboxRight = (std::max)(advancePx, inkRight);

		int width = (std::max)(1, (std::max)(advancePx, dx + bboxRight));
		if (isHangul(ch)) {
			width += style->hangulPad;
		}

		// 글리프를 그려서 문턱값 이상인 픽셀만 흰색으로 남긴다 (흰색/투명 두 가지 색)
		vector<unsigned char> rgba((size_t)width * cellHeight * 4, 0);
		if (bitmap.buffer != NULL && bitmap.width > 0 && bitmap.rows > 0) {
			int originX = dx + bitmap.left;
			int originY = dy + ascent - bitmap.top;
			for (int gy = 0; gy < bitmap.rows; gy++) {
				int py = originY + gy;
				if (py < 0 || py >= cellHeight) {
					continue;
				}
				for (int gx = 0; gx < bitmap.width; gx++) {
					int px = originX + gx;
					if (px < 0 || px >= width) {
						continue;
					}
					if (bitmap.buffer[gy * bitmap.pitch + gx] >= style->threshold) {
						unsigned char* p = &rgba[((size_t)py * width + px) * 4];
						p[0] = p[1] = p[2] = p[3] = 255;
					}
				}
			}
		}

		string shapeKey((const char*)&width, sizeof(width));
		shapeKey.append((const char*)&cellHeight, sizeof(cellHeight));
		shapeKey.append((const char*)&rgba[0], rgba.size());
		map<string, UINT32>::iterator shared = shapeCache.find(shapeKey);
		if (shared != shapeCache.end()) {
			handleCache[key] = shared->second;
			return shared->second;
		}

		UINT32 handle = nextHandle++;
		wostringstream fileName;
		fileName << dec << handle << L".png";
		BYTE* buffer = &rgba[0];
		UINT32 w = (UINT32)width, h = (UINT32)cellHeight, bitCount = 32;
		if (!writeToPng(buffer, pngDir + L"/" + fileName.str(), w, h, bitCount)) {
			return -2;
		}
		Json::Value item(Json::objectValue);
		item["handle"] = (Json::UInt)handle;
		item["file"] = "glyphs/" + utf8(fileName.str());
		manifestImages.append(item);
		shapeCache[shapeKey] = handle;
		handleCache[key] = handle;
		return handle;
	}


	bool GlyphGenerator::run(const wstring& configPath)
	{
		wstring configPathCopy = configPath;
		if (!readFromJson(config, configPathCopy)) {
			return false;
		}
		if (FT_Init_FreeType(&library) != 0) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": FreeType 을 초기화하지 못했습니다." << endl;
			return false;
		}
		if (!loadStyles()) {
			return false;
		}

		const wstring manifestRelative = L"image/appendImages.json";
		wstring manifestPath = path(manifestRelative);
		if (isFileOrDir(manifestPath) == FSOType::FSO_FILE) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 이미 image/appendImages.json 이 있습니다. 새로 추출한 폴더에서 실행하세요." << endl;
			return false;
		}

		// 1) 번역 문자열에서 쓰는 글자 모으기
		wstring wanted;
		if (!collectWantedChars(wanted)) {
			return false;
		}
		wcout << infoPrefix << L"번역에 쓰인 글자 " << dec << wanted.size() << L"종" << endl;

		// 2) 대상 오브젝트 읽기 (설정에 처음 나온 순서)
		Json::Value imageList;
		if (!loadJson(L"image/imageList.json", imageList)) {
			return false;
		}
		nextHandle = imageList["nextHandle"].asUInt();

		vector<int> objectOrder;
		map<int, Json::Value> objects;
		for (size_t i = 0; i < styles.size(); i++) {
			const Json::Value& targets = config["styles"][(Json::ArrayIndex)i]["targets"];
			for (Json::ArrayIndex t = 0; t < targets.size(); t++) {
				int object = targets[t]["object"].asInt();
				if (objects.find(object) == objects.end()) {
					wchar_t relative[64];
					swprintf(relative, 64, L"script/object/%06d.json", object);
					if (!loadJson(relative, objects[object])) {
						return false;
					}
					objectOrder.push_back(object);
				}
			}
		}

		pngDir = path(L"image/glyphs");
		createDirectoryRecursively(pngDir);
		manifestImages = Json::Value(Json::arrayValue);

		// 글자 슬롯: 프레임 수가 문자표 길이와 같은 애니메이션
		struct Local
		{
			static vector<Json::Value*> glyphSlots(Json::Value& d)
			{
				vector<Json::Value*> slots;
				size_t charsetLength = fromUtf8(d["alterableStrings"][0].asString()).size();
				for (Json::ArrayIndex a = 0; a < d["animations"].size(); a++) {
					Json::Value& anim = d["animations"][a];
					if (anim["directions"][0]["frames"].size() == charsetLength) {
						slots.push_back(&anim);
					}
				}
				return slots;
			}
		};

		wstring fallbackText = config.isMember("fallbackChar") ? fromUtf8(config["fallbackChar"].asString()) : L"?";
		wchar_t fallbackChar = fallbackText.empty() ? L'?' : fallbackText[0];

		struct SummaryItem { int object; int slot; wstring what; size_t count; };
		vector<SummaryItem> summary;

		for (size_t o = 0; o < objectOrder.size(); o++) {
			int object = objectOrder[o];
			Json::Value& d = objects[object];
			wstring charset = fromUtf8(d["alterableStrings"][0].asString());

			wstring newChars;
			for (size_t c = 0; c < wanted.size(); c++) {
				if (charset.find(wanted[c]) == wstring::npos) {
					newChars.push_back(wanted[c]);
				}
			}

			// 스타일에 regenerateExisting 이 있으면 문자표에 이미 있는 글자도 새 폰트로 다시 그려 한글과 모양/간격을 맞춘다
			size_t regenerated = 0;
			vector<Json::Value*> slots = Local::glyphSlots(d);
			for (size_t s = 0; s < slots.size(); s++) {
				map<pair<int, int>, Style*>::iterator it = styleOf.find(pair<int, int>(object, (*slots[s])["slot"].asInt()));
				if (it == styleOf.end() || !it->second->regenerateExisting) {
					continue;
				}
				Json::Value& frames = (*slots[s])["directions"][0]["frames"];
				for (size_t idx = 0; idx < charset.size(); idx++) {
					if (charset[idx] == L' ' || idx >= frames.size()) {
						continue;
					}
					int64_t handle = makeGlyph(it->second, charset[idx]);
					if (handle == -2) {
						return false;
					}
					if (handle >= 0) {
						frames[(Json::ArrayIndex)idx] = (Json::UInt)handle;
						regenerated++;
					}
				}
			}
			if (regenerated > 0) {
				SummaryItem item = { object, 0, L"기존 글자 다시 그림", regenerated };
				summary.push_back(item);
			}
			if (newChars.empty()) {
				continue;
			}

			size_t fallbackIndex = charset.find(fallbackChar);
			if (fallbackIndex == wstring::npos) {
				fallbackIndex = charset.find(L' ');
			}
			if (fallbackIndex == wstring::npos) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 오브젝트 " << dec << object << L" 의 문자표에 대체 글자(" << fallbackChar << L")도 공백도 없습니다." << endl;
				return false;
			}
			for (size_t s = 0; s < slots.size(); s++) {
				int slot = (*slots[s])["slot"].asInt();
				Json::Value& frames = (*slots[s])["directions"][0]["frames"];
				map<pair<int, int>, Style*>::iterator it = styleOf.find(pair<int, int>(object, slot));
				Json::Value fallbackFrame = frames[(Json::ArrayIndex)fallbackIndex];
				for (size_t c = 0; c < newChars.size(); c++) {
					int64_t handle = (it != styleOf.end()) ? makeGlyph(it->second, newChars[c]) : -1;
					if (handle == -2) {
						return false;
					}
					frames.append((handle >= 0) ? Json::Value((Json::UInt)handle) : fallbackFrame);
				}
				SummaryItem item = { object, slot, (it != styleOf.end()) ? it->second->name : wstring(L"(대상 아님: 대체 글자 사용)"), newChars.size() };
				summary.push_back(item);
			}
			d["alterableStrings"][0] = utf8(charset + newChars);
		}

		for (size_t o = 0; o < objectOrder.size(); o++) {
			wchar_t relative[64];
			swprintf(relative, 64, L"script/object/%06d.json", objectOrder[o]);
			if (!saveJson(relative, objects[objectOrder[o]])) {
				return false;
			}
		}

		// 새 이미지의 헤더 값을 복제할 템플릿: 대상 오브젝트의 첫 글자 슬롯의 첫 프레임
		if (manifestImages.size() > 0) {
			Json::Value templateHandle;
			for (size_t o = 0; o < objectOrder.size() && templateHandle.isNull(); o++) {
				vector<Json::Value*> slots = Local::glyphSlots(objects[objectOrder[o]]);
				if (!slots.empty()) {
					templateHandle = (*slots[0])["directions"][0]["frames"][0];
				}
			}
			Json::Value manifest(Json::objectValue);
			manifest["type"] = "imageAppend";
			manifest["version"] = 1;
			manifest["templateHandle"] = templateHandle;
			manifest["images"] = manifestImages;
			if (!saveJson(manifestRelative, manifest)) {
				return false;
			}
		}

		for (size_t i = 0; i < summary.size(); i++) {
			wcout << infoPrefix << L"오브젝트 " << dec << summary[i].object << L" 슬롯 " << summary[i].slot << L": " << summary[i].what << L", 글자 " << summary[i].count << L"개 추가" << endl;
		}
		wcout << infoPrefix << L"새 이미지 " << dec << manifestImages.size() << L"개";
		if (manifestImages.size() > 0) {
			wcout << L" (핸들 " << manifestImages[0]["handle"].asUInt() << L".." << manifestImages[manifestImages.size() - 1]["handle"].asUInt() << L")";
		}
		wcout << endl;
		for (map<wstring, wstring>::iterator it = missing.begin(); it != missing.end(); ++it) {
			wcout << L"[*] warning: 스타일 " << it->first << L" 의 폰트에 없는 글자 " << dec << it->second.size() << L"종 (대체 글자로 처리): " << it->second.substr(0, 60) << endl;
		}
		return true;
	}

}


bool CTT::Glyph::generateGlyphs(const wstring& gameDir, const wstring& configPath)
{
	GlyphGenerator generator(gameDir);
	return generator.run(configPath);
}


int CTT::Glyph::runGlyphCommand(int argc, WCHAR** argv)
{
	if (argc < 4) {
		wcout << L"usage: --glyphs <extracted folder> <glyph config json>" << endl;
		return 2;
	}
	return generateGlyphs(argv[2], argv[3]) ? 0 : 1;
}
