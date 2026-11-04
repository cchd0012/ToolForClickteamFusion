#include "Dx9Application.h"
#include "../ImGUI/Headers.h"
#include "../Util/CommonUtil.h"

#include <algorithm>
#include <fstream>
#include <cmath>

using namespace CTT::App;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;
using namespace CTT::JsonUtil;

// 확인 단계: 프레임 배치도. 추출한 폴더의 파일만 읽어 오브젝트를 처음 위치대로 그린다 (Application/CheckUi.h 참고)

CheckUiState::CheckUiState()
{
	loaded = false;
	selectedFrame = -1;
	layoutLoaded = false;
	frameWidth = frameHeight = 0;
	background[0] = background[1] = background[2] = 0;
	selectedInstance = -1;
	zoom = 0;
	showNames = false;
	showImages = true;
	useGlyphs = true;
	hideOverlays = true;
}


CheckUiState::~CheckUiState()
{
	releaseTextures();
}


void CheckUiState::releaseTextures()
{
	for (map<int, CheckImageInfo>::iterator it = images.begin(); it != images.end(); ++it) {
		if (it->second.texture != NULL) {
			it->second.texture->Release();
			it->second.texture = NULL;
		}
	}
}


void CheckUiState::reset()
{
	releaseTextures();
	loaded = false;
	message.clear();
	frames.clear();
	selectedFrame = -1;
	layoutLoaded = false;
	layout = Json::Value();
	frameWidth = frameHeight = 0;
	background[0] = background[1] = background[2] = 0;
	images.clear();
	objects.clear();
	backdrops.clear();
	selectedInstance = -1;
	hiddenObjects.clear();
	frameSnap = DirSnapshot();
	objectSnap = DirSnapshot();
	imageSnap = DirSnapshot();
	backdropSnap = DirSnapshot();
}


namespace {

	const ImVec4 kMutedColor(0.62f, 0.68f, 0.72f, 1.0f);

	wstring checkBase(CTT::Archive::Archive* archive)
	{
		wstring archivePath = archive->getArchivePath();
		return getExtentionRemovedPath(archivePath);
	}

