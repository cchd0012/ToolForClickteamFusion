#include "Dx9Application.h"
#include "../Glyph/FtFont.h"
#include "../Glyph/TxtbltFontPatch.h"
#include "../Util/CommonUtil.h"
#include "../Util/JsonUtil.h"

#include <algorithm>

using namespace CTT::App;
using namespace CTT::Glyph;
using namespace CTT::CommonUtil;
using namespace CTT::JsonUtil;


// 한글화 도구 창: 글자 아틀라스 만들기 / 번호 배정 / 모듈 패치, 블리터 글자표 배정 / 예시 문장 미리보기
// (작업 흐름 안내와 상태 구조체는 HangulUi.h 참고)


HangulUiState::HangulUiState()
{
	requestTab = -1;

	strcpy_s(atlasName, "atlas1");
	memset(fontPath, 0, sizeof(fontPath));
	fontSize = 10;
	atlasMode = 0;
	memset(extraChars, 0, sizeof(extraChars));
	color[0] = color[1] = color[2] = 1.0f;
	outlineColor[0] = outlineColor[1] = outlineColor[2] = 0.0f;
	outlineSize = 1;
	letterSpacing = 0;
	dy = 0;
	atlasWidth = 1024;
	buildRunning = 0;
	buildDone = 0;
	buildOk = 0;
	atlasesLoaded = false;

	memset(moduleInput, 0, sizeof(moduleInput));
	memset(moduleOutput, 0, sizeof(moduleOutput));

	blittersLoaded = false;
	strcpy_s(liveSample, u8"한글 폰트 미리보기 가나다라 마바사 ABC abc 123 ?!");
	fontReloadKnown = false;
	fontReloadDetected = false;
	fontReloadCount = 0;
	fontReloadFrames = 0;
	currentBlitter = -1;
	memset(filter, 0, sizeof(filter));
	applyTable = 0;
	applyLayout = 1;

	strcpy_s(sample, u8"하이톱 마운틴 저장 옵션\n모험을 시작하세요! ABC abc 123");
	previewWidth = 160;
	previewScale = 3;
	previewAtlas = -1;
	messageIsError = false;
}


HangulUiState::~HangulUiState()
{
	releaseTextures();
}


void HangulUiState::releaseTextures()
{
	for (map<wstring, HangulPreviewAtlas>::iterator it = previewCache.begin(); it != previewCache.end(); ++it) {
		if (it->second.texture != NULL) {
			it->second.texture->Release();
			it->second.texture = NULL;
		}
	}
	previewCache.clear();
	if (livePreview.texture != NULL) {
		livePreview.texture->Release();
		livePreview.texture = NULL;
	}
	livePreview.data = CTT::Glyph::FontAtlasData();
	liveKey.clear();
}


namespace {

	string utf8(const wstring& s) { return getAnsiStr(s, CodePage::CP_UTF_8); }
	wstring fromUtf8(const string& s) { return getUniStr(s, CodePage::CP_UTF_8); }

	string lowerAscii(string s)
	{
		for (size_t i = 0; i < s.size(); i++) {
			if (s[i] >= 'A' && s[i] <= 'Z') {
				s[i] = (char)(s[i] - 'A' + 'a');
			}
		}
		return s;
	}

