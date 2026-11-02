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
	showAtlasWindow = false;
	showBlitterWindow = false;

	strcpy_s(atlasName, "atlas1");
	memset(fontPath, 0, sizeof(fontPath));
	fontSize = 10;
	atlasMode = 0;
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

	// 문자열을 상자 폭에 맞춰 놓는다. 게임 안의 배치(txtblt 의 layout: pixel)와 같은 규칙의 근사이다:
	// 폭은 글자 폭(advance)의 합, 공백 뒤 단어는 통째로 다음 줄로 넘기고, 단어가 줄보다 길면 글자 단위로 끊는다
	int layoutPreview(const FontAtlasData& atlas, const wstring& text, int boxWidth, vector<PlacedGlyph>& placed)
	{
		struct Local
		{
			static int advanceOf(const FontAtlasData& a, UINT16 code)
			{
				map<UINT16, FontAtlasGlyph>::const_iterator it = a.glyphs.find(code);
				return (it != a.glyphs.end()) ? (int)it->second.advance : (int)a.missingAdvance;
			}
		};

		int line = 0;
		int x = 0;
		size_t i = 0;
		while (i < text.size()) {
			wchar_t ch = text[i];
			if (ch == L'\r') {
				i++;
				continue;
			}
			if (ch == L'\n') {
				line++;
				x = 0;
				i++;
				continue;
			}
			if (ch == L' ') {
				// 줄 맨 앞의 공백은 건너뛴다
				if (x > 0) {
					int adv = Local::advanceOf(atlas, ' ');
					PlacedGlyph g = { (UINT16)' ', x, line };
					placed.push_back(g);
					x += adv;
				}
				i++;
				continue;
			}
			// 단어 하나
			size_t j = i;
			int wordWidth = 0;
			while (j < text.size() && text[j] != L' ' && text[j] != L'\n' && text[j] != L'\r') {
				wordWidth += Local::advanceOf(atlas, toCode(text[j]));
				j++;
			}
			if (x > 0 && x + wordWidth > boxWidth) {
				line++;
				x = 0;
			}
			for (size_t k = i; k < j; k++) {
				UINT16 code = toCode(text[k]);
				int adv = Local::advanceOf(atlas, code);
				if (x > 0 && x + adv > boxWidth) {
					line++;
					x = 0;
				}
				PlacedGlyph g = { code, x, line };
				placed.push_back(g);
				x += adv;
			}
			i = j;
		}
		return line + 1;
	}

	// 글자를 상자 안에 그려서 미리 보여 준다. 반환값은 그린 줄 수
	int drawPreview(LPDIRECT3DDEVICE9 device, HangulPreviewAtlas& pa, const string& utf8Text, int boxWidth, int scale)
	{
		vector<PlacedGlyph> placed;
		int lines = layoutPreview(pa.data, fromUtf8(utf8Text), boxWidth, placed);
		int lineHeight = pa.data.lineHeight;

		ImVec2 origin = ImGui::GetCursorScreenPos();
		ImVec2 size((float)(boxWidth * scale), (float)(lines * lineHeight * scale));
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(32, 80, 56, 255));
		drawList->AddCallback(setPointSampler, device);
		for (size_t k = 0; k < placed.size(); k++) {
			map<UINT16, FontAtlasGlyph>::const_iterator it = pa.data.glyphs.find(placed[k].code);
			if (it == pa.data.glyphs.end() || it->second.width == 0 || it->second.height == 0) {
				continue;
			}
			const FontAtlasGlyph& g = it->second;
			ImVec2 p0(origin.x + (float)((placed[k].x + g.offsetX) * scale), origin.y + (float)(placed[k].line * lineHeight * scale));
			ImVec2 p1(p0.x + (float)(g.width * scale), p0.y + (float)(g.height * scale));
			ImVec2 uv0((float)g.x / pa.data.atlasWidth, (float)g.y / pa.data.atlasHeight);
			ImVec2 uv1((float)(g.x + g.width) / pa.data.atlasWidth, (float)(g.y + g.height) / pa.data.atlasHeight);
			drawList->AddImage((ImTextureID)pa.texture, p0, p1, uv0, uv1);
		}
		drawList->AddCallback(setLinearSampler, device);
		ImGui::Dummy(size);
		return lines;
	}


	// ---- 블리터 목록

	void loadBlitters(CTT::Archive::Archive* archive, HangulUiState& hangul)
	{
		hangul.blitters.clear();
		hangul.textsByName.clear();
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
			}
		}

		for (size_t i = 0; i < hangul.blitters.size(); i++) {
			if (hangul.tableByFont.find(hangul.blitters[i].font) == hangul.tableByFont.end()) {
				hangul.tableByFont[hangul.blitters[i].font] = 0;
			}
		}
		hangul.blittersLoaded = true;
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