	vector<wstring> listJsonFiles(const wstring& dir)
	{
		vector<wstring> result;
		WIN32_FIND_DATAW data;
		HANDLE handle = FindFirstFileW((dir + L"\\*.json").c_str(), &data);
		if (handle == INVALID_HANDLE_VALUE) {
			return result;
		}
		do {
			if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
				result.push_back(data.cFileName);
			}
		} while (FindNextFileW(handle, &data));
		FindClose(handle);
		sort(result.begin(), result.end());
		return result;
	}

	// 프레임 JSON 은 이벤트 때문에 크다. 이름은 파일 끝쪽에 있으므로 끝만 읽어서 찾는다
	string readFrameNameFromTail(const wstring& path)
	{
		ifstream file(path.c_str(), ios::binary);
		if (!file) {
			return string();
		}
		file.seekg(0, ios::end);
		streamoff size = file.tellg();
		streamoff start = (size > 1200) ? size - 1200 : 0;
		file.seekg(start, ios::beg);
		string tail((size_t)(size - start), '\0');
		file.read(&tail[0], (streamsize)tail.size());
		size_t key = tail.rfind("\"name\"");
		if (key == string::npos) {
			return string();
		}
		size_t open = tail.find('"', key + 6);
		if (open == string::npos) {
			return string();
		}
		size_t close = tail.find('"', open + 1);
		if (close == string::npos) {
			return string();
		}
		return tail.substr(open + 1, close - open - 1);
	}

	bool loadFrameList(CTT::Archive::Archive* archive, CheckUiState& c)
	{
		c.reset();
		wstring base = checkBase(archive);
		// 도구 밖에서 파일이 바뀌었는지 알아내는 기준 (읽는 시점의 상태)
		c.frameSnap = takeDirSnapshot(base + L"\\script\\frame", L"*.json");
		c.objectSnap = takeDirSnapshot(base + L"\\script\\object", L"*.json");
		c.imageSnap = takeDirSnapshot(base + L"\\image", L"*");
		c.backdropSnap = takeDirSnapshot(base + L"\\script", L"backdrops.json");
		vector<wstring> files = listJsonFiles(base + L"\\script\\frame");
		if (files.empty()) {
			c.message = u8"추출한 프레임 스크립트가 없습니다. 추출 단계에서 '스크립트'를 추출하세요.";
			c.loaded = true;
			return false;
		}
		for (size_t i = 0; i < files.size(); i++) {
			CheckFrameItem item;
			item.index = _wtoi(files[i].c_str());
			item.path = base + L"\\script\\frame\\" + files[i];
			item.name = readFrameNameFromTail(item.path);
			c.frames.push_back(item);
		}

		// 그림 목록 (2.5+ 형식만 있다)
		Json::Value list;
		wstring listPath = base + L"\\image\\imageList.json";
		if (GetFileAttributesW(listPath.c_str()) != INVALID_FILE_ATTRIBUTES && readFromJson(list, listPath) && list["images"].isArray()) {
			const Json::Value& images = list["images"];
			for (Json::ArrayIndex i = 0; i < images.size(); i++) {
				CheckImageInfo info;
				info.file = getUniStr(images[i].get("file", "").asString(), CodePage::CP_UTF_8);
				info.width = images[i].get("width", 0).asInt();
				info.height = images[i].get("height", 0).asInt();
				info.hotX = images[i].get("hotspotX", 0).asInt();
				info.hotY = images[i].get("hotspotY", 0).asInt();
				c.images[images[i].get("handle", 0).asInt()] = info;
			}
		}
		else {
			c.message = u8"그림 목록(image/imageList.json)이 없어 그림은 그리지 못하고 상자로만 보여 줍니다 (옛 형식 아카이브는 그림 목록이 없습니다).";
		}
		// 배경 오브젝트(크기, 그림 번호). 배경은 오브젝트 JSON 이 없어서 따로 읽는다
		Json::Value backdrops;
		wstring backdropPath = base + L"\\script\\backdrops.json";
		if (GetFileAttributesW(backdropPath.c_str()) != INVALID_FILE_ATTRIBUTES && readFromJson(backdrops, backdropPath) && backdrops["backdrops"].isArray()) {
			const Json::Value& list = backdrops["backdrops"];
			for (Json::ArrayIndex i = 0; i < list.size(); i++) {
				CheckObjectInfo info;
				info.found = true;
				info.isBackdrop = true;
				info.name = list[i].get("name", "").asString();
				info.objectType = list[i].get("objectType", 1).asInt();
				info.imageHandle = list[i].get("imageHandle", 0).asInt();
				info.width = list[i].get("width", 0).asInt();
				info.height = list[i].get("height", 0).asInt();
				c.backdrops[list[i].get("index", 0).asInt()] = info;
			}
		}
		c.loaded = true;
		return true;
	}

	bool loadFrameLayout(CheckUiState& c, int position)
	{
		c.layoutLoaded = false;
		c.layout = Json::Value();
		c.selectedInstance = -1;
		c.selectedFrame = position;
		if (position < 0 || position >= (int)c.frames.size()) {
			return false;
		}
		Json::Value root;
		wstring path = c.frames[position].path;
		if (!readFromJson(root, path)) {
			return false;
		}
		const Json::Value& layout = root["layout"];
		if (!layout.isObject() || !layout["instances"].isArray()) {
			return false;
		}
		c.layout = layout;
		c.frameWidth = layout.get("width", 0).asInt();
		c.frameHeight = layout.get("height", 0).asInt();
		const Json::Value& bg = layout["background"];
		for (int i = 0; i < 3; i++) {
			c.background[i] = (bg.isArray() && bg.size() > (Json::ArrayIndex)i) ? bg[i].asInt() : 0;
		}
		c.layoutLoaded = (c.frameWidth > 0 && c.frameHeight > 0);
		return c.layoutLoaded;
	}

	// 오브젝트 JSON 은 필요할 때 한 번만 읽는다 (파일 이름이 오브젝트 번호다)
	const CheckObjectInfo& getObjectInfo(const wstring& base, CheckUiState& c, int objectInfo)
	{
		map<int, CheckObjectInfo>::iterator found = c.objects.find(objectInfo);
		if (found != c.objects.end()) {
			return found->second;
		}
		CheckObjectInfo& info = c.objects[objectInfo];
		map<int, CheckObjectInfo>::const_iterator backdrop = c.backdrops.find(objectInfo);
		if (backdrop != c.backdrops.end()) {
			info = backdrop->second;
			return info;
		}
		wchar_t fileName[32];
		swprintf_s(fileName, L"%06d.json", objectInfo);
		wstring path = base + L"\\script\\object\\" + fileName;
		Json::Value root;
		if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES || !readFromJson(root, path)) {
			return info;
		}
		info.found = true;
		info.name = root.get("name", "").asString();
		info.identifier = root.get("identifier", "").asString();
		info.objectType = root.get("objectType", 0).asInt();
		const Json::Value& animations = root["animations"];
		if (animations.isArray() && animations.size() > 0) {
			const Json::Value& directions = animations[0]["directions"];
			if (directions.isArray() && directions.size() > 0) {
				const Json::Value& frames = directions[0]["frames"];
				if (frames.isArray() && frames.size() > 0) {
					info.imageHandle = frames[0].asInt();
				}
			}
		}
		if (root.isMember("text") && root["text"].isObject()) {
			info.width = root["text"].get("width", 0).asInt();
			info.height = root["text"].get("height", 0).asInt();
		}
		if (root.isMember("textBlitter") && root["textBlitter"].isObject()) {
			const Json::Value& blitter = root["textBlitter"];
			info.isBlitter = true;
			info.blitterText = blitter.get("text", "").asString();
			info.fontTable = blitter.get("fontTable", 0).asInt();
			info.flags = blitter.get("flags", 0).asUInt();
			info.boxWidth = blitter.get("imageWidth", 0).asInt();
			info.boxHeight = blitter.get("imageHeight", 0).asInt();
			info.cellWidth = blitter.get("cellWidth", 8).asInt();
			info.align = blitter.get("align", 0).asInt();
			info.pixelLayout = (blitter.get("layout", "byte").asString() == "pixel");
			const Json::Value& flagNames = blitter["flagNames"];
			for (Json::ArrayIndex i = 0; i < flagNames.size(); i++) {
				info.flagNames.push_back(flagNames[i].asString());
			}
		}
		return info;
	}

	LPDIRECT3DTEXTURE9 ensureImageTexture(LPDIRECT3DDEVICE9 device, const wstring& base, CheckImageInfo& info)
	{
		if (info.triedLoad) {
			return info.texture;
		}
		info.triedLoad = true;
		if (info.file.empty()) {
			return NULL;
		}
		wstring path = base + L"\\image\\" + info.file;
		LPDIRECT3DTEXTURE9 texture = NULL;
		if (FAILED(D3DXCreateTextureFromFileExW(device, path.c_str(), D3DX_DEFAULT_NONPOW2, D3DX_DEFAULT_NONPOW2, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
			D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, NULL, NULL, &texture))) {
			return NULL;
		}
		info.texture = texture;
		return texture;
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

	string withoutCarriageReturn(string text)
	{
		string result;
		for (size_t i = 0; i < text.size(); i++) {
			if (text[i] != '\r') {
				result += text[i];
			}
		}
		return result;
	}

	wstring lastPathPart(const wstring& path)
	{
		size_t slash = path.find_last_of(L"\\/");
		return (slash == wstring::npos) ? path : path.substr(slash + 1);
	}

	struct PlacedBox
	{
		int instance;
		ImVec2 minPos;
		ImVec2 maxPos;
	};

}