	bool dirExists(const wstring& path)
	{
		DWORD attr = GetFileAttributesW(path.c_str());
		return (attr != INVALID_FILE_ATTRIBUTES) && ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0);
	}

	bool fileExists(const wstring& path)
	{
		DWORD attr = GetFileAttributesW(path.c_str());
		return (attr != INVALID_FILE_ATTRIBUTES) && ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0);
	}

	void setText(char* dst, size_t size, const string& value)
	{
		strncpy_s(dst, size, value.c_str(), _TRUNCATE);
	}

	void setMessage(HangulUiState& hangul, const string& text, bool isError)
	{
		hangul.message = text;
		hangul.messageIsError = isError;
	}

	// 폴더 안의 파일 이름을 모은다 (패턴 예: L"*.json")
	vector<wstring> listFiles(const wstring& dir, const wstring& pattern)
	{
		vector<wstring> names;
		WIN32_FIND_DATAW data;
		HANDLE h = FindFirstFileW((dir + L"/" + pattern).c_str(), &data);
		if (h == INVALID_HANDLE_VALUE) {
			return names;
		}
		do {
			if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
				names.push_back(data.cFileName);
			}
		} while (FindNextFileW(h, &data));
		FindClose(h);
		sort(names.begin(), names.end());
		return names;
	}

	wstring archiveBase(CTT::Archive::Archive* archive)
	{
		wstring path = archive->getArchivePath();
		return getExtentionRemovedPath(path);
	}

	wstring glyphDir(CTT::Archive::Archive* archive) { return archiveBase(archive) + L"/glyph"; }
	wstring atlasDir(CTT::Archive::Archive* archive) { return glyphDir(archive) + L"/atlas"; }
	wstring registryPath(CTT::Archive::Archive* archive) { return glyphDir(archive) + L"/atlases.json"; }


	// ---- 아틀라스 목록 / 번호

	void loadAtlasList(CTT::Archive::Archive* archive, HangulUiState& hangul)
	{
		hangul.atlases.clear();
		// 저장해 둔 번호
		map<wstring, INT32> idByName;
		wstring registry = registryPath(archive);
		if (fileExists(registry)) {
			Json::Value root;
			if (readFromJson(root, registry) && root["assignments"].isArray()) {
				const Json::Value& list = root["assignments"];
				for (Json::ArrayIndex i = 0; i < list.size(); i++) {
					idByName[fromUtf8(list[i].get("atlas", "").asString())] = list[i].get("id", 0).asInt();
				}
			}
		}
		wstring dir = atlasDir(archive);
		vector<wstring> files = listFiles(dir, L"*.json");
		for (size_t i = 0; i < files.size(); i++) {
			Json::Value meta;
			wstring path = dir + L"/" + files[i];
			if (!readFromJson(meta, path) || meta.get("type", "").asString() != "fontAtlas") {
				continue;
			}
			HangulAtlasItem item;
			item.name = fromUtf8(meta.get("name", "").asString());
			item.glyphCount = meta.get("glyphCount", 0).asUInt();
			item.lineHeight = meta.get("lineHeight", 0).asUInt();
			map<wstring, INT32>::iterator it = idByName.find(item.name);
			item.id = (it != idByName.end()) ? it->second : 0;
			hangul.atlases.push_back(item);
		}
		hangul.atlasesLoaded = true;
		hangul.atlasSnap = takeDirSnapshot(dir, L"*");
		hangul.registrySnap = takeDirSnapshot(glyphDir(archive), L"atlases.json");
	}

	// 번호가 겹치지 않는지 확인하고 설정 파일(txtblt-font patch 의 설정과 같은 형식)로 저장한다
	bool saveRegistry(CTT::Archive::Archive* archive, HangulUiState& hangul, string& error)
	{
		set<INT32> used;
		Json::Value list(Json::arrayValue);
		for (size_t i = 0; i < hangul.atlases.size(); i++) {
			const HangulAtlasItem& a = hangul.atlases[i];
			if (a.id <= 0) {
				continue;
			}
			if (used.count(a.id) > 0) {
				error = u8"같은 번호를 두 아틀라스에 배정할 수 없습니다";
				return false;
			}
			used.insert(a.id);
			Json::Value item(Json::objectValue);
			item["id"] = a.id;
			item["atlasDir"] = "atlas";
			item["atlas"] = utf8(a.name);
			list.append(item);
		}
		Json::Value root(Json::objectValue);
		root["assignments"] = list;
		createDirectoryRecursively(glyphDir(archive));
		wstring path = registryPath(archive);
		if (!writeIntoJson(root, path)) {
			error = u8"배정 파일을 저장하지 못했습니다";
			return false;
		}
		return true;
	}


	// ---- 아틀라스 만들기 스레드

	struct BuildJob
	{
		HangulUiState* hangul;
		wstring config;
	};

	DWORD WINAPI buildThread(LPVOID param)
	{
		BuildJob* job = (BuildJob*)param;
		bool ok = buildFontAtlas(job->config);
		InterlockedExchange(&job->hangul->buildOk, ok ? 1 : 0);
		InterlockedExchange(&job->hangul->buildDone, 1);
		delete job;
		return 0;
	}


	// ---- 미리보기

	// 아틀라스 하나를 읽어 D3D 텍스처로 올린다 (이미 있으면 그대로)
	HangulPreviewAtlas* ensurePreviewAtlas(LPDIRECT3DDEVICE9 device, CTT::Archive::Archive* archive, HangulUiState& hangul, const wstring& name)
	{
		map<wstring, HangulPreviewAtlas>::iterator found = hangul.previewCache.find(name);
		if (found != hangul.previewCache.end()) {
			return (found->second.texture != NULL) ? &found->second : NULL;
		}
		HangulPreviewAtlas& pa = hangul.previewCache[name];
		if (!loadFontAtlas(pa.data, atlasDir(archive), name) || pa.data.bgra.empty()) {
			return NULL;
		}
		LPDIRECT3DTEXTURE9 texture = NULL;
		if (FAILED(device->CreateTexture(pa.data.atlasWidth, pa.data.atlasHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, NULL)) || texture == NULL) {
			return NULL;
		}
		D3DLOCKED_RECT rect;
		if (FAILED(texture->LockRect(0, &rect, NULL, 0))) {
			texture->Release();
			return NULL;
		}
		for (UINT y = 0; y < pa.data.atlasHeight; y++) {
			memcpy((BYTE*)rect.pBits + (size_t)y * rect.Pitch, &pa.data.bgra[(size_t)y * pa.data.atlasWidth * 4], (size_t)pa.data.atlasWidth * 4);
		}
		texture->UnlockRect(0);
		pa.texture = texture;
		return &pa;
	}


	// 폰트 파일이 BMFont(.fnt)이면 폰트에서 그리지 않고 만들어진 글자 그림을 그대로 가져온다
	bool isBmFontPath(const char* path)
	{
		size_t n = strlen(path);
		return n > 4 && _stricmp(path + n - 4, ".fnt") == 0;
	}

	// 폼의 값으로 메모리에 작은 아틀라스를 만들어 미리보기 텍스처를 갱신한다. 설정이 같으면 다시 만들지 않는다
	void updateLivePreview(LPDIRECT3DDEVICE9 device, HangulUiState& h)
	{
		char colorKey[96];
		sprintf_s(colorKey, "%.3f,%.3f,%.3f|%.3f,%.3f,%.3f", h.color[0], h.color[1], h.color[2], h.outlineColor[0], h.outlineColor[1], h.outlineColor[2]);
		string key = string(h.fontPath) + "|" + to_string(h.fontSize) + "|" + to_string(h.outlineSize) + "|" + to_string(h.letterSpacing) + "|" + to_string(h.dy) + "|" + colorKey + "|" + h.liveSample;
		if (key == h.liveKey) {
			return;
		}
		h.liveKey = key;
		if (h.livePreview.texture != NULL) {
			h.livePreview.texture->Release();
			h.livePreview.texture = NULL;
		}
		h.livePreview.data = CTT::Glyph::FontAtlasData();
		h.liveError.clear();
		if (strlen(h.fontPath) == 0 || (!isBmFontPath(h.fontPath) && h.fontSize <= 0)) {
			return;
		}

		Json::Value cfg(Json::objectValue);
		cfg["name"] = "preview";
		if (isBmFontPath(h.fontPath)) {
			cfg["bmfont"] = h.fontPath;
		}
		else {
			cfg["font"] = h.fontPath;
			cfg["size"] = h.fontSize;
			cfg["mode"] = "limitedHangul";
			cfg["extraChars"] = h.liveSample;
		}
		cfg["outlineSize"] = h.outlineSize;
		cfg["letterSpacing"] = h.letterSpacing;
		cfg["dy"] = h.dy;
		cfg["atlasWidth"] = 1024;
		Json::Value color(Json::arrayValue);
		Json::Value outline(Json::arrayValue);
		for (int c = 0; c < 3; c++) {
			color.append((int)(h.color[c] * 255.0f + 0.5f));
			outline.append((int)(h.outlineColor[c] * 255.0f + 0.5f));
		}
		cfg["color"] = color;
		cfg["outlineColor"] = outline;
		if (!CTT::Glyph::buildFontAtlasData(cfg, wstring(), h.livePreview.data, true) || h.livePreview.data.bgra.empty()) {
			h.liveError = u8"폰트를 열지 못했거나 글자를 만들지 못했습니다. 폰트 파일 경로와 글자 크기를 확인하세요";
			h.livePreview.data = CTT::Glyph::FontAtlasData();
			return;
		}
		LPDIRECT3DTEXTURE9 texture = NULL;
		if (FAILED(device->CreateTexture(h.livePreview.data.atlasWidth, h.livePreview.data.atlasHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, NULL)) || texture == NULL) {
			h.liveError = u8"미리보기 그림을 만들지 못했습니다";
			return;
		}
		D3DLOCKED_RECT rect;
		if (FAILED(texture->LockRect(0, &rect, NULL, 0))) {
			texture->Release();
			h.liveError = u8"미리보기 그림을 만들지 못했습니다";
			return;
		}
		for (UINT y = 0; y < h.livePreview.data.atlasHeight; y++) {
			memcpy((BYTE*)rect.pBits + (size_t)y * rect.Pitch, &h.livePreview.data.bgra[(size_t)y * h.livePreview.data.atlasWidth * 4], (size_t)h.livePreview.data.atlasWidth * 4);
		}
		texture->UnlockRect(0);
		h.livePreview.texture = texture;
	}

	void setPointSampler(const ImDrawList*, const ImDrawCmd* cmd)
	{
		LPDIRECT3DDEVICE9 device = (LPDIRECT3DDEVICE9)cmd->UserCallbackData;
		device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
	}

	void setLinearSampler(const ImDrawList*, const ImDrawCmd* cmd)
	{
		LPDIRECT3DDEVICE9 device = (LPDIRECT3DDEVICE9)cmd->UserCallbackData;
		device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	}

	// 문자열 한 글자를 글자표 코드(1바이트 또는 CP949 2바이트)로 바꾼다
	UINT16 toCode(wchar_t ch)
	{
		if (ch < 0x80) {
			return (UINT16)ch;
		}
		char bytes[4] = { 0, 0, 0, 0 };
		BOOL usedDefault = FALSE;
		int n = WideCharToMultiByte(949, WC_NO_BEST_FIT_CHARS, &ch, 1, bytes, 4, NULL, &usedDefault);
		if (n == 2 && !usedDefault) {
			return (UINT16)(((BYTE)bytes[0] << 8) | (BYTE)bytes[1]);
		}
		return (UINT16)'?';
	}

	struct PlacedGlyph
	{
		UINT16 code;
		int x;
		int line;
	};

	// 문자열을 상자 폭에 맞춰 놓는다. 게임 안의 배치(txtblt 의 layout)와 같은 규칙의 근사이다:
	// 줄 폭은 pixel 방식이면 글자 폭(advance)의 합, 원래 방식이면 글자 수 x 칸 폭이다 (그리는 위치는 늘 글자 폭대로 이어 붙인다).
	// 자동 줄바꿈이면 공백 뒤 단어는 통째로 다음 줄로 넘기고, 단어가 줄보다 길면 글자 단위로 끊는다.
	// 줄마다 정렬(가운데, 오른쪽)을 줄 폭 기준으로 옮긴다
	int layoutPreview(const FontAtlasData& atlas, const wstring& text, const BlitterLayout& lay, vector<PlacedGlyph>& placed)
	{
		struct Local
		{
			static int advanceOf(const FontAtlasData& a, UINT16 code)
			{
				map<UINT16, FontAtlasGlyph>::const_iterator it = a.glyphs.find(code);
				return (it != a.glyphs.end()) ? (int)it->second.advance : (int)a.missingAdvance;
			}
			// 줄 폭을 잴 때 글자 하나가 차지하는 폭
			static int measureOf(const FontAtlasData& a, const BlitterLayout& l, UINT16 code)
			{
				return l.pixel ? advanceOf(a, code) : l.cellWidth * ((code > 0xFF) ? 2 : 1);
			}
		};

		const int boxWidth = lay.boxWidth;
		int line = 0;
		int x = 0;				// 그리는 위치
		int measured = 0;		// 줄 폭 계산에 쓰는 폭
		vector<int> lineWidths;
		size_t i = 0;
		while (i < text.size()) {
			wchar_t ch = text[i];
			if (ch == L'\r') {
				i++;
				continue;
			}
			if (ch == L'\n') {
				lineWidths.push_back(measured);
				line++;
				x = 0;
				measured = 0;
				i++;
				continue;
			}
			if (ch == L' ') {
				// 줄 맨 앞의 공백은 건너뛴다
				if (measured > 0) {
					PlacedGlyph g = { (UINT16)' ', x, line };
					placed.push_back(g);
					x += Local::advanceOf(atlas, ' ');
					measured += Local::measureOf(atlas, lay, ' ');
				}
				i++;
				continue;
			}
			// 단어 하나
			size_t j = i;
			int wordWidth = 0;
			while (j < text.size() && text[j] != L' ' && text[j] != L'\n' && text[j] != L'\r') {
				wordWidth += Local::measureOf(atlas, lay, toCode(text[j]));
				j++;
			}
			if (lay.wrap && measured > 0 && measured + wordWidth > boxWidth) {
				lineWidths.push_back(measured);
				line++;
				x = 0;
				measured = 0;
			}
			for (size_t k = i; k < j; k++) {
				UINT16 code = toCode(text[k]);
				int measure = Local::measureOf(atlas, lay, code);
				if (lay.wrap && measured > 0 && measured + measure > boxWidth) {
					lineWidths.push_back(measured);
					line++;
					x = 0;
					measured = 0;
				}
				PlacedGlyph g = { code, x, line };
				placed.push_back(g);
				x += Local::advanceOf(atlas, code);
				measured += measure;
			}
			i = j;
		}
		lineWidths.push_back(measured);

		if (lay.align != 0) {
			for (size_t k = 0; k < placed.size(); k++) {
				int room = boxWidth - lineWidths[placed[k].line];
				if (room > 0) {
					placed[k].x += (lay.align == 1) ? room / 2 : room;
				}
			}
		}
		return line + 1;
	}

	// 글자를 상자 안에 그려서 미리 보여 준다. 반환값은 그린 줄 수
	// 문자열을 폰트 아틀라스 글자로 origin 에 그린다 (화면 좌표). background 이면 초록 바탕을 깐다.
	// 그린 글자 전체의 크기를 outSize 로 돌려주고, 줄 수를 돌려준다
	int drawPreviewAt(LPDIRECT3DDEVICE9 device, HangulPreviewAtlas& pa, const string& utf8Text, const BlitterLayout& layout, float scale, ImVec2 origin, bool background, ImVec2* outSize)
	{
		const int boxWidth = layout.boxWidth;
		vector<PlacedGlyph> placed;
		int lines = layoutPreview(pa.data, fromUtf8(utf8Text), layout, placed);
		int lineHeight = pa.data.lineHeight;

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		if (background) {
			drawList->AddRectFilled(origin, ImVec2(origin.x + boxWidth * scale, origin.y + lines * lineHeight * scale), IM_COL32(32, 80, 56, 255));
		}
		int right = 0;
		drawList->AddCallback(setPointSampler, device);
		for (size_t k = 0; k < placed.size(); k++) {
			map<UINT16, FontAtlasGlyph>::const_iterator it = pa.data.glyphs.find(placed[k].code);
			if (it == pa.data.glyphs.end()) {
				continue;
			}
			const FontAtlasGlyph& g = it->second;
			right = max(right, placed[k].x + (int)g.advance);
			if (g.width == 0 || g.height == 0) {
				continue;
			}
			ImVec2 p0(origin.x + (placed[k].x + g.offsetX) * scale, origin.y + placed[k].line * lineHeight * scale);
			ImVec2 p1(p0.x + g.width * scale, p0.y + g.height * scale);
			ImVec2 uv0((float)g.x / pa.data.atlasWidth, (float)g.y / pa.data.atlasHeight);
			ImVec2 uv1((float)(g.x + g.width) / pa.data.atlasWidth, (float)(g.y + g.height) / pa.data.atlasHeight);
			drawList->AddImage((ImTextureID)pa.texture, p0, p1, uv0, uv1);
		}
		drawList->AddCallback(setLinearSampler, device);
		if (outSize != NULL) {
			*outSize = ImVec2((background || layout.showBox) ? boxWidth * scale : right * scale, lines * lineHeight * scale);
		}
		return lines;
	}

	int drawPreview(LPDIRECT3DDEVICE9 device, HangulPreviewAtlas& pa, const string& utf8Text, int boxWidth, int scale)
	{
		ImVec2 size;
		BlitterLayout layout;
		layout.boxWidth = boxWidth;
		int lines = drawPreviewAt(device, pa, utf8Text, layout, (float)scale, ImGui::GetCursorScreenPos(), true, &size);
		ImGui::Dummy(size);
		return lines;
	}


	// ---- 블리터 목록

	void loadBlitters(CTT::Archive::Archive* archive, HangulUiState& hangul)
	{
		hangul.blitters.clear();
		hangul.textsByName.clear();
		hangul.fontReloadKnown = false;
		hangul.fontReloadDetected = false;
		hangul.fontReloadCount = 0;
		hangul.fontReloadFrames = 0;
		hangul.fontReloadExample.clear();
		hangul.currentBlitter = -1;

		wstring base = archiveBase(archive);
		wstring objectDir = base + L"/script/object";
		vector<wstring> files = listFiles(objectDir, L"*.json");
		for (size_t i = 0; i < files.size(); i++) {
			Json::Value root;
			wstring path = objectDir + L"/" + files[i];
			if (!readFromJson(root, path)) {
				continue;
			}
			if (root.get("identifier", "").asString() != "TLBT" || !root.isMember("textBlitter")) {
				continue;
			}
			HangulBlitterItem item;
			item.index = root.get("index", 0).asInt();
			item.name = root.get("name", "").asString();
			const Json::Value& strings = root["alterableStrings"];
			if (strings.isArray() && strings.size() > 0) {
				item.font = strings[0].asString();
			}
			const Json::Value& blitter = root["textBlitter"];
			item.initialText = blitter.get("text", "").asString();
			item.fontTable = blitter.get("fontTable", 0).asInt();
			item.cellWidth = blitter.get("cellWidth", 0).asInt();
			item.cellHeight = blitter.get("cellHeight", 0).asInt();
			item.imageHandle = blitter.get("imageHandle", 0).asInt();
			item.imageWidth = blitter.get("imageWidth", 0).asInt();
			item.imageHeight = blitter.get("imageHeight", 0).asInt();
			const Json::Value& flagNames = blitter["flagNames"];
			for (Json::ArrayIndex f = 0; f < flagNames.size(); f++) {
				item.flagNames.push_back(flagNames[f].asString());
			}
			item.pixel = (blitter.get("layout", "byte").asString() == "pixel");
			item.jsonPath = path;
			hangul.blitters.push_back(item);
		}

		// 블리터가 받는 문자열 (텍스트 흐름 분석 결과)
		wstring flowPath = base + L"/text/textFlow.json";
		if (fileExists(flowPath)) {
			Json::Value flow;
			if (readFromJson(flow, flowPath) && flow["blitters"].isArray()) {
				const Json::Value& list = flow["blitters"];
				for (Json::ArrayIndex i = 0; i < list.size(); i++) {
					vector<string>& texts = hangul.textsByName[list[i].get("name", "").asString()];
					const Json::Value& t = list[i]["texts"];
					for (Json::ArrayIndex k = 0; k < t.size(); k++) {
						texts.push_back(t[k].asString());
					}
				}
				const Json::Value& reload = flow["fontReload"];
				if (reload.isObject()) {
					hangul.fontReloadKnown = true;
					hangul.fontReloadDetected = reload.get("detected", false).asBool();
					hangul.fontReloadCount = reload.get("loadImage", 0).asUInt();
					set<UINT32> frames;
					const Json::Value& loads = reload["loads"];
					for (Json::ArrayIndex i = 0; i < loads.size(); i++) {
						if (loads[i].get("action", "").asString() != "loadImage") {
							continue;
						}
						frames.insert(loads[i].get("frame", 0).asUInt());
						const Json::Value& literals = loads[i]["literals"];
						if (hangul.fontReloadExample.empty() && literals.isArray() && (literals.size() > 0)) {
							hangul.fontReloadExample = literals[0].asString();
						}
					}
					hangul.fontReloadFrames = (UINT32)frames.size();
				}
			}
		}

		for (size_t i = 0; i < hangul.blitters.size(); i++) {
			if (hangul.tableByFont.find(hangul.blitters[i].font) == hangul.tableByFont.end()) {
				hangul.tableByFont[hangul.blitters[i].font] = 0;
			}
		}
		hangul.blittersLoaded = true;
		hangul.objectSnap = takeDirSnapshot(objectDir, L"*.json");
		hangul.flowSnap = takeDirSnapshot(base + L"/text", L"textFlow.json");
	}

	bool saveBlitters(HangulUiState& hangul, UINT32& savedCount)
	{
		savedCount = 0;
		for (size_t i = 0; i < hangul.blitters.size(); i++) {
			HangulBlitterItem& item = hangul.blitters[i];
			if (!item.dirty) {
				continue;
			}
			Json::Value root;
			if (!readFromJson(root, item.jsonPath)) {
				return false;
			}
			root["textBlitter"]["fontTable"] = item.fontTable;
			root["textBlitter"]["layout"] = (item.fontTable > 0 && item.pixel) ? "pixel" : "byte";
			if (!writeIntoJson(root, item.jsonPath)) {
				return false;
			}
			item.dirty = false;
			savedCount++;
		}
		return true;
	}

	// 번호에 맞는 아틀라스 이름을 찾는다 (없으면 빈 문자열)
	wstring atlasNameForId(const HangulUiState& hangul, INT32 id)
	{
		for (size_t i = 0; i < hangul.atlases.size(); i++) {
			if (hangul.atlases[i].id == id && id > 0) {
				return hangul.atlases[i].name;
			}
		}
		return wstring();
	}

	// "번호 - 이름" 콤보. 현재 값을 바꾸면 true
	bool tableCombo(const char* label, const HangulUiState& hangul, INT32& id)
	{
		string current = u8"없음 (원래 방식)";
		if (id > 0) {
			wstring name = atlasNameForId(hangul, id);
			current = to_string(id) + " - " + (name.empty() ? string(u8"(목록에 없음)") : utf8(name));
		}
		bool changed = false;
		if (ImGui::BeginCombo(label, current.c_str())) {
			if (ImGui::Selectable(u8"없음 (원래 방식)", id == 0)) {
				id = 0;
				changed = true;
			}
			for (size_t i = 0; i < hangul.atlases.size(); i++) {
				if (hangul.atlases[i].id <= 0) {
					continue;
				}
				string text = to_string(hangul.atlases[i].id) + " - " + utf8(hangul.atlases[i].name);
				if (ImGui::Selectable(text.c_str(), id == hangul.atlases[i].id)) {
					id = hangul.atlases[i].id;
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	void openFileDialog(const char* key, const char* title, const char* filters)
	{
		IGFD::FileDialogConfig config;
		config.path = ".";
		ImGui::SetNextWindowPos(ImVec2(20.0f, 40.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x - 40.0f, ImGui::GetIO().DisplaySize.y - 80.0f), ImGuiCond_Always);
		ImGuiFileDialog::Instance()->OpenDialog(key, title, filters, config);
	}

	// 파일 선택 다이얼로그를 그리고, 확인하면 선택한 경로를 dst 에 넣는다
	void displayFileDialog(const char* key, char* dst, size_t size)
	{
		if (ImGuiFileDialog::Instance()->Display(key, ImGuiWindowFlags_NoResize)) {
			if (ImGuiFileDialog::Instance()->IsOk()) {
				setText(dst, size, ImGuiFileDialog::Instance()->GetFilePathName());
			}
			ImGuiFileDialog::Instance()->Close();
		}
	}

}


namespace {

	// 동그라미 안의 i 아이콘에 마우스를 올리면 설명이 나온다
	void helpMarker(const char* text)
	{
		ImGui::SameLine();
		const float size = ImGui::GetTextLineHeight();
		const ImVec2 pos = ImGui::GetCursorScreenPos();
		ImGui::Dummy(ImVec2(size, size));
		const bool hovered = ImGui::IsItemHovered();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImU32 color = hovered ? IM_COL32(120, 200, 255, 255) : IM_COL32(150, 160, 170, 255);
		const ImVec2 center(pos.x + size * 0.5f, pos.y + size * 0.5f);
		drawList->AddCircle(center, size * 0.46f, color, 20, 1.4f);
		drawList->AddCircleFilled(ImVec2(center.x, center.y - size * 0.19f), size * 0.08f, color, 8);
		drawList->AddLine(ImVec2(center.x, center.y - size * 0.04f), ImVec2(center.x, center.y + size * 0.2f), color, 1.8f);
		if (hovered) {
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
			ImGui::TextUnformatted(text);
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}

	void messageLine(const HangulUiState& h)
	{
		if (!h.message.empty()) {
			ImGui::Separator();
			ImGui::TextColored(h.messageIsError ? ImVec4(1.0f, 0.5f, 0.4f, 1.0f) : ImVec4(0.5f, 1.0f, 0.6f, 1.0f), "%s", h.message.c_str());
		}
	}

	// 줄바꿈 문자를 공백으로 바꿔 한 줄로 보여 준다
	string oneLine(string text)
	{
		for (size_t k = 0; k < text.size(); k++) {
			if (text[k] == '\r' || text[k] == '\n') {
				text[k] = ' ';
			}
		}
		return text;
	}

	// 단계 표시: 끝난 단계는 [v], 아직이면 [ ]
	void stepText(bool done, const string& text)
	{
		ImGui::TextColored(done ? ImVec4(0.5f, 1.0f, 0.6f, 1.0f) : ImVec4(0.85f, 0.85f, 0.85f, 1.0f), "%s %s", done ? "[v]" : "[ ]", text.c_str());
	}

	const char* kLayoutNames[2] = { u8"원래 방식 (글자 수 x 칸 폭)", u8"글자 폭 합산 (한글에 권장)" };

}


// 폰트 아틀라스 만들기 탭
void Dx9Application::renderHangulAtlasTab()
{
	HangulUiState& h = hangul;
	wstring dir = glyphDir(archive);

	// 끝난 만들기 작업 처리
	if (InterlockedCompareExchange(&h.buildDone, 0, 1) == 1) {
		InterlockedExchange(&h.buildRunning, 0);
		if (h.buildOk) {
			setMessage(h, u8"폰트 아틀라스를 만들었습니다. 아래 목록에서 번호를 배정하세요", false);
			loadAtlasList(archive, h);
		}
		else {
			setMessage(h, u8"폰트 아틀라스를 만들지 못했습니다. 폰트 파일과 설정을 확인하세요", true);
		}
	}
	if (!h.atlasesLoaded) {
		loadAtlasList(archive, h);
	}

	ImGui::TextWrapped(u8"폰트 파일(.ttf, .otf)에서 글자 그림을 만들어 '폰트 아틀라스'로 저장합니다. 게임 글꼴이 한글을 갖고 있지 않을 때 필요합니다. 이미 만들어진 비트맵 폰트(BMFont .fnt)도 가져올 수 있습니다.");
	ImGui::Spacing();

	// ---- 새 폰트 아틀라스
	ImGui::SeparatorText(u8"새 폰트 아틀라스 만들기");
	float full = ImGui::GetContentRegionAvail().x;
	ImGui::SetNextItemWidth(full * 0.35f);
	ImGui::InputText(u8"아틀라스 이름", h.atlasName, IM_ARRAYSIZE(h.atlasName));
	helpMarker(u8"저장할 이름입니다. 영문/숫자를 권장합니다 (예: 본문10)");
	ImGui::SetNextItemWidth(full * 0.6f);
	ImGui::InputText(u8"폰트 파일", h.fontPath, IM_ARRAYSIZE(h.fontPath));
	ImGui::SameLine();
	if (ImGui::Button(u8"찾아보기##font")) {
		openFileDialog("HangulFontDlg", u8"폰트 파일(.ttf, .otf) 또는 BMFont(.fnt)를 선택하세요", ".ttf,.otf,.fnt");
	}
	displayFileDialog("HangulFontDlg", h.fontPath, IM_ARRAYSIZE(h.fontPath));
	const bool bm = isBmFontPath(h.fontPath);
	if (bm) {
		ImGui::TextWrapped(u8"BMFont 를 가져옵니다. 글자 그림은 다시 그리지 않고 그대로 옮기므로 글자 크기, 글자 색, 외곽선, 세로 위치는 쓰이지 않습니다. 텍스트 또는 XML 형식 .fnt 와 그 PNG 페이지(같은 폴더)가 필요하고, CP949 로 표현되는 글자만 담습니다.");
	}
	else {
	ImGui::SetNextItemWidth(full * 0.2f);
	ImGui::InputInt(u8"글자 크기(px)", &h.fontSize);
	helpMarker(u8"게임에서 글자 한 줄이 차지하는 높이에 맞추세요. 너무 작으면 획이 뭉개져 읽기 어렵습니다 (한글은 보통 10px 이상).");

	const char* modes[3] = {
		u8"기본: 영문 + 한글 2350자",
		u8"전체: 영문 + 한글 11172자",
		u8"필요한 글자만: 번역문에 쓰인 글자" };
	ImGui::SetNextItemWidth(full * 0.5f);
	ImGui::Combo(u8"포함할 글자", &h.atlasMode, modes, 3);
	helpMarker(u8"기본(Hangul): 일상적인 한글 2350자 + 기호. 대부분의 번역에 충분합니다.\n전체(ExtendedHangul): 현대 한글 11172자 모두. 용량이 큽니다.\n필요한 글자만(LimitedHangul): 번역문(text, script/object)에서 실제로 쓴 글자만. 가장 작습니다. 번역을 고치면 다시 만들어야 합니다.");
	if (h.atlasMode == 2) {
		ImGui::TextDisabled(u8"먼저 [1.추출]로 text 와 script 를 추출하고 번역한 뒤에 만드세요. 그 파일들의 글자를 모아 담습니다.");
		ImGui::SetNextItemWidth(full * 0.6f);
		ImGui::InputText(u8"더 넣을 글자", h.extraChars, IM_ARRAYSIZE(h.extraChars));
		helpMarker(u8"번역문에 없지만 나중에 쓸 글자(예: 이름 입력용 글자)를 더 적으세요.");
	}
	ImGui::ColorEdit3(u8"글자 색", h.color, ImGuiColorEditFlags_NoInputs);
	ImGui::SameLine();
	ImGui::ColorEdit3(u8"외곽선 색", h.outlineColor, ImGuiColorEditFlags_NoInputs);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(80.0f);
	ImGui::InputInt(u8"외곽선 두께", &h.outlineSize, 0);
	helpMarker(u8"0 이면 외곽선이 없습니다. 1 이상이면 글자 둘레를 외곽선 색으로 두릅니다 (배경이 복잡할 때 읽기 쉬워집니다).");
	}

	if (ImGui::CollapsingHeader(u8"고급 설정")) {
		ImGui::SetNextItemWidth(80.0f);
		ImGui::InputInt(u8"글자 간격", &h.letterSpacing, 0);
		helpMarker(u8"모든 글자 폭에 더하는 값(px)입니다. 글자가 붙어 보이면 1 로 늘려 보세요.");
		if (!bm) {
			ImGui::SetNextItemWidth(80.0f);
			ImGui::InputInt(u8"세로 위치 보정", &h.dy, 0);
			helpMarker(u8"글자를 칸 안에서 위/아래로 옮깁니다(px). 글자가 위나 아래로 잘리면 조정하세요.");
		}
		ImGui::SetNextItemWidth(80.0f);
		ImGui::InputInt(u8"이미지 폭", &h.atlasWidth, 0);
		helpMarker(u8"폰트 아틀라스 이미지(PNG)의 폭입니다. 보통 그대로 두면 됩니다.");
	}

	// ---- 간격 미리보기: 만들기 전에 글자 간격, 외곽선, 크기를 눈으로 확인한다
	if (ImGui::CollapsingHeader(u8"간격 미리보기", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SetNextItemWidth(full * 0.7f);
		ImGui::InputText(u8"미리보기 문구", h.liveSample, IM_ARRAYSIZE(h.liveSample));
		helpMarker(u8"이 문구를 지금 폼의 값(폰트, 글자 크기, 외곽선, 글자 간격, 세로 위치)으로 그려 줍니다. 번역한 문장을 넣어 보면 실제와 가깝습니다.");
		if (strlen(h.fontPath) == 0) {
			ImGui::TextDisabled(u8"폰트 파일을 정하면 여기서 미리 볼 수 있습니다.");
		}
		else {
			updateLivePreview(pD3dDevice, h);
			if (!h.liveError.empty()) {
				ImGui::TextColored(ImVec4(1.0f, 0.52f, 0.46f, 1.0f), "%s", h.liveError.c_str());
			}
			else if (h.livePreview.texture != NULL) {
				ImGui::SetNextItemWidth(90.0f);
				ImGui::InputInt(u8"상자 폭(px)", &h.previewWidth, 0);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(60.0f);
				ImGui::InputInt(u8"확대 배율", &h.previewScale, 0);
				h.previewWidth = max(16, min(h.previewWidth, 1024));
				h.previewScale = max(1, min(h.previewScale, 8));
				const int fitScale = max(1, (int)((ImGui::GetContentRegionAvail().x - 20.0f) / (float)h.previewWidth));
				drawPreview(pD3dDevice, h.livePreview, h.liveSample, h.previewWidth, min(h.previewScale, fitScale));
				ImGui::TextDisabled(u8"줄 높이 %d px, 글자 %d개", (int)h.livePreview.data.lineHeight, (int)h.livePreview.data.glyphs.size());
			}
		}
	}

	bool canBuild = (strlen(h.atlasName) > 0) && (strlen(h.fontPath) > 0) && (bm || h.fontSize > 0) && !h.buildRunning;
	if (!canBuild) {
		ImGui::BeginDisabled(true);
	}
	if (ImGui::Button(u8"폰트 아틀라스 만들기")) {
		Json::Value cfg(Json::objectValue);
		Json::Value atlasCfg(Json::objectValue);
		atlasCfg["name"] = h.atlasName;
		if (bm) {
			atlasCfg["bmfont"] = h.fontPath;
		}
		else {
			atlasCfg["font"] = h.fontPath;
			atlasCfg["size"] = h.fontSize;
			atlasCfg["mode"] = (h.atlasMode == 2) ? "limitedHangul" : ((h.atlasMode == 1) ? "extendedHangul" : "hangul");
		}
		if (!bm && h.atlasMode == 2) {
			// 설정 파일이 있는 glyph 폴더 기준으로 추출 폴더의 text 와 script/object 를 가리킨다
			Json::Value sources(Json::arrayValue);
			sources.append("../text");
			sources.append("../script/object");
			atlasCfg["textSources"] = sources;
			atlasCfg["extraChars"] = h.extraChars;
		}
		atlasCfg["outlineSize"] = h.outlineSize;
		atlasCfg["letterSpacing"] = h.letterSpacing;
		atlasCfg["dy"] = h.dy;
		atlasCfg["atlasWidth"] = h.atlasWidth;
		atlasCfg["outputDir"] = "atlas";
		Json::Value color(Json::arrayValue);
		Json::Value outline(Json::arrayValue);
		for (int c = 0; c < 3; c++) {
			color.append((int)(h.color[c] * 255.0f + 0.5f));
			outline.append((int)(h.outlineColor[c] * 255.0f + 0.5f));
		}
		atlasCfg["color"] = color;
		atlasCfg["outlineColor"] = outline;
		cfg["fontAtlas"] = atlasCfg;
		createDirectoryRecursively(dir);
		// 같은 설정으로 다시 만들 수 있게 설정 파일을 남긴다 (--font-atlas build <설정> 으로도 쓸 수 있다)
		wstring configPath = dir + L"/build_" + fromUtf8(h.atlasName) + L".json";
		if (writeIntoJson(cfg, configPath)) {
			BuildJob* job = new BuildJob();
			job->hangul = &h;
			job->config = configPath;
			InterlockedExchange(&h.buildRunning, 1);
			InterlockedExchange(&h.buildDone, 0);
			setMessage(h, u8"폰트 아틀라스를 만드는 중입니다...", false);
			CreateThread(NULL, 0, buildThread, job, 0, NULL);
		}
		else {
			setMessage(h, u8"설정 파일을 저장하지 못했습니다", true);
		}
	}
	if (!canBuild) {
		ImGui::EndDisabled();
	}
	if (h.buildRunning) {
		ImGui::SameLine();
		ImGui::TextDisabled(u8"만드는 중...");
	}

	// ---- 만든 폰트 아틀라스 / 번호
	ImGui::SeparatorText(u8"만든 폰트 아틀라스와 번호");
	ImGui::TextWrapped(u8"오브젝트는 번호로 폰트 아틀라스를 가리킵니다. 사용할 폰트 아틀라스에 1 이상의 서로 다른 번호를 붙이고 [번호 저장]을 누르세요.");
	if (ImGui::Button(u8"목록 새로고침")) {
		loadAtlasList(archive, h);
		h.releaseTextures();
	}
	if (h.atlases.empty()) {
		ImGui::TextDisabled(u8"만든 폰트 아틀라스가 없습니다. 위에서 먼저 만드세요");
	}
	else if (ImGui::BeginTable("##atlasTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn(u8"번호", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		ImGui::TableSetupColumn(u8"이름");
		ImGui::TableSetupColumn(u8"글자 수", ImGuiTableColumnFlags_WidthFixed, 70.0f);
		ImGui::TableSetupColumn(u8"줄 높이", ImGuiTableColumnFlags_WidthFixed, 70.0f);
		ImGui::TableSetupColumn(u8"미리보기", ImGuiTableColumnFlags_WidthFixed, 90.0f);
		ImGui::TableHeadersRow();
		for (size_t i = 0; i < h.atlases.size(); i++) {
			HangulAtlasItem& a = h.atlases[i];
			ImGui::TableNextRow();
			ImGui::PushID((int)i);
			ImGui::TableSetColumnIndex(0);
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputInt("##id", &a.id, 0);
			if (a.id < 0) {
				a.id = 0;
			}
			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted(utf8(a.name).c_str());
			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%u", a.glyphCount);
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%u", a.lineHeight);
			ImGui::TableSetColumnIndex(4);
			if (ImGui::SmallButton(u8"보기")) {
				h.previewAtlas = (int)i;
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	if (ImGui::Button(u8"번호 저장")) {
		string error;
		if (saveRegistry(archive, h, error)) {
			h.registrySnap = takeDirSnapshot(glyphDir(archive), L"atlases.json");
			setMessage(h, u8"번호를 저장했습니다 (glyph/atlases.json). 다음은 [나. 오브젝트에 적용]입니다", false);
		}
		else {
			setMessage(h, error, true);
		}
	}
	ImGui::SameLine();
	ImGui::TextDisabled(u8"번호가 0 인 폰트 아틀라스는 게임에 넣지 않습니다");

	// ---- 미리보기
	ImGui::SeparatorText(u8"미리보기");
	if (h.previewAtlas >= 0 && h.previewAtlas < (int)h.atlases.size()) {
		ImGui::Text(u8"폰트 아틀라스: %s", utf8(h.atlases[h.previewAtlas].name).c_str());
		ImGui::InputTextMultiline("##sample", h.sample, IM_ARRAYSIZE(h.sample), ImVec2(ImGui::GetContentRegionAvail().x, 52.0f));
		ImGui::SetNextItemWidth(100.0f);
		ImGui::InputInt(u8"상자 폭(px)", &h.previewWidth, 0);
		helpMarker(u8"게임에서 글이 놓일 상자의 폭입니다. 이 폭을 넘으면 줄이 바뀝니다.");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100.0f);
		ImGui::InputInt(u8"확대 배율", &h.previewScale, 0);
		h.previewWidth = max(16, min(h.previewWidth, 1024));
		h.previewScale = max(1, min(h.previewScale, 8));
		HangulPreviewAtlas* pa = ensurePreviewAtlas(pD3dDevice, archive, h, h.atlases[h.previewAtlas].name);
		if (pa != NULL) {
			drawPreview(pD3dDevice, *pa, h.sample, h.previewWidth, h.previewScale);
			ImGui::TextDisabled(u8"게임 안의 줄바꿈 규칙(글자 폭 합산)을 따른 근사 미리보기입니다");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), u8"폰트 아틀라스를 읽지 못했습니다");
		}
	}
	else {
		ImGui::TextDisabled(u8"목록에서 [보기]를 누르면 예시 문장을 그 폰트 아틀라스로 그려 봅니다");
	}

	messageLine(h);
}


// 오브젝트에 적용 탭
void Dx9Application::renderHangulBlitterTab()
{
	HangulUiState& h = hangul;
	if (!h.atlasesLoaded) {
		loadAtlasList(archive, h);
	}

	wstring base = archiveBase(archive);
	bool hasObjects = dirExists(base + L"/script/object");
	bool hasFlow = fileExists(base + L"/text/textFlow.json");

	ImGui::TextWrapped(u8"텍스트를 그려 주는 오브젝트(Text Blitter. 아래에서는 '글자 오브젝트')마다, 어떤 폰트 아틀라스로 그릴지와 줄 폭을 재는 방식을 정합니다.");
	ImGui::Spacing();

	if (ImGui::Button(u8"글자 오브젝트 불러오기")) {
		if (hasObjects) {
			loadBlitters(archive, h);
			loadAtlasList(archive, h);
			setMessage(h, to_string(h.blitters.size()) + u8"개의 글자 오브젝트를 불러왔습니다", false);
		}
		else {
			setMessage(h, u8"추출한 오브젝트 파일이 없습니다. 오른쪽 [스크립트 + 텍스트 흐름 추출]을 먼저 하세요", true);
		}
	}
	helpMarker(u8"추출 폴더의 script/object 에서 텍스트 블리터(txtblt.mfx) 오브젝트를 읽습니다.");
	ImGui::SameLine();
	if (ImGui::Button(u8"스크립트 + 텍스트 흐름 추출...")) {
		// 지정한 유형 추출 창을 스크립트와 텍스트 흐름만 체크해서 연다
		states.isPartExtractModalShow = true;
		states.totalDataCount = 0;
		states.currentDataIndex = 0;
		states.usingImage = false;
		states.usingImageAndExtractNoneTransparent = false;
		states.usingSound = false;
		states.usingFont = false;
		states.usingModule = false;
		states.usingShader = false;
		states.usingBinaryData = false;
		states.usingText = false;
		states.usingScript = true;
		states.usingFlow = true;
	}
	helpMarker(u8"'텍스트 흐름'은 각 글자 오브젝트가 받는 문구의 목록입니다. 아래 미리보기에서 그 문구를 실제 폰트 아틀라스로 그려 볼 수 있습니다.");
	ImGui::SameLine();
	ImGui::TextDisabled(u8"오브젝트 파일 %s / 텍스트 흐름 %s", hasObjects ? u8"있음" : u8"없음", hasFlow ? u8"있음" : u8"없음");
	if (h.fontReloadKnown) {
		if (h.fontReloadDetected) {
			ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), u8"이 게임은 실행 중에 글꼴 이미지를 다시 불러옵니다 (%u곳, %u개 화면%s%s)", h.fontReloadCount, h.fontReloadFrames, h.fontReloadExample.empty() ? "" : u8", 예: ", h.fontReloadExample.c_str());
			ImGui::TextWrapped(u8"그대로 두면 지정한 폰트 아틀라스가 원본 글꼴 그림으로 덮여 글자가 깨집니다. 아틀라스를 이미지 뱅크에 넣거나(appendImages.json + 오브젝트의 imageHandle), 게임이 불러오는 PNG 를 아틀라스로 바꾸세요. 이 도구가 만든 모듈은 글자표가 있는 오브젝트의 불러오기를 무시합니다.");
		}
		else {
			ImGui::TextDisabled(u8"실행 중 글꼴 이미지 다시 불러오기: 없음 (이미지 뱅크의 아틀라스가 그대로 쓰입니다)");
		}
	}

	if (!h.blittersLoaded) {
		ImGui::Spacing();
		ImGui::TextDisabled(u8"[글자 오브젝트 불러오기]를 눌러 주세요");
		messageLine(h);
		return;
	}

	// ---- 원래 글꼴 이름별 자동 배정
	if (ImGui::CollapsingHeader(u8"원래 글꼴 이름별로 한꺼번에 정하기")) {
		ImGui::TextDisabled(u8"게임은 글꼴 이름을 오브젝트의 문자열 0 에 둡니다. 같은 글꼴을 쓰는 오브젝트들에 같은 폰트 아틀라스를 지정할 때 편리합니다.");
		for (map<string, INT32>::iterator it = h.tableByFont.begin(); it != h.tableByFont.end(); ++it) {
			ImGui::PushID(it->first.c_str());
			ImGui::SetNextItemWidth(260.0f);
			tableCombo(it->first.empty() ? u8"(글꼴 이름 없음)" : it->first.c_str(), h, it->second);
			ImGui::PopID();
		}
	}

	// ---- 목록
	ImGui::SeparatorText(u8"글자 오브젝트 목록");
	ImGui::SetNextItemWidth(220.0f);
	ImGui::InputText(u8"찾기", h.filter, IM_ARRAYSIZE(h.filter));
	helpMarker(u8"이름, 글꼴 이름, 처음 문자열에서 찾습니다.");
	ImGui::SameLine();
	if (ImGui::Button(u8"찾은 항목 모두 체크")) {
		string needle = lowerAscii(h.filter);
		for (size_t i = 0; i < h.blitters.size(); i++) {
			HangulBlitterItem& b = h.blitters[i];
			b.selected = needle.empty() || lowerAscii(b.name).find(needle) != string::npos || lowerAscii(b.font).find(needle) != string::npos || lowerAscii(b.initialText).find(needle) != string::npos;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button(u8"체크 모두 해제")) {
		for (size_t i = 0; i < h.blitters.size(); i++) {
			h.blitters[i].selected = false;
		}
	}

	float tableHeight = max(120.0f, ImGui::GetContentRegionAvail().y * 0.33f);
	if (ImGui::BeginTable("##blitterTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable, ImVec2(0.0f, tableHeight))) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn(u8"체크", ImGuiTableColumnFlags_WidthFixed, 40.0f);
		ImGui::TableSetupColumn(u8"번호", ImGuiTableColumnFlags_WidthFixed, 46.0f);
		ImGui::TableSetupColumn(u8"이름");
		ImGui::TableSetupColumn(u8"원래 글꼴", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		ImGui::TableSetupColumn(u8"아틀라스", ImGuiTableColumnFlags_WidthFixed, 72.0f);
		ImGui::TableSetupColumn(u8"줄 폭 계산", ImGuiTableColumnFlags_WidthFixed, 80.0f);
		ImGui::TableSetupColumn(u8"처음 문자열");
		ImGui::TableHeadersRow();
		string needle = lowerAscii(h.filter);
		for (size_t i = 0; i < h.blitters.size(); i++) {
			HangulBlitterItem& b = h.blitters[i];
			if (!needle.empty() && lowerAscii(b.name).find(needle) == string::npos && lowerAscii(b.font).find(needle) == string::npos && lowerAscii(b.initialText).find(needle) == string::npos) {
				continue;
			}
			ImGui::TableNextRow();
			ImGui::PushID((int)i);
			ImGui::TableSetColumnIndex(0);
			ImGui::Checkbox("##sel", &b.selected);
			ImGui::TableSetColumnIndex(1);
			if (ImGui::Selectable(to_string(b.index).c_str(), h.currentBlitter == (int)i, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
				h.currentBlitter = (int)i;
			}
			ImGui::TableSetColumnIndex(2);
			ImGui::TextUnformatted(b.name.c_str());
			ImGui::TableSetColumnIndex(3);
			ImGui::TextUnformatted(b.font.c_str());
			ImGui::TableSetColumnIndex(4);
			if (b.fontTable > 0) {
				ImGui::Text("%d%s", b.fontTable, b.dirty ? " *" : "");
			}
			else {
				ImGui::Text("%s%s", u8"없음", b.dirty ? " *" : "");
			}
			ImGui::TableSetColumnIndex(5);
			ImGui::TextUnformatted((b.fontTable > 0 && b.pixel) ? u8"폭 합산" : u8"원래");
			ImGui::TableSetColumnIndex(6);
			ImGui::TextUnformatted(oneLine(b.initialText).c_str());
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	ImGui::TextDisabled(u8"번호를 누르면 그 오브젝트의 상세와 문구 미리보기가 오른쪽 패널에 나옵니다. * 는 저장하지 않은 변경입니다.");

	// ---- 적용
	ImGui::SeparatorText(u8"체크한 오브젝트에 적용");
	ImGui::SetNextItemWidth(260.0f);
	tableCombo(u8"폰트 아틀라스##apply", h, h.applyTable);
	helpMarker(u8"'없음'은 원래 방식(게임의 이미지 글꼴)으로 그립니다. 번호는 [가. 폰트 아틀라스 만들기]에서 붙인 번호입니다.");
	ImGui::SetNextItemWidth(260.0f);
	ImGui::Combo(u8"줄 폭 계산", &h.applyLayout, kLayoutNames, 2);
	helpMarker(u8"자동 줄바꿈과 정렬(가운데/오른쪽)을 위해 한 줄의 폭을 재는 방식입니다.\n원래 방식: 글자 수 x 칸 폭. 영문처럼 폭이 같은 글꼴에 맞습니다.\n글자 폭 합산: 글자마다 실제 폭을 더합니다. 한글처럼 폭이 제각각인 글자에서 줄바꿈 위치가 맞고, 공백으로 폭을 맞출 필요가 없습니다.");
	if (ImGui::Button(u8"체크한 오브젝트에 적용")) {
		UINT32 count = 0;
		for (size_t i = 0; i < h.blitters.size(); i++) {
			HangulBlitterItem& b = h.blitters[i];
			if (!b.selected) {
				continue;
			}
			b.fontTable = h.applyTable;
			b.pixel = (h.applyLayout == 1);
			b.dirty = true;
			count++;
		}
		setMessage(h, to_string(count) + u8"개에 적용했습니다 (저장하기 전까지 파일은 바뀌지 않습니다)", false);
	}
	ImGui::SameLine();
	if (ImGui::Button(u8"원래 글꼴 이름별 설정으로 모두 적용")) {
		UINT32 count = 0;
		for (size_t i = 0; i < h.blitters.size(); i++) {
			HangulBlitterItem& b = h.blitters[i];
			map<string, INT32>::iterator it = h.tableByFont.find(b.font);
			if (it == h.tableByFont.end() || it->second <= 0) {
				continue;
			}
			b.fontTable = it->second;
			b.pixel = (h.applyLayout == 1);
			b.dirty = true;
			count++;
		}
		setMessage(h, to_string(count) + u8"개에 글꼴 이름별 폰트 아틀라스를 적용했습니다", false);
	}
	ImGui::SameLine();
	bool anyDirty = false;
	for (size_t i = 0; i < h.blitters.size(); i++) {
		anyDirty = anyDirty || h.blitters[i].dirty;
	}
	if (!anyDirty) {
		ImGui::BeginDisabled(true);
	}
	if (ImGui::Button(u8"파일에 저장")) {
		UINT32 saved = 0;
		if (saveBlitters(h, saved)) {
			h.objectSnap = takeDirSnapshot(archiveBase(archive) + L"/script/object", L"*.json");
			check.objects.clear();
			if (check.objectSnap.valid) {
				check.objectSnap = h.objectSnap;
			}
			setMessage(h, to_string(saved) + u8"개 오브젝트 파일을 저장했습니다. [3.교체 (재합성)]의 '스크립트'로 아카이브에 반영하세요", false);
		}
		else {
			setMessage(h, u8"파일을 저장하지 못했습니다", true);
		}
	}
	if (!anyDirty) {
		ImGui::EndDisabled();
	}
	messageLine(h);

	// 선택한 오브젝트의 상세와 문구 미리보기는 오른쪽 패널(인스펙터, 미리보기)에 나온다
	ImGui::Spacing();
	ImGui::TextDisabled(u8"목록에서 오브젝트를 선택하면 오른쪽 패널에 상세와 문구 미리보기가 나옵니다.");
}


// 게임에 넣기 탭
void Dx9Application::renderHangulModuleTab()
{
	HangulUiState& h = hangul;
	wstring dir = glyphDir(archive);
	if (!h.atlasesLoaded) {
		loadAtlasList(archive, h);
	}

	ImGui::TextWrapped(u8"폰트 아틀라스를 게임의 txtblt.mfx(텍스트 글자 표시 확장 모듈)에 심어, 새 모듈 파일을 만듭니다. 번호를 붙인 폰트 아틀라스가 모두 들어갑니다.");
	ImGui::Spacing();

	ImGui::SeparatorText(u8"새 모듈 만들기");
	float full = ImGui::GetContentRegionAvail().x;
	ImGui::SetNextItemWidth(full * 0.6f);
	ImGui::InputText(u8"원본 txtblt.mfx", h.moduleInput, IM_ARRAYSIZE(h.moduleInput));
	helpMarker(u8"게임에 들어 있는 원래의 txtblt.mfx 입니다 (37376바이트짜리 판만 지원). exe 게임은 [1.추출]에서 '모듈'을 추출하면 추출 폴더의 modules 에 있습니다.");
	ImGui::SameLine();
	if (ImGui::Button(u8"찾아보기##module")) {
		openFileDialog("HangulModuleDlg", u8"원본 txtblt.mfx 를 선택하세요", ".mfx");
	}
	displayFileDialog("HangulModuleDlg", h.moduleInput, IM_ARRAYSIZE(h.moduleInput));
	if (archive->getArchiveType() == CTT::Archive::ArchiveType::ARCHIVE_EXE) {
		ImGui::SameLine();
		if (ImGui::Button(u8"추출한 모듈에서 찾기")) {
			vector<wstring> found = listFiles(archiveBase(archive) + L"/modules", L"*txtblt*.mfx");
			if (!found.empty()) {
				setText(h.moduleInput, sizeof(h.moduleInput), utf8(archiveBase(archive) + L"/modules/" + found[0]));
			}
			else {
				setMessage(h, u8"추출 폴더의 modules 에서 txtblt.mfx 를 찾지 못했습니다. [1.추출]에서 모듈을 먼저 추출하세요", true);
			}
		}
	}
	if (strlen(h.moduleOutput) == 0) {
		setText(h.moduleOutput, sizeof(h.moduleOutput), utf8(dir + L"/txtblt_patched.mfx"));
	}
	ImGui::SetNextItemWidth(full * 0.6f);
	ImGui::InputText(u8"새 모듈 저장 위치", h.moduleOutput, IM_ARRAYSIZE(h.moduleOutput));
	if (strlen(h.moduleInput) == 0) {
		ImGui::BeginDisabled(true);
	}
	if (ImGui::Button(u8"폰트 아틀라스를 심은 모듈 만들기")) {
		string error;
		vector<TxtbltFontAssignment> assignments;
		set<INT32> used;
		bool ok = true;
		for (size_t i = 0; i < h.atlases.size() && ok; i++) {
			if (h.atlases[i].id <= 0) {
				continue;
			}
			if (used.count(h.atlases[i].id) > 0) {
				error = u8"같은 번호를 두 폰트 아틀라스에 붙일 수 없습니다";
				ok = false;
				break;
			}
			used.insert(h.atlases[i].id);
			shared_ptr<vector<unsigned char> > table = FtFont::readFile(atlasDir(archive) + L"/" + h.atlases[i].name + L".kft");
			if (!table) {
				error = u8"폰트 아틀라스 파일(.kft)을 읽지 못했습니다: " + utf8(h.atlases[i].name);
				ok = false;
				break;
			}
			TxtbltFontAssignment a;
			a.id = (UINT32)h.atlases[i].id;
			a.table = *table;
			assignments.push_back(a);
		}
		if (ok && assignments.empty()) {
			error = u8"번호를 붙인 폰트 아틀라스가 없습니다. [가. 폰트 아틀라스 만들기]에서 번호를 저장하세요";
			ok = false;
		}
		shared_ptr<vector<unsigned char> > module;
		if (ok) {
			module = FtFont::readFile(fromUtf8(h.moduleInput));
			if (!module) {
				error = u8"원본 txtblt.mfx 를 읽지 못했습니다";
				ok = false;
			}
		}
		if (ok) {
			wstring patchError;
			if (!patchTxtbltFont(*module, assignments, patchError)) {
				error = u8"모듈을 만들지 못했습니다: " + utf8(patchError);
				ok = false;
			}
		}
		if (ok) {
			wstring outPath = fromUtf8(h.moduleOutput);
			createDirectoryRecursively(getOnlyDirPath(outPath));
			HANDLE file = CreateFileW(outPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			DWORD written = 0;
			if (file == INVALID_HANDLE_VALUE || !WriteFile(file, &(*module)[0], (DWORD)module->size(), &written, NULL) || written != module->size()) {
				error = u8"모듈 파일을 저장하지 못했습니다";
				ok = false;
			}
			if (file != INVALID_HANDLE_VALUE) {
				CloseHandle(file);
			}
		}
		if (ok) {
			setMessage(h, u8"폰트 아틀라스 " + to_string(assignments.size()) + u8"종을 심은 모듈을 저장했습니다: " + h.moduleOutput, false);
		}
		else {
			setMessage(h, error, true);
		}
	}
	if (strlen(h.moduleInput) == 0) {
		ImGui::EndDisabled();
	}
	messageLine(h);

	ImGui::SeparatorText(u8"그다음 할 일");
	ImGui::BulletText(u8"만든 모듈을 게임의 txtblt.mfx 와 바꿔 넣습니다 (같은 이름으로).");
	ImGui::BulletText(u8"exe 게임이면 추출 폴더의 modules 안 파일을 바꾼 뒤 [3.교체 (재합성)]에서 '모듈'을 체크합니다.");
	ImGui::BulletText(u8"dat/ccn 게임이면 게임 폴더의 Data/Modules 등 외부 모듈 파일을 직접 바꿉니다.");
	ImGui::BulletText(u8"[나. 오브젝트에 적용]에서 저장한 파일은 [3.교체 (재합성)]의 '스크립트'로 아카이브에 반영합니다.");
	ImGui::BulletText(u8"이 게임이 실행 중에 글꼴 이미지를 다시 불러온다면(Data/Fonts 등), 그 이미지도 폰트 아틀라스 PNG 로 바꿔야 합니다.");
}


// 한글화 도구 창 (수정 단계): 설명, 진행 상황, 세 탭
void Dx9Application::renderHangulToolContent()
{
	HangulUiState& h = hangul;

	ImGui::TextWrapped(u8"게임 글자를 한글로 그리려면: 가. 폰트 아틀라스를 만들고, 나. 글자를 그리는 오브젝트마다 쓸 폰트 아틀라스를 정한 뒤, 다. 게임 모듈에 심습니다. 바뀐 파일은 재합성 단계에서 아카이브에 반영합니다.");

	// 진행 상황
	if (!h.atlasesLoaded) {
		loadAtlasList(archive, h);
	}
	int assignedAtlases = 0;
	for (size_t i = 0; i < h.atlases.size(); i++) {
		assignedAtlases += (h.atlases[i].id > 0) ? 1 : 0;
	}
	int appliedBlitters = 0;
	for (size_t i = 0; i < h.blitters.size(); i++) {
		appliedBlitters += (h.blitters[i].fontTable > 0) ? 1 : 0;
	}
	wstring outPath = fromUtf8(h.moduleOutput);
	bool moduleMade = (strlen(h.moduleOutput) > 0) && fileExists(outPath);
	stepText(assignedAtlases > 0, u8"가. 폰트 아틀라스 " + to_string(h.atlases.size()) + u8"개 (번호를 붙인 것 " + to_string(assignedAtlases) + u8"개)");
	ImGui::SameLine();
	stepText(appliedBlitters > 0, u8"나. 글자 오브젝트 " + (h.blittersLoaded ? to_string(h.blitters.size()) + u8"개 중 " + to_string(appliedBlitters) + u8"개에 폰트 아틀라스 지정" : string(u8"(아직 불러오지 않음)")));
	ImGui::SameLine();
	stepText(moduleMade, u8"다. 새 모듈 " + string(moduleMade ? u8"만듦" : u8"아직"));
	ImGui::TextDisabled(u8"작업 폴더: %s", utf8(glyphDir(archive)).c_str());

	if (ImGui::CollapsingHeader(u8"용어 설명 (처음이라면 먼저 읽어 보세요)")) {
		ImGui::BulletText(u8"폰트 아틀라스: 폰트에서 만든 글자 그림(PNG)과, 글자마다의 위치와 폭 정보(.kft)를 묶은 것입니다.");
		ImGui::BulletText(u8"폰트 아틀라스 번호: 오브젝트가 어떤 폰트 아틀라스를 쓸지 가리키는 1 이상의 번호입니다.");
		ImGui::BulletText(u8"글자 오브젝트(Text Blitter): 이미지 글꼴로 글을 그리는 확장 오브젝트(txtblt.mfx)입니다. 게임의 메뉴, 대사 상자 등에 쓰입니다.");
		ImGui::BulletText(u8"줄 폭 계산: 자동 줄바꿈과 정렬을 위해 한 줄의 폭을 재는 방식입니다. 한글에는 '글자 폭 합산'이 맞습니다.");
		ImGui::BulletText(u8"텍스트 흐름: 각 글자 오브젝트가 받는 문구의 목록입니다. 번역이 빠진 문구를 찾거나 미리보기에 씁니다.");
		ImGui::BulletText(u8"추출 / 수정 / 재합성: 아카이브에서 파일로 꺼내고, 파일을 고친 뒤, 다시 아카이브에 합쳐 새 파일로 저장합니다.");
	}

	ImGui::Separator();
	if (ImGui::BeginTabBar("##hangulTabs")) {
		ImGuiTabItemFlags flag0 = (h.requestTab == 0) ? ImGuiTabItemFlags_SetSelected : 0;
		ImGuiTabItemFlags flag1 = (h.requestTab == 1) ? ImGuiTabItemFlags_SetSelected : 0;
		ImGuiTabItemFlags flag2 = (h.requestTab == 2) ? ImGuiTabItemFlags_SetSelected : 0;
		h.requestTab = -1;
		if (ImGui::BeginTabItem(u8"가. 폰트 아틀라스 만들기", NULL, flag0)) {
			ImGui::BeginChild("##tab0", ImVec2(0.0f, max(ImGui::GetContentRegionAvail().y, 520.0f)));
			renderHangulAtlasTab();
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(u8"나. 오브젝트에 적용", NULL, flag1)) {
			ImGui::BeginChild("##tab1", ImVec2(0.0f, max(ImGui::GetContentRegionAvail().y, 520.0f)));
			renderHangulBlitterTab();
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(u8"다. 게임에 넣기", NULL, flag2)) {
			ImGui::BeginChild("##tab2", ImVec2(0.0f, max(ImGui::GetContentRegionAvail().y, 520.0f)));
			renderHangulModuleTab();
			ImGui::EndChild();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

}

// 오른쪽 패널의 인스펙터: 선택한 글자 오브젝트의 상세
void Dx9Application::renderHangulBlitterInspector()
{
	HangulUiState& h = hangul;
	if (h.currentBlitter < 0 || h.currentBlitter >= (int)h.blitters.size()) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::TextWrapped("%s", u8"'나. 오브젝트에 적용' 탭에서 글자 오브젝트를 선택하면 상세가 여기에 나옵니다.");
		ImGui::PopStyleColor();
		return;
	}
	const HangulBlitterItem& b = h.blitters[h.currentBlitter];
	ImGui::TextWrapped("%s", b.name.c_str());
	ImGui::TextDisabled(u8"오브젝트 번호 %d", b.index);
	ImGui::Spacing();

	const float labelWidth = 108.0f;
	auto row = [&](const char* label, const string& value) {
		ImGui::TextDisabled("%s", label);
		ImGui::SameLine(labelWidth);
		ImGui::TextWrapped("%s", value.c_str());
	};
	row(u8"원래 글꼴", b.font.empty() ? string(u8"(없음)") : b.font);
	wstring atlasName = atlasNameForId(h, b.fontTable);
	string table = (b.fontTable > 0) ? (to_string(b.fontTable) + (atlasName.empty() ? string() : string(" (") + utf8(atlasName) + ")")) : string(u8"없음 (원래 방식)");
	row(u8"폰트 아틀라스", table);
	row(u8"줄 폭 계산", b.pixel ? string(u8"글자 폭 합산") : string(u8"원래"));
	row(u8"칸 크기", to_string(b.cellWidth) + " x " + to_string(b.cellHeight));
	row(u8"이미지", string(u8"핸들 ") + to_string(b.imageHandle) + ", " + to_string(b.imageWidth) + " x " + to_string(b.imageHeight));
	if (b.dirty) {
		ImGui::TextColored(ImVec4(0.32f, 0.77f, 0.82f, 1.0f), u8"저장하지 않은 변경이 있습니다");
	}

	ImGui::Spacing();
	ImGui::TextDisabled(u8"옵션");
	if (b.flagNames.empty()) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::TextWrapped("%s", u8"(옵션 정보 없음. 스크립트를 다시 추출하세요)");
		ImGui::PopStyleColor();
	}
	// 옵션 이름을 줄바꿈하며 나열한다
	float lineLeft = ImGui::GetCursorPosX();
	float lineWidth = ImGui::GetContentRegionAvail().x;
	float used = 0.0f;
	for (size_t i = 0; i < b.flagNames.size(); i++) {
		const string& name = b.flagNames[i];
		float width = ImGui::CalcTextSize(name.c_str()).x + 18.0f;
		if (used > 0.0f && used + width > lineWidth) {
			used = 0.0f;
			ImGui::SetCursorPosX(lineLeft);
		}
		else if (used > 0.0f) {
			ImGui::SameLine();
		}
		bool known = !(name.size() > 2 && name[0] == '0' && name[1] == 'x');
		ImGui::PushStyleColor(ImGuiCol_Button, known ? ImVec4(0.14f, 0.27f, 0.32f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
		ImGui::SmallButton(name.c_str());
		ImGui::PopStyleColor();
		if (ImGui::IsItemHovered()) {
			const char* tip = NULL;
			if (name == "ESCAPE") { tip = u8"백슬래시 이스케이프 처리 (탭 칸이 0보다 크면 탭 문자도 처리)"; }
			else if (name == "WRAP") { tip = u8"자동 줄바꿈"; }
			else if (name == "CRLF") { tip = u8"CR/LF 를 줄바꿈으로 처리"; }
			else if (name == "CHAR_EVENT") { tip = u8"글자 하나를 그릴 때마다 이벤트를 보냅니다. 글자표가 있는 오브젝트는 이 옵션이 꺼진 것으로 처리됩니다"; }
			else if (name == "BOX") { tip = u8"상자 안에 맞춰 그립니다"; }
			else { tip = u8"이름을 모르는 옵션 비트입니다"; }
			ImGui::SetTooltip("%s", tip);
		}
		used += width + ImGui::GetStyle().ItemSpacing.x;
	}
}


// 오른쪽 패널의 미리보기: 선택한 글자 오브젝트에 나올 수 있는 문구를 정한 폰트 아틀라스로 그린다
void Dx9Application::renderHangulBlitterPreview()
{
	HangulUiState& h = hangul;
	if (h.currentBlitter < 0 || h.currentBlitter >= (int)h.blitters.size()) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::TextWrapped("%s", u8"글자 오브젝트를 선택하면 나올 수 있는 문구를 폰트 아틀라스로 그려 보여 줍니다.");
		ImGui::PopStyleColor();
		return;
	}
	const HangulBlitterItem& b = h.blitters[h.currentBlitter];
	ImGui::SetNextItemWidth(84.0f);
	ImGui::InputInt(u8"상자 폭(px)", &h.previewWidth, 0);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(56.0f);
	ImGui::InputInt(u8"배율", &h.previewScale, 0);
	h.previewWidth = max(16, min(h.previewWidth, 1024));
	h.previewScale = max(1, min(h.previewScale, 8));

	// 패널 폭에 맞게 배율을 줄인다 (정한 배율이 크면 잘리기 때문)
	const int fitScale = max(1, (int)((ImGui::GetContentRegionAvail().x - 20.0f) / (float)h.previewWidth));
	const int drawScale = min(h.previewScale, fitScale);

	wstring atlasName = atlasNameForId(h, b.fontTable);
	HangulPreviewAtlas* pa = atlasName.empty() ? NULL : ensurePreviewAtlas(pD3dDevice, archive, h, atlasName);
	vector<string> texts;
	map<string, vector<string> >::iterator found = h.textsByName.find(b.name);
	if (found != h.textsByName.end()) {
		texts = found->second;
	}
	else if (!b.initialText.empty()) {
		texts.push_back(b.initialText);
	}
	if (pa == NULL) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::TextWrapped("%s", u8"폰트 아틀라스를 지정하면(번호가 있는 폰트 아틀라스) 그 폰트 아틀라스로 그려 보여 줍니다");
		ImGui::PopStyleColor();
	}
	if (texts.empty()) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::TextWrapped("%s", u8"문구가 없습니다 (텍스트 흐름을 추출하면 이벤트가 넣는 문구도 나옵니다)");
		ImGui::PopStyleColor();
	}
	ImGui::BeginChild("##texts", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Border);
	for (size_t i = 0; i < texts.size(); i++) {
		ImGui::PushID((int)i);
		if (pa != NULL) {
			drawPreview(pD3dDevice, *pa, texts[i], h.previewWidth, drawScale);
		}
		ImGui::TextDisabled("%s", oneLine(texts[i]).c_str());
		ImGui::Spacing();
		ImGui::PopID();
	}
	ImGui::EndChild();
}


// 글자 오브젝트의 문자열을 배정한 폰트 아틀라스 글자로 origin(화면 좌표)에 그린다. 아틀라스가 없으면 false
bool Dx9Application::drawBlitterGlyphs(INT32 fontTable, const string& text, ImVec2 origin, float scale, const BlitterLayout& layout, ImVec2& outSize)
{
	HangulUiState& h = hangul;
	if (fontTable <= 0) {
		return false;
	}
	if (!h.atlasesLoaded) {
		loadAtlasList(archive, h);
	}
	wstring atlasName = atlasNameForId(h, fontTable);
	HangulPreviewAtlas* pa = atlasName.empty() ? NULL : ensurePreviewAtlas(pD3dDevice, archive, h, atlasName);
	if (pa == NULL) {
		return false;
	}
	BlitterLayout adjusted = layout;
	adjusted.boxWidth = max(layout.boxWidth, 8);
	drawPreviewAt(pD3dDevice, *pa, text, adjusted, scale, origin, false, &outSize);
	return true;
}


// ---- 도구 밖에서 바뀐 파일 다시 읽기 (Dx9Application_Refresh.cpp 가 부른다)

void Dx9Application::scanHangulFiles(HangulChanges& changes)
{
	HangulUiState& h = hangul;
	const wstring base = archiveBase(archive);
	changes.objects = compareDir(h.objectSnap, base + L"\\script\\object", L"*.json");
	changes.flow = compareDir(h.flowSnap, base + L"\\text", L"textFlow.json");
	// 아틀라스를 만드는 중에는 파일이 반쯤 쓰여 있으므로 건드리지 않는다 (끝나면 목록을 다시 읽으며 기준이 갱신된다)
	if (!h.buildRunning) {
		changes.atlases = compareDir(h.atlasSnap, atlasDir(archive), L"*");
		changes.registry = compareDir(h.registrySnap, glyphDir(archive), L"atlases.json");
	}
}


void Dx9Application::applyHangulFiles(HangulChanges& changes)
{
	HangulUiState& h = hangul;

	// ---- 글자 오브젝트 / 텍스트 흐름
	bool reload = changes.flow.any();
	if (!reload && changes.objects.any()) {
		// 읽어 둔 글자 오브젝트 파일이 바뀌었거나 없어졌거나, 새 파일이 생긴 경우만 다시 읽는다 (다른 오브젝트 파일은 상관없다)
		set<wstring> known;
		for (size_t i = 0; i < h.blitters.size(); i++) {
			size_t slash = h.blitters[i].jsonPath.find_last_of(L"\\/");
			known.insert(slash == wstring::npos ? h.blitters[i].jsonPath : h.blitters[i].jsonPath.substr(slash + 1));
		}
		for (size_t i = 0; i < changes.objects.changed.size(); i++) {
			const wstring& name = changes.objects.changed[i];
			if (known.count(name) > 0 || h.objectSnap.files.find(name) == h.objectSnap.files.end()) {
				reload = true;
			}
		}
	}
	if (reload) {
		bool anyDirty = false;
		for (size_t i = 0; i < h.blitters.size(); i++) {
			anyDirty = anyDirty || h.blitters[i].dirty;
		}
		if (anyDirty) {
			// 저장하지 않은 폰트 아틀라스 지정을 잃지 않도록 다시 읽지 않는다
			addShellLog(ShellLogLevel::Warn, u8"글자 오브젝트 파일이 바뀌었지만, 저장하지 않은 폰트 아틀라스 지정이 있어 다시 읽지 않았습니다. [파일에 저장]을 하거나 [글자 오브젝트 불러오기]를 누르세요");
			h.objectSnap = changes.objects.after;
			h.flowSnap = changes.flow.after.valid ? changes.flow.after : h.flowSnap;
		}
		else {
			INT32 keepIndex = (h.currentBlitter >= 0 && h.currentBlitter < (int)h.blitters.size()) ? h.blitters[h.currentBlitter].index : -1;
			loadBlitters(archive, h);
			for (size_t i = 0; i < h.blitters.size(); i++) {
				if (h.blitters[i].index == keepIndex) {
					h.currentBlitter = (int)i;
				}
			}
			addShellLog(ShellLogLevel::Info, string(u8"글꼴·글자: 글자 오브젝트 ") + to_string(h.blitters.size()) + u8"개를 다시 읽었습니다" + (changes.flow.any() ? u8" (텍스트 흐름이 바뀌었습니다)" : ""));
		}
	}
	else {
		h.objectSnap = changes.objects.after.valid ? changes.objects.after : h.objectSnap;
	}

	// ---- 폰트 아틀라스 파일 / 번호 배정 파일
	if (changes.atlases.any() || changes.registry.any()) {
		// 번호 배정 파일이 그대로면, 아직 저장하지 않은 번호 고르기를 유지한다
		map<wstring, INT32> keepIds;
		if (!changes.registry.any()) {
			for (size_t i = 0; i < h.atlases.size(); i++) {
				keepIds[h.atlases[i].name] = h.atlases[i].id;
			}
		}
		for (map<wstring, HangulPreviewAtlas>::iterator it = h.previewCache.begin(); it != h.previewCache.end(); ++it) {
			if (it->second.texture != NULL) {
				it->second.texture->Release();
				it->second.texture = NULL;
			}
		}
		h.previewCache.clear();
		loadAtlasList(archive, h);
		for (size_t i = 0; i < h.atlases.size(); i++) {
			map<wstring, INT32>::iterator it = keepIds.find(h.atlases[i].name);
			if (it != keepIds.end()) {
				h.atlases[i].id = it->second;
			}
		}
		if (h.previewAtlas >= (int)h.atlases.size()) {
			h.previewAtlas = -1;
		}
		addShellLog(ShellLogLevel::Info, string(u8"글꼴·글자: 폰트 아틀라스 파일이 바뀌어 목록(") + to_string(h.atlases.size()) + u8"개)과 미리보기를 다시 읽었습니다");
	}
}