// 글자 아틀라스 창
void Dx9Application::renderHangulAtlasWindow()
{
	HangulUiState& h = hangul;
	ImGui::SetNextWindowPos(ImVec2(30.0f, 56.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(640.0f, 520.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(u8"글자 아틀라스", &h.showAtlasWindow)) {
		ImGui::End();
		return;
	}

	ImGui::TextWrapped(u8"작업 흐름: 추출 > 수정(이 창과 '블리터 글자표 배정') > 교체. 폰트에서 글자 아틀라스를 만들고, 번호를 붙여 txtblt.mfx 에 심은 모듈을 만듭니다. 만든 모듈은 게임의 txtblt.mfx 와 바꿔 넣으세요.");

	wstring dir = glyphDir(archive);
	ImGui::TextDisabled(u8"작업 폴더: %s", utf8(dir).c_str());

	// 끝난 만들기 작업 처리
	if (InterlockedCompareExchange(&h.buildDone, 0, 1) == 1) {
		InterlockedExchange(&h.buildRunning, 0);
		if (h.buildOk) {
			setMessage(h, u8"아틀라스를 만들었습니다", false);
			loadAtlasList(archive, h);
		}
		else {
			setMessage(h, u8"아틀라스를 만들지 못했습니다. 폰트 파일과 설정을 확인하세요", true);
		}
	}
	if (!h.atlasesLoaded) {
		loadAtlasList(archive, h);
	}

	// ---- 만들기
	ImGui::SeparatorText(u8"1. 아틀라스 만들기");
	ImGui::SetNextItemWidth(200.0f);
	ImGui::InputText(u8"이름", h.atlasName, IM_ARRAYSIZE(h.atlasName));
	ImGui::SetNextItemWidth(360.0f);
	ImGui::InputText(u8"폰트 파일", h.fontPath, IM_ARRAYSIZE(h.fontPath));
	ImGui::SameLine();
	if (ImGui::Button(u8"찾아보기##font")) {
		openFileDialog("HangulFontDlg", u8"폰트 파일(.ttf, .otf)을 선택하세요", ".ttf,.otf");
	}
	displayFileDialog("HangulFontDlg", h.fontPath, IM_ARRAYSIZE(h.fontPath));
	ImGui::SetNextItemWidth(120.0f);
	ImGui::InputInt(u8"크기(px)", &h.fontSize);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(260.0f);
	const char* modes[2] = { u8"Hangul (ASCII + 한글 2350자)", u8"ExtendedHangul (한글 11172자)" };
	ImGui::Combo(u8"범위", &h.atlasMode, modes, 2);
	ImGui::ColorEdit3(u8"글자 색", h.color, ImGuiColorEditFlags_NoInputs);
	ImGui::SameLine();
	ImGui::ColorEdit3(u8"외곽선 색", h.outlineColor, ImGuiColorEditFlags_NoInputs);
	ImGui::SetNextItemWidth(100.0f);
	ImGui::InputInt(u8"외곽선 두께", &h.outlineSize, 0);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100.0f);
	ImGui::InputInt(u8"글자 간격", &h.letterSpacing, 0);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100.0f);
	ImGui::InputInt(u8"세로 보정", &h.dy, 0);
	ImGui::SetNextItemWidth(100.0f);
	ImGui::InputInt(u8"아틀라스 폭", &h.atlasWidth, 0);

	bool canBuild = (strlen(h.atlasName) > 0) && (strlen(h.fontPath) > 0) && (h.fontSize > 0) && !h.buildRunning;
	if (!canBuild) {
		ImGui::BeginDisabled(true);
	}
	if (ImGui::Button(u8"아틀라스 만들기")) {
		Json::Value cfg(Json::objectValue);
		Json::Value atlasCfg(Json::objectValue);
		atlasCfg["name"] = h.atlasName;
		atlasCfg["font"] = h.fontPath;
		atlasCfg["size"] = h.fontSize;
		atlasCfg["mode"] = (h.atlasMode == 1) ? "extendedHangul" : "hangul";
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
			setMessage(h, u8"아틀라스를 만드는 중입니다...", false);
			CreateThread(NULL, 0, buildThread, job, 0, NULL);
		}
		else {
			setMessage(h, u8"설정 파일을 저장하지 못했습니다", true);
		}
	}
	if (!canBuild) {
		ImGui::EndDisabled();
	}

	// ---- 목록 / 번호
	ImGui::SeparatorText(u8"2. 아틀라스 목록과 글자표 번호");
	if (ImGui::Button(u8"목록 새로고침")) {
		loadAtlasList(archive, h);
		h.releaseTextures();
	}
	if (h.atlases.empty()) {
		ImGui::TextDisabled(u8"만든 아틀라스가 없습니다");
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
	if (ImGui::Button(u8"번호 배정 저장")) {
		string error;
		if (saveRegistry(archive, h, error)) {
			setMessage(h, u8"번호 배정을 저장했습니다 (glyph/atlases.json)", false);
		}
		else {
			setMessage(h, error, true);
		}
	}
	ImGui::SameLine();
	ImGui::TextDisabled(u8"번호 1 이상만 모듈에 심고, 오브젝트의 글자표(fontTable)는 이 번호를 가리킵니다");

	// ---- 미리보기
	ImGui::SeparatorText(u8"3. 미리보기");
	if (h.previewAtlas >= 0 && h.previewAtlas < (int)h.atlases.size()) {
		ImGui::Text(u8"아틀라스: %s", utf8(h.atlases[h.previewAtlas].name).c_str());
		ImGui::InputTextMultiline("##sample", h.sample, IM_ARRAYSIZE(h.sample), ImVec2(ImGui::GetContentRegionAvail().x, 52.0f));
		ImGui::SetNextItemWidth(100.0f);
		ImGui::InputInt(u8"상자 폭(px)", &h.previewWidth, 0);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100.0f);
		ImGui::InputInt(u8"배율", &h.previewScale, 0);
		h.previewWidth = max(16, min(h.previewWidth, 1024));
		h.previewScale = max(1, min(h.previewScale, 8));
		HangulPreviewAtlas* pa = ensurePreviewAtlas(pD3dDevice, archive, h, h.atlases[h.previewAtlas].name);
		if (pa != NULL) {
			drawPreview(pD3dDevice, *pa, h.sample, h.previewWidth, h.previewScale);
			ImGui::TextDisabled(u8"게임 안의 배치(layout: pixel)와 같은 규칙으로 줄을 나눈 근사 미리보기입니다");
		}
		else {
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), u8"아틀라스를 읽지 못했습니다");
		}
	}
	else {
		ImGui::TextDisabled(u8"목록에서 [보기]를 누르면 예시 문장을 이 아틀라스로 그려 봅니다");
	}

	// ---- 모듈 패치
	ImGui::SeparatorText(u8"4. txtblt.mfx 에 글자표 심기");
	ImGui::SetNextItemWidth(360.0f);
	ImGui::InputText(u8"원본 txtblt.mfx", h.moduleInput, IM_ARRAYSIZE(h.moduleInput));
	ImGui::SameLine();
	if (ImGui::Button(u8"찾아보기##module")) {
		openFileDialog("HangulModuleDlg", u8"원본 txtblt.mfx 를 선택하세요", ".mfx");
	}
	displayFileDialog("HangulModuleDlg", h.moduleInput, IM_ARRAYSIZE(h.moduleInput));
	if (archive->getArchiveType() == CTT::Archive::ArchiveType::ARCHIVE_EXE) {
		ImGui::SameLine();
		if (ImGui::Button(u8"추출한 모듈 찾기")) {
			vector<wstring> found = listFiles(archiveBase(archive) + L"/modules", L"*txtblt*.mfx");
			if (!found.empty()) {
				setText(h.moduleInput, sizeof(h.moduleInput), utf8(archiveBase(archive) + L"/modules/" + found[0]));
			}
			else {
				setMessage(h, u8"추출 폴더의 modules 에서 txtblt.mfx 를 찾지 못했습니다. 모듈을 먼저 추출하세요", true);
			}
		}
	}
	if (strlen(h.moduleOutput) == 0) {
		setText(h.moduleOutput, sizeof(h.moduleOutput), utf8(dir + L"/txtblt_patched.mfx"));
	}
	ImGui::SetNextItemWidth(360.0f);
	ImGui::InputText(u8"저장할 파일", h.moduleOutput, IM_ARRAYSIZE(h.moduleOutput));
	if (strlen(h.moduleInput) == 0) {
		ImGui::BeginDisabled(true);
	}
	if (ImGui::Button(u8"글자표를 심은 모듈 만들기")) {
		string error;
		vector<TxtbltFontAssignment> assignments;
		set<INT32> used;
		bool ok = true;
		for (size_t i = 0; i < h.atlases.size() && ok; i++) {
			if (h.atlases[i].id <= 0) {
				continue;
			}
			if (used.count(h.atlases[i].id) > 0) {
				error = u8"같은 번호를 두 아틀라스에 배정할 수 없습니다";
				ok = false;
				break;
			}
			used.insert(h.atlases[i].id);
			shared_ptr<vector<unsigned char> > table = FtFont::readFile(atlasDir(archive) + L"/" + h.atlases[i].name + L".kft");
			if (!table) {
				error = u8"글자표(.kft)를 읽지 못했습니다: " + utf8(h.atlases[i].name);
				ok = false;
				break;
			}
			TxtbltFontAssignment a;
			a.id = (UINT32)h.atlases[i].id;
			a.table = *table;
			assignments.push_back(a);
		}
		if (ok && assignments.empty()) {
			error = u8"번호를 배정한 아틀라스가 없습니다";
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
				error = u8"모듈을 패치하지 못했습니다: " + utf8(patchError);
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
			setMessage(h, u8"글자표 " + to_string(assignments.size()) + u8"종을 심은 모듈을 저장했습니다: " + h.moduleOutput, false);
		}
		else {
			setMessage(h, error, true);
		}
	}
	if (strlen(h.moduleInput) == 0) {
		ImGui::EndDisabled();
	}

	if (!h.message.empty()) {
		ImGui::Separator();
		ImGui::TextColored(h.messageIsError ? ImVec4(1.0f, 0.5f, 0.4f, 1.0f) : ImVec4(0.5f, 1.0f, 0.6f, 1.0f), "%s", h.message.c_str());
	}

	ImGui::End();
}