void Dx9Application::renderShellStepCheck()
{
	CheckUiState& c = check;
	if (!c.loaded) {
		loadFrameList(archive, c);
	}
	const wstring base = checkBase(archive);

	if (c.frames.empty()) {
		ImGui::TextWrapped("%s", c.message.c_str());
		return;
	}

	// 왼쪽: 프레임 목록
	const float listWidth = ImGui::GetFontSize() * 13.0f;
	ImGui::BeginChild("##checkframes", ImVec2(listWidth, 0.0f), true);
	for (size_t i = 0; i < c.frames.size(); i++) {
		char label[256];
		sprintf_s(label, "%d  %s###frame%d", c.frames[i].index, c.frames[i].name.c_str(), (int)i);
		if (ImGui::Selectable(label, c.selectedFrame == (int)i)) {
			loadFrameLayout(c, (int)i);
		}
	}
	ImGui::EndChild();
	ImGui::SameLine();

	// 오른쪽: 도구 줄과 배치도
	ImGui::BeginChild("##checkcanvas", ImVec2(0.0f, 0.0f), false);
	const char* zoomNames[5] = { u8"맞춤", "1x", "2x", "3x", "4x" };
	ImGui::SetNextItemWidth(80.0f);
	if (ImGui::BeginCombo(u8"배율", zoomNames[c.zoom])) {
		for (int z = 0; z < 5; z++) {
			if (ImGui::Selectable(zoomNames[z], c.zoom == z)) {
				c.zoom = z;
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::Checkbox(u8"이름 표시", &c.showNames);
	ImGui::SameLine();
	ImGui::Checkbox(u8"그림 표시", &c.showImages);
	ImGui::SameLine();
	ImGui::Checkbox(u8"폰트 아틀라스 글자", &c.useGlyphs);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(u8"글자 오브젝트에 폰트 아틀라스가 배정되어 있으면 그 글자로 그립니다.\n끄면 도구의 글꼴로 대신 그립니다");
	}
	ImGui::SameLine();
	ImGui::Checkbox(u8"덮개 숨김", &c.hideOverlays);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(u8"맨 위에 그려지는 전체 화면 그림(페이드 덮개 등)을 숨깁니다.\n이벤트가 걷어내는 것이라 처음 배치도에서는 화면을 가립니다");
	}
	ImGui::SameLine();
	ImGui::TextDisabled(u8"처음 배치입니다 (이벤트는 실행하지 않습니다)");

	if (c.selectedFrame < 0) {
		ImGui::Spacing();
		ImGui::TextDisabled(u8"왼쪽에서 프레임을 고르세요.");
		if (!c.message.empty()) {
			ImGui::TextWrapped("%s", c.message.c_str());
		}
		ImGui::EndChild();
		return;
	}
	if (!c.layoutLoaded) {
		ImGui::Spacing();
		ImGui::TextWrapped(u8"이 프레임의 배치 정보(layout)가 없습니다. 이 도구의 이전 버전으로 추출한 스크립트일 수 있으니 스크립트를 다시 추출하세요.");
		ImGui::EndChild();
		return;
	}
	if (!c.message.empty()) {
		ImGui::TextDisabled("%s", c.message.c_str());
	}

	ImVec2 avail = ImGui::GetContentRegionAvail();
	ImVec2 canvasPos = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("##canvasarea", ImVec2(max(avail.x, 1.0f), max(avail.y, 1.0f)));
	const bool canvasHovered = ImGui::IsItemHovered();
	const bool canvasClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

	float scale = (float)c.zoom;
	if (c.zoom == 0) {
		float fit = min((avail.x - 8.0f) / (float)c.frameWidth, (avail.y - 8.0f) / (float)c.frameHeight);
		scale = (fit >= 1.0f) ? floorf(fit) : max(fit, 0.2f);
	}
	ImVec2 origin(canvasPos.x + 4.0f, canvasPos.y + 4.0f);
	ImVec2 frameMax(origin.x + c.frameWidth * scale, origin.y + c.frameHeight * scale);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(origin, frameMax, IM_COL32(c.background[0], c.background[1], c.background[2], 255));

	// 레이어 순서(같으면 인스턴스 순서)로 그린다
	const Json::Value& instances = c.layout["instances"];
	vector<int> order;
	for (Json::ArrayIndex i = 0; i < instances.size(); i++) {
		order.push_back((int)i);
	}
	stable_sort(order.begin(), order.end(), [&](int a, int b) { return instances[(Json::ArrayIndex)a].get("layer", 0).asInt() < instances[(Json::ArrayIndex)b].get("layer", 0).asInt(); });

	drawList->PushClipRect(origin, frameMax, true);
	vector<PlacedBox> placed;
	ImFont* font = ImGui::GetFont();
	for (size_t n = 0; n < order.size(); n++) {
		const Json::Value& instance = instances[(Json::ArrayIndex)order[n]];
		const int x = instance.get("x", 0).asInt();
		const int y = instance.get("y", 0).asInt();
		const int objectNumber = instance.get("objectInfo", 0).asInt();
		const CheckObjectInfo& object = getObjectInfo(base, c, objectNumber);
		if (c.hiddenObjects.count(objectNumber) > 0) {
			continue;
		}
		// 맨 위쪽에 그려지는 전체 화면 그림(페이드 덮개 등)은 이벤트가 걷어내므로 처음 배치도에서는 숨긴다
		if (c.hideOverlays && object.found && object.imageHandle > 0 && n + 10 >= order.size()) {
			map<int, CheckImageInfo>::iterator overlay = c.images.find(object.imageHandle);
			if (overlay != c.images.end() && overlay->second.width >= c.frameWidth && overlay->second.height >= c.frameHeight) {
				continue;
			}
		}

		ImVec2 minPos, maxPos;
		bool drewImage = false;
		if (object.found && object.imageHandle > 0) {
			map<int, CheckImageInfo>::iterator img = c.images.find(object.imageHandle);
			if (img != c.images.end()) {
				minPos = ImVec2(origin.x + (x - (object.isBackdrop ? 0 : img->second.hotX)) * scale, origin.y + (y - (object.isBackdrop ? 0 : img->second.hotY)) * scale);
				maxPos = ImVec2(minPos.x + img->second.width * scale, minPos.y + img->second.height * scale);
				LPDIRECT3DTEXTURE9 texture = c.showImages ? ensureImageTexture(pD3dDevice, base, img->second) : NULL;
				if (texture != NULL) {
					drawList->AddCallback(setPointSampler, pD3dDevice);
					drawList->AddImage((ImTextureID)texture, minPos, maxPos);
					drawList->AddCallback(setLinearSampler, pD3dDevice);
					drewImage = true;
				}
				else {
					drawList->AddRect(minPos, maxPos, IM_COL32(255, 200, 87, 200));
				}
			}
		}
		if (!drewImage && !(object.found && object.imageHandle > 0 && c.images.count(object.imageHandle) > 0)) {
			if (object.isBlitter && !object.blitterText.empty()) {
				// 글자 오브젝트: 처음 문자열을 오브젝트의 줄 폭, 자동 줄바꿈, 정렬대로 그린다 (칸 크기를 몰라 글자 크기는 근사)
				minPos = ImVec2(origin.x + x * scale, origin.y + y * scale);
				BlitterLayout layout;
				layout.boxWidth = (object.boxWidth > 0) ? object.boxWidth : max(c.frameWidth - x, 8);
				layout.wrap = ((object.flags & 0x20) != 0);
				layout.align = object.align;
				layout.pixel = object.pixelLayout;
				layout.cellWidth = max(object.cellWidth, 1);
				layout.showBox = true;
				ImVec2 glyphSize;
				if (c.useGlyphs && drawBlitterGlyphs(object.fontTable, object.blitterText, minPos, scale, layout, glyphSize)) {
					maxPos = ImVec2(minPos.x + glyphSize.x, minPos.y + glyphSize.y);
				}
				else {
					const float fontSize = max(8.0f * scale, 6.0f);
					string text = withoutCarriageReturn(object.blitterText);
					ImVec2 extent = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str());
					float offsetX = 0.0f;
					const float room = layout.boxWidth * scale - extent.x;
					if (object.align != 0 && room > 0.0f) {
						offsetX = (object.align == 1) ? room * 0.5f : room;
					}
					maxPos = ImVec2(minPos.x + max(layout.boxWidth * scale, extent.x), minPos.y + extent.y);
					drawList->AddText(font, fontSize, ImVec2(minPos.x + offsetX, minPos.y), IM_COL32(235, 240, 255, 255), text.c_str());
				}
				drawList->AddRect(minPos, maxPos, IM_COL32(82, 196, 210, 140));
			}
			else if (object.width > 0 && object.height > 0) {
				minPos = ImVec2(origin.x + x * scale, origin.y + y * scale);
				maxPos = ImVec2(minPos.x + object.width * scale, minPos.y + object.height * scale);
				drawList->AddRect(minPos, maxPos, IM_COL32(180, 140, 255, 200));
			}
			else {
				// 크기를 모르는 오브젝트(배경, 확장 오브젝트 등)는 작은 표식으로 둔다
				const float marker = max(10.0f * scale, 8.0f);
				minPos = ImVec2(origin.x + x * scale, origin.y + y * scale);
				maxPos = ImVec2(minPos.x + marker, minPos.y + marker);
				drawList->AddRect(minPos, maxPos, object.found ? IM_COL32(255, 200, 87, 180) : IM_COL32(150, 150, 150, 140));
			}
		}
		if (c.showNames && !object.name.empty() && !drewImage) {
			drawList->AddText(font, 11.0f, ImVec2(minPos.x + 2.0f, minPos.y - 12.0f), IM_COL32(170, 220, 255, 220), object.name.c_str());
		}
		PlacedBox box;
		box.instance = order[n];
		box.minPos = minPos;
		box.maxPos = maxPos;
		placed.push_back(box);
	}

	// 마우스 아래의 가장 위 오브젝트: 강조, 설명, 선택
	int hovered = -1;
	if (canvasHovered) {
		ImVec2 mouse = ImGui::GetMousePos();
		for (size_t i = placed.size(); i > 0; i--) {
			const PlacedBox& b = placed[i - 1];
			if (mouse.x >= b.minPos.x && mouse.x <= b.maxPos.x && mouse.y >= b.minPos.y && mouse.y <= b.maxPos.y) {
				hovered = b.instance;
				break;
			}
		}
	}
	for (size_t i = 0; i < placed.size(); i++) {
		if (placed[i].instance == hovered || placed[i].instance == c.selectedInstance) {
			drawList->AddRect(placed[i].minPos, placed[i].maxPos, (placed[i].instance == c.selectedInstance) ? IM_COL32(82, 196, 210, 255) : IM_COL32(255, 255, 255, 160), 0.0f, 0, 2.0f);
		}
	}
	drawList->PopClipRect();
	drawList->AddRect(origin, frameMax, IM_COL32(120, 140, 150, 255));

	if (canvasClicked) {
		c.selectedInstance = hovered;
	}
	if (hovered >= 0) {
		const Json::Value& instance = instances[(Json::ArrayIndex)hovered];
		const CheckObjectInfo& object = getObjectInfo(base, c, instance.get("objectInfo", 0).asInt());
		ImGui::SetTooltip("%s  (%d, %d)", object.name.empty() ? u8"(이름 없음)" : object.name.c_str(), instance.get("x", 0).asInt(), instance.get("y", 0).asInt());
	}
	ImGui::EndChild();
}


// 오른쪽 패널(확인 단계): 선택한 인스턴스의 상세
void Dx9Application::renderCheckInspector()
{
	CheckUiState& c = check;
	if (!c.layoutLoaded || c.selectedInstance < 0 || c.selectedInstance >= (int)c.layout["instances"].size()) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		ImGui::TextWrapped("%s", u8"배치도에서 오브젝트를 누르면 상세가 여기에 나옵니다.");
		ImGui::PopStyleColor();
		return;
	}
	const Json::Value& instance = c.layout["instances"][(Json::ArrayIndex)c.selectedInstance];
	const int objectInfo = instance.get("objectInfo", 0).asInt();
	const CheckObjectInfo& object = getObjectInfo(checkBase(archive), c, objectInfo);

	ImGui::TextWrapped("%s", object.name.empty() ? u8"(이름 없음)" : object.name.c_str());
	ImGui::TextDisabled(u8"오브젝트 번호 %d", objectInfo);
	ImGui::Spacing();
	{
		bool hidden = c.hiddenObjects.count(objectInfo) > 0;
		if (ImGui::Button(hidden ? u8"이 오브젝트 보이기" : u8"이 오브젝트 숨기기")) {
			if (hidden) {
				c.hiddenObjects.erase(objectInfo);
			}
			else {
				c.hiddenObjects.insert(objectInfo);
			}
		}
		if (!c.hiddenObjects.empty()) {
			ImGui::SameLine();
			if (ImGui::Button(u8"숨김 모두 해제")) {
				c.hiddenObjects.clear();
			}
		}
		ImGui::Spacing();
	}
	const float labelWidth = 108.0f;
	auto row = [&](const char* label, const string& value) {
		ImGui::TextDisabled("%s", label);
		ImGui::SameLine(labelWidth);
		ImGui::TextWrapped("%s", value.c_str());
	};
	if (object.found) {
		row(u8"종류", object.identifier + " (" + to_string(object.objectType) + ")");
	}
	else {
		row(u8"종류", string(u8"배경 등 (오브젝트 JSON 없음)"));
	}
	row(u8"위치", to_string(instance.get("x", 0).asInt()) + ", " + to_string(instance.get("y", 0).asInt()));
	row(u8"레이어", to_string(instance.get("layer", 0).asInt()));
	if (object.imageHandle > 0) {
		row(u8"그림 핸들", to_string(object.imageHandle));
	}
	if (object.isBlitter) {
		row(u8"글자표", object.fontTable > 0 ? to_string(object.fontTable) : string(u8"없음"));
		if (object.boxWidth > 0) {
			row(u8"줄 폭", to_string(object.boxWidth) + u8" px");
		}
		static const char* alignNames[3] = { u8"왼쪽", u8"가운데", u8"오른쪽" };
		row(u8"정렬", (object.align >= 0 && object.align < 3) ? string(alignNames[object.align]) : to_string(object.align));
		ImGui::Spacing();
		ImGui::TextDisabled(u8"처음 문자열");
		ImGui::TextWrapped("%s", withoutCarriageReturn(object.blitterText).c_str());
		ImGui::Spacing();
		ImGui::TextDisabled(u8"옵션");
		string flags;
		for (size_t i = 0; i < object.flagNames.size(); i++) {
			flags += (i > 0 ? ", " : "") + object.flagNames[i];
		}
		ImGui::TextWrapped("%s", flags.empty() ? u8"(정보 없음)" : flags.c_str());
	}
}