// 블리터 글자표 배정 창
void Dx9Application::renderHangulBlitterWindow()
{
	HangulUiState& h = hangul;
	ImGui::SetNextWindowPos(ImVec2(20.0f, 48.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(800.0f, 540.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(u8"블리터 글자표 배정", &h.showBlitterWindow)) {
		ImGui::End();
		return;
	}

	ImGui::TextWrapped(u8"작업 흐름: 추출(스크립트 + 텍스트 흐름) > 수정(이 창) > 교체(스크립트). 텍스트 블리터마다 글자표와 줄 폭 계산 방식을 고르면 오브젝트 JSON(script/object)에 저장되고, [교체]의 '스크립트'에 반영됩니다.");

	if (!h.atlasesLoaded) {
		loadAtlasList(archive, h);
	}

	wstring base = archiveBase(archive);
	bool hasObjects = dirExists(base + L"/script/object");
	bool hasFlow = fileExists(base + L"/text/textFlow.json");

	if (ImGui::Button(u8"블리터 불러오기")) {
		if (hasObjects) {
			loadBlitters(archive, h);
			loadAtlasList(archive, h);
			setMessage(h, to_string(h.blitters.size()) + u8"개의 텍스트 블리터를 불러왔습니다", false);
		}
		else {
			setMessage(h, u8"추출한 오브젝트 JSON 이 없습니다. [추출]에서 스크립트를 먼저 추출하세요", true);
		}
	}
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
	ImGui::SameLine();
	ImGui::TextDisabled(u8"오브젝트 JSON %s, 텍스트 흐름 %s", hasObjects ? u8"있음" : u8"없음", hasFlow ? u8"있음" : u8"없음");

	if (!h.blittersLoaded) {
		ImGui::TextDisabled(u8"[블리터 불러오기]를 눌러 주세요");
		if (!h.message.empty()) {
			ImGui::TextColored(h.messageIsError ? ImVec4(1.0f, 0.5f, 0.4f, 1.0f) : ImVec4(0.5f, 1.0f, 0.6f, 1.0f), "%s", h.message.c_str());
		}
		ImGui::End();
		return;
	}

	// ---- 글꼴 이름 규칙 (접어 둔다: 목록과 미리보기를 위한 자리를 아낀다)
	if (ImGui::CollapsingHeader(u8"글꼴 이름별 글자표 (게임이 알터러블 문자열 0 에 글꼴 이름을 둡니다)")) {
		for (map<string, INT32>::iterator it = h.tableByFont.begin(); it != h.tableByFont.end(); ++it) {
			ImGui::PushID(it->first.c_str());
			ImGui::SetNextItemWidth(260.0f);
			tableCombo(it->first.empty() ? u8"(글꼴 이름 없음)" : it->first.c_str(), h, it->second);
			ImGui::PopID();
		}
	}

	// ---- 목록
	ImGui::SeparatorText(u8"텍스트 블리터");
	ImGui::SetNextItemWidth(200.0f);
	ImGui::InputText(u8"찾기", h.filter, IM_ARRAYSIZE(h.filter));
	ImGui::SameLine();
	if (ImGui::Button(u8"보이는 항목 선택")) {
		string needle = lowerAscii(h.filter);
		for (size_t i = 0; i < h.blitters.size(); i++) {
			HangulBlitterItem& b = h.blitters[i];
			b.selected = needle.empty() || lowerAscii(b.name).find(needle) != string::npos || lowerAscii(b.font).find(needle) != string::npos || lowerAscii(b.initialText).find(needle) != string::npos;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button(u8"선택 해제")) {
		for (size_t i = 0; i < h.blitters.size(); i++) {
			h.blitters[i].selected = false;
		}
	}

	if (ImGui::BeginTable("##blitterTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable, ImVec2(0.0f, 150.0f))) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 24.0f);
		ImGui::TableSetupColumn(u8"번호", ImGuiTableColumnFlags_WidthFixed, 46.0f);
		ImGui::TableSetupColumn(u8"이름");
		ImGui::TableSetupColumn(u8"글꼴", ImGuiTableColumnFlags_WidthFixed, 70.0f);
		ImGui::TableSetupColumn(u8"글자표", ImGuiTableColumnFlags_WidthFixed, 60.0f);
		ImGui::TableSetupColumn(u8"배치", ImGuiTableColumnFlags_WidthFixed, 50.0f);
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
				ImGui::Text("%d%s", b.fontTable, b.dirty ? "*" : "");
			}
			else {
				ImGui::Text("-%s", b.dirty ? "*" : "");
			}
			ImGui::TableSetColumnIndex(5);
			ImGui::TextUnformatted((b.fontTable > 0 && b.pixel) ? "pixel" : "byte");
			ImGui::TableSetColumnIndex(6);
			string shown = b.initialText;
			for (size_t k = 0; k < shown.size(); k++) {
				if (shown[k] == '\r' || shown[k] == '\n') {
					shown[k] = ' ';
				}
			}
			ImGui::TextUnformatted(shown.c_str());
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	// ---- 적용
	ImGui::SetNextItemWidth(260.0f);
	tableCombo(u8"글자표##apply", h, h.applyTable);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(110.0f);
	const char* layouts[2] = { u8"byte (원래)", u8"pixel (폭 합산)" };
	ImGui::Combo(u8"배치", &h.applyLayout, layouts, 2);
	if (ImGui::Button(u8"선택한 항목에 적용")) {
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
		setMessage(h, to_string(count) + u8"개 항목에 적용했습니다 (저장하기 전까지 파일은 바뀌지 않습니다)", false);
	}
	ImGui::SameLine();
	if (ImGui::Button(u8"글꼴 이름별 글자표로 모두 배정")) {
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
		setMessage(h, to_string(count) + u8"개 항목에 글꼴 이름별 글자표를 배정했습니다", false);
	}
	ImGui::SameLine();
	bool anyDirty = false;
	for (size_t i = 0; i < h.blitters.size(); i++) {
		anyDirty = anyDirty || h.blitters[i].dirty;
	}
	if (!anyDirty) {
		ImGui::BeginDisabled(true);
	}
	if (ImGui::Button(u8"JSON 에 저장")) {
		UINT32 saved = 0;
		if (saveBlitters(h, saved)) {
			setMessage(h, to_string(saved) + u8"개 오브젝트 JSON 을 저장했습니다. [교체]의 '스크립트'로 아카이브에 반영하세요", false);
		}
		else {
			setMessage(h, u8"JSON 을 저장하지 못했습니다", true);
		}
	}
	if (!anyDirty) {
		ImGui::EndDisabled();
	}

	// ---- 예시 문장 미리보기
	ImGui::SeparatorText(u8"이 블리터에 나올 수 있는 문자열 (배정한 글자표로 미리 보기)");
	if (h.currentBlitter >= 0 && h.currentBlitter < (int)h.blitters.size()) {
		const HangulBlitterItem& b = h.blitters[h.currentBlitter];
		ImGui::Text(u8"%d번 %s (글꼴 %s)", b.index, b.name.c_str(), b.font.c_str());
		ImGui::SetNextItemWidth(100.0f);
		ImGui::InputInt(u8"상자 폭(px)", &h.previewWidth, 0);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100.0f);
		ImGui::InputInt(u8"배율", &h.previewScale, 0);
		h.previewWidth = max(16, min(h.previewWidth, 1024));
		h.previewScale = max(1, min(h.previewScale, 8));

		INT32 table = b.fontTable;
		wstring atlasName = atlasNameForId(h, table);
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
			ImGui::TextDisabled(u8"글자표를 배정하면(번호가 있는 아틀라스) 그 글자표로 그려 보여 줍니다");
		}
		if (texts.empty()) {
			ImGui::TextDisabled(u8"문자열이 없습니다 (텍스트 흐름을 추출하면 이벤트가 넣는 문자열도 나옵니다)");
		}
		ImGui::BeginChild("##texts", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Border);
		for (size_t i = 0; i < texts.size(); i++) {
			ImGui::PushID((int)i);
			if (pa != NULL) {
				drawPreview(pD3dDevice, *pa, texts[i], h.previewWidth, h.previewScale);
			}
			string shown = texts[i];
			for (size_t k = 0; k < shown.size(); k++) {
				if (shown[k] == '\r' || shown[k] == '\n') {
					shown[k] = ' ';
				}
			}
			ImGui::TextDisabled("%s", shown.c_str());
			ImGui::Spacing();
			ImGui::PopID();
		}
		ImGui::EndChild();
	}
	else {
		ImGui::TextDisabled(u8"목록에서 블리터의 번호를 눌러 선택하세요");
	}

	if (!h.message.empty()) {
		ImGui::TextColored(h.messageIsError ? ImVec4(1.0f, 0.5f, 0.4f, 1.0f) : ImVec4(0.5f, 1.0f, 0.6f, 1.0f), "%s", h.message.c_str());
	}

	ImGui::End();
}