// ---- 도구 밖에서 바뀐 파일 다시 읽기 (Dx9Application_Refresh.cpp 가 부른다)

void Dx9Application::scanCheckFiles(CheckChanges& changes)
{
	if (!check.loaded) {
		return;
	}
	const wstring base = checkBase(archive);
	changes.frames = compareDir(check.frameSnap, base + L"\\script\\frame", L"*.json");
	changes.objects = compareDir(check.objectSnap, base + L"\\script\\object", L"*.json");
	changes.images = compareDir(check.imageSnap, base + L"\\image", L"*");
	changes.backdrops = compareDir(check.backdropSnap, base + L"\\script", L"backdrops.json");
}


void Dx9Application::applyCheckFiles(CheckChanges& changes)
{
	CheckUiState& c = check;
	const wstring base = checkBase(archive);

	// 프레임 파일이 늘거나 줄었거나 그림 목록이 바뀌었으면 처음부터 다시 읽는다. 보던 프레임, 고른 인스턴스, 숨긴 오브젝트는 유지한다
	bool imageListChanged = false;
	for (size_t i = 0; i < changes.images.changed.size(); i++) {
		if (changes.images.changed[i] == L"imageList.json") {
			imageListChanged = true;
		}
	}
	if (changes.backdrops.any()) {
		imageListChanged = true;		// 배경 정보도 처음부터 다시 읽는다
	}
	bool frameSetChanged = false;
	if (changes.frames.any()) {
		if (c.frames.size() != changes.frames.after.files.size()) {
			frameSetChanged = true;
		}
		else {
			for (size_t i = 0; i < c.frames.size(); i++) {
				if (changes.frames.after.files.find(lastPathPart(c.frames[i].path)) == changes.frames.after.files.end()) {
					frameSetChanged = true;
				}
			}
		}
	}
	if (frameSetChanged || imageListChanged) {
		int selectedNumber = (c.selectedFrame >= 0 && c.selectedFrame < (int)c.frames.size()) ? c.frames[c.selectedFrame].index : -1;
		int selectedInstance = c.selectedInstance;
		set<int> hidden = c.hiddenObjects;
		loadFrameList(archive, c);
		c.hiddenObjects = hidden;
		for (size_t i = 0; i < c.frames.size(); i++) {
			if (c.frames[i].index == selectedNumber) {
				loadFrameLayout(c, (int)i);
				if (c.layoutLoaded && selectedInstance >= 0 && selectedInstance < (int)c.layout["instances"].size()) {
					c.selectedInstance = selectedInstance;
				}
				break;
			}
		}
		addShellLog(ShellLogLevel::Info, frameSetChanged ? u8"프레임 파일 목록이 바뀌어 확인 화면을 다시 읽었습니다" : u8"그림 목록이나 배경 정보가 바뀌어 확인 화면을 다시 읽었습니다");
		return;
	}

	int frameCount = 0;
	for (size_t k = 0; k < changes.frames.changed.size(); k++) {
		for (size_t i = 0; i < c.frames.size(); i++) {
			if (lastPathPart(c.frames[i].path) != changes.frames.changed[k]) {
				continue;
			}
			c.frames[i].name = readFrameNameFromTail(c.frames[i].path);
			if (c.selectedFrame == (int)i) {
				int selectedInstance = c.selectedInstance;
				loadFrameLayout(c, (int)i);
				if (c.layoutLoaded && selectedInstance >= 0 && selectedInstance < (int)c.layout["instances"].size()) {
					c.selectedInstance = selectedInstance;
				}
			}
			frameCount++;
		}
	}

	int objectCount = 0;
	for (size_t i = 0; i < changes.objects.changed.size(); i++) {
		objectCount += (int)c.objects.erase(_wtoi(changes.objects.changed[i].c_str()));
	}

	int imageCount = 0;
	for (size_t i = 0; i < changes.images.changed.size(); i++) {
		for (map<int, CheckImageInfo>::iterator it = c.images.begin(); it != c.images.end(); ++it) {
			if (it->second.file != changes.images.changed[i]) {
				continue;
			}
			if (it->second.texture != NULL) {
				it->second.texture->Release();
				it->second.texture = NULL;
			}
			it->second.triedLoad = false;
			imageCount++;
		}
	}

	c.frameSnap = changes.frames.after;
	c.objectSnap = changes.objects.after;
	c.imageSnap = changes.images.after;

	if (frameCount + objectCount + imageCount > 0) {
		addShellLog(ShellLogLevel::Info, string(u8"확인 화면: 바뀐 파일을 다시 읽었습니다 (프레임 ") + to_string(frameCount) + u8"개, 오브젝트 " + to_string(objectCount) + u8"개, 그림 " + to_string(imageCount) + u8"개)");
	}
}
