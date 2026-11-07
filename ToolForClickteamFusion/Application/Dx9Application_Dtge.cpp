// dtge (DustTale: The Genocide's End) 0.1.4 한글 빌드 화면.
// 원본 dtge exe, 번역 폴더, 한글 폰트를 고르면 한글 exe 까지 만든다. 실제 작업은 Glyph/DtgeBuilder.cpp 가 한다.
#include "Dx9Application.h"
#include "../Util/CommonUtil.h"

#include <shellapi.h>

using namespace CTT::App;
using namespace CTT::CommonUtil;
using namespace CTT::Dtge;


namespace {

	const ImVec4 kOkColor(0.35f, 0.80f, 0.45f, 1.0f);
	const ImVec4 kWarnColor(0.95f, 0.75f, 0.25f, 1.0f);
	const ImVec4 kBadColor(0.95f, 0.40f, 0.40f, 1.0f);
	const ImVec4 kMutedColor(0.60f, 0.60f, 0.64f, 1.0f);
	const ImVec4 kAccentColor(0.40f, 0.70f, 1.00f, 1.0f);

	const ImGuiWindowFlags kDtgePanelFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

	void copyText(char* dst, size_t size, const string& text)
	{
		strncpy_s(dst, size, text.c_str(), _TRUNCATE);
	}

	wstring wideOf(const char* utf8Text)
	{
		return getUniStr(string(utf8Text), CodePage::CP_UTF_8);
	}

	string utf8Of(const wstring& text)
	{
		return getAnsiStr(text, CodePage::CP_UTF_8);
	}

	wstring exeDirectory()
	{
		wchar_t buffer[MAX_PATH * 2];
		DWORD length = GetModuleFileNameW(NULL, buffer, MAX_PATH * 2);
		wstring path(buffer, length);
		size_t slash = path.find_last_of(L"\\/");
		return (slash == wstring::npos) ? wstring(L".") : path.substr(0, slash);
	}

	wstring settingsPath()
	{
		return exeDirectory() + L"\\dtge_builder.ini";
	}

	bool readTextFile(const wstring& path, string& out)
	{
		HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (h == INVALID_HANDLE_VALUE) {
			return false;
		}
		DWORD size = GetFileSize(h, NULL);
		out.assign(size, '\0');
		DWORD read = 0;
		bool ok = size == 0 || (ReadFile(h, &out[0], size, &read, NULL) != 0 && read == size);
		CloseHandle(h);
		return ok;
	}

	bool dirExistsW(const wstring& path)
	{
		DWORD attr = GetFileAttributesW(path.c_str());
		return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}

	// 저장해 둔 경로를 읽는다 (도구 exe 옆의 dtge_builder.ini, 한 줄에 키=값)
	void loadSettings(DtgeUiState& s)
	{
		string text;
		if (!readTextFile(settingsPath(), text)) {
			return;
		}
		size_t pos = 0;
		while (pos < text.size()) {
			size_t end = text.find('\n', pos);
			if (end == string::npos) {
				end = text.size();
			}
			string line = text.substr(pos, end - pos);
			pos = end + 1;
			while (!line.empty() && (line[line.size() - 1] == '\r')) {
				line.erase(line.size() - 1);
			}
			size_t eq = line.find('=');
			if (eq == string::npos) {
				continue;
			}
			string key = line.substr(0, eq);
			string value = line.substr(eq + 1);
			if (key == "exe") {
				copyText(s.exePath, sizeof(s.exePath), value);
			}
			else if (key == "translation") {
				copyText(s.translationDir, sizeof(s.translationDir), value);
			}
			else if (key == "font") {
				copyText(s.fontDir, sizeof(s.fontDir), value);
			}
			else if (key == "output") {
				copyText(s.outputParent, sizeof(s.outputParent), value);
			}
			else if (key == "image") {
				copyText(s.imageDir, sizeof(s.imageDir), value);
			}
			else if (key == "override") {
				copyText(s.overrideDir, sizeof(s.overrideDir), value);
			}
			else if (key == "nodamage") {
				s.noDamage = (value == "1");
			}
			else if (key.compare(0, 8, "fontslot") == 0) {
				int slot = atoi(key.c_str() + 8);
				if (slot >= 0 && slot < kDtgeFontSlotCount) {
					copyText(s.fontOverride[slot], sizeof(s.fontOverride[slot]), value);
				}
			}
		}
	}

	void saveSettings(const DtgeUiState& s)
	{
		string text;
		text += string("exe=") + s.exePath + "\r\n";
		text += string("translation=") + s.translationDir + "\r\n";
		text += string("font=") + s.fontDir + "\r\n";
		text += string("output=") + s.outputParent + "\r\n";
		text += string("image=") + s.imageDir + "\r\n";
		text += string("override=") + s.overrideDir + "\r\n";
		text += string("nodamage=") + (s.noDamage ? "1" : "0") + "\r\n";
		for (int i = 0; i < kDtgeFontSlotCount; i++) {
			if (s.fontOverride[i][0] != 0) {
				text += string("fontslot") + to_string(i) + "=" + s.fontOverride[i] + "\r\n";
			}
		}
		HANDLE h = CreateFileW(settingsPath().c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (h == INVALID_HANDLE_VALUE) {
			return;
		}
		DWORD written = 0;
		WriteFile(h, text.c_str(), (DWORD)text.size(), &written, NULL);
		CloseHandle(h);
	}

	DtgeOptions makeOptions(const DtgeUiState& s)
	{
		DtgeOptions o;
		o.originalExe = wideOf(s.exePath);
		o.translationDir = wideOf(s.translationDir);
		o.fontDir = wideOf(s.fontDir);
		o.outputParent = wideOf(s.outputParent);
		o.imageDir = wideOf(s.imageDir);
		o.overrideDir = wideOf(s.overrideDir);
		o.noDamage = s.noDamage;
		for (int i = 0; i < kDtgeFontSlotCount; i++) {
			o.fontOverride[i] = wideOf(s.fontOverride[i]);
		}
		return o;
	}

	// 입력이 바뀌었는지 알아보는 열쇠 (점검은 바뀌었거나 시간이 지났을 때만 다시 한다)
	string inputKey(const DtgeUiState& s)
	{
		string key = string(s.exePath) + "|" + s.translationDir + "|" + s.fontDir + "|" + s.outputParent + "|" + s.overrideDir + "|" + s.imageDir;
		for (int i = 0; i < kDtgeFontSlotCount; i++) {
			key += string("|") + s.fontOverride[i];
		}
		return key;
	}

	// ---- 빌드 스레드

	struct RunData
	{
		DtgeUiState* state;
		DtgeOptions options;
	};

	void dtgeLog(void* user, int step, int stateCode, const string& text)
	{
		DtgeUiState* s = (DtgeUiState*)user;
		std::lock_guard<std::mutex> guard(s->lock);
		if (step >= 0 && step < kDtgeStepCount && stateCode >= 1) {
			s->stepState[step] = stateCode;
		}
		string line = (step >= 0) ? string("[") + dtgeStepName(step) + "] " + text : text;
		s->logLines.push_back(line);
		s->logChanged = true;
	}

	DWORD WINAPI dtgeThreadFunction(LPVOID param)
	{
		RunData* data = (RunData*)param;
		wstring exe, dir;
		bool ok = runDtgeBuild(data->options, dtgeLog, data->state, exe, dir);
		{
			std::lock_guard<std::mutex> guard(data->state->lock);
			data->state->success = ok;
			data->state->resultExe = exe;
			data->state->resultDir = dir;
			data->state->finished = true;
			data->state->running = false;
			data->state->logLines.push_back(ok ? string(u8"끝났습니다") : string(u8"중단했습니다. 만들던 결과 폴더는 지우지 않았으니 확인한 뒤 필요 없으면 직접 지우세요"));
			data->state->logChanged = true;
		}
		delete data;
		return 0;
	}

	// ---- 파일/폴더 선택창

	void openPicker(const char* key, const char* title, const char* filters, const char* currentValue)
	{
		IGFD::FileDialogConfig config;
		config.path = ".";
		if (currentValue[0] != 0) {
			wstring value = wideOf(currentValue);
			DWORD attr = GetFileAttributesW(value.c_str());
			if (attr != INVALID_FILE_ATTRIBUTES) {
				if (attr & FILE_ATTRIBUTE_DIRECTORY) {
					config.path = currentValue;
				}
				else {
					size_t slash = value.find_last_of(L"\\/");
					if (slash != wstring::npos && dirExistsW(value.substr(0, slash))) {
						config.path = utf8Of(value.substr(0, slash));
					}
				}
			}
		}
		ImGui::SetNextWindowPos(ImVec2(20.0f, 40.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x - 40.0f, ImGui::GetIO().DisplaySize.y - 80.0f), ImGuiCond_Always);
		ImGuiFileDialog::Instance()->OpenDialog(key, title, filters, config);
	}

	// 선택창을 그리고, 확인하면 고른 경로를 dst 에 넣고 true. 폴더 선택은 고른 폴더(없으면 지금 보던 폴더)
	bool displayPicker(const char* key, char* dst, size_t size, bool pickFolder)
	{
		bool picked = false;
		if (ImGuiFileDialog::Instance()->Display(key, ImGuiWindowFlags_NoResize)) {
			if (ImGuiFileDialog::Instance()->IsOk()) {
				string path = ImGuiFileDialog::Instance()->GetFilePathName();
				if (pickFolder && path.empty()) {
					path = ImGuiFileDialog::Instance()->GetCurrentPath();
				}
				if (!path.empty()) {
					copyText(dst, size, path);
					picked = true;
				}
			}
			ImGuiFileDialog::Instance()->Close();
		}
		return picked;
	}

	// 경로 한 줄: 이름, 입력 칸, [찾기], 점검 결과
	bool pathRow(const char* label, const char* id, char* buffer, size_t size, const char* dialogKey, const char* dialogTitle, const char* filters, const DtgeCheck* check, float width)
	{
		bool changed = false;
		ImGui::Text("%s", label);
		ImGui::PushID(id);
		ImGui::SetNextItemWidth(width - ImGui::GetFontSize() * 6.0f);
		if (ImGui::InputText("##path", buffer, size)) {
			changed = true;
		}
		ImGui::SameLine();
		if (ImGui::Button(u8"찾기...")) {
			openPicker(dialogKey, dialogTitle, filters, buffer);
		}
		ImGui::PopID();
		if (displayPicker(dialogKey, buffer, size, filters == NULL)) {
			changed = true;
		}
		if (check != NULL) {
			const ImVec4& color = (check->level == 2) ? kOkColor : (check->level == 1 ? kWarnColor : kBadColor);
			const char* word = (check->level == 2) ? u8"정상" : (check->level == 1 ? u8"주의" : u8"확인");
			ImGui::TextColored(color, "  %s", word);
			ImGui::SameLine();
			ImGui::TextWrapped("%s", check->text.c_str());
		}
		return changed;
	}

}


void Dx9Application::renderDtgeScreen(float x, float y, float w, float h)
{
	DtgeUiState& d = dtge;
	if (!d.loaded) {
		loadSettings(d);
		d.loaded = true;
	}

	// 끝난 스레드의 핸들을 닫는다
	{
		std::lock_guard<std::mutex> guard(d.lock);
		if (d.finished && d.thread != NULL) {
			CloseHandle(d.thread);
			d.thread = NULL;
		}
	}

	// 입력 점검: 바뀌었거나 0.5초가 지났을 때
	const string key = inputKey(d);
	const DWORD now = GetTickCount();
	if (key != d.checkedKey || now - d.checkedTick > 500 || d.checks.size() != 6) {
		checkDtgeInputs(makeOptions(d), d.checks);
		d.checkedKey = key;
		d.checkedTick = now;
	}

	bool running;
	{
		std::lock_guard<std::mutex> guard(d.lock);
		running = d.running;
	}

	ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 18.0f));
	if (ImGui::Begin("##dtge_screen", NULL, kDtgePanelFlags)) {
		const float inner = ImGui::GetContentRegionAvail().x;

		ImGui::Text(u8"dtge 0.1.4 한글 빌드");
		ImGui::SameLine(inner - ImGui::GetFontSize() * 11.0f);
		ImGui::BeginDisabled(running);
		if (ImGui::Button(u8"일반 도구 화면으로")) {
			saveSettings(d);
			d.active = false;
		}
		ImGui::EndDisabled();
		ImGui::TextDisabled(u8"원본 dtge exe 에서 한글 exe 까지 만듭니다. 결과는 항상 새 폴더에 만들고, 이미 있는 파일과 폴더는 건드리지 않습니다.");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::BeginDisabled(running);
		bool changed = false;
		changed |= pathRow(u8"1. 원본 dtge exe (읽기만 합니다)", "exe", d.exePath, sizeof(d.exePath), "DtgeExeDlg", u8"원본 dtge exe 를 선택하세요", ".exe", &d.checks[0], inner);
		ImGui::Spacing();
		changed |= pathRow(u8"2. 번역 폴더 (frameEventText.json, objectText.json)", "translation", d.translationDir, sizeof(d.translationDir), "DtgeTransDlg", u8"번역 폴더를 선택하세요", NULL, &d.checks[1], inner);
		ImGui::Spacing();
		changed |= pathRow(u8"3. 한글 폰트 폴더 (000000.ttf ~ 000009.ttf)", "font", d.fontDir, sizeof(d.fontDir), "DtgeFontDlg", u8"한글 폰트 폴더를 선택하세요", NULL, &d.checks[2], inner);
		ImGui::Spacing();
		changed |= pathRow(u8"4. 결과를 만들 위치 (이 안에 새 폴더를 만듭니다)", "output", d.outputParent, sizeof(d.outputParent), "DtgeOutDlg", u8"결과를 만들 위치를 선택하세요", NULL, &d.checks[3], inner);
		ImGui::Spacing();
		changed |= pathRow(u8"5. (선택) 고친 스크립트 폴더 - 이 안의 script\\... 파일을 추출 폴더에 덮어씁니다 (image\\ 폴더도 쓰입니다)", "override", d.overrideDir, sizeof(d.overrideDir), "DtgeOverrideDlg", u8"고친 파일이 든 폴더를 선택하세요 (script 폴더나 image 폴더가 든 폴더)", NULL, &d.checks[4], inner);
		ImGui::Spacing();
		changed |= pathRow(u8"6. (선택) 고친 이미지 폴더 - 000414.png 처럼 번호가 이름인 PNG 가 든 폴더", "image", d.imageDir, sizeof(d.imageDir), "DtgeImageDlg", u8"고친 이미지(PNG)가 든 폴더를 선택하세요", NULL, &d.checks[5], inner);
		ImGui::Spacing();

		// 글꼴 지정(고급): 자리마다 쓸 폰트를 직접 고른다. 비워 두면 위의 폰트 폴더의 기본 파일을 쓴다
		if (ImGui::CollapsingHeader(u8"글꼴 지정 (고급) - 자리마다 쓸 폰트를 직접 고릅니다")) {
			ImGui::TextWrapped(u8"비워 두면 3번 폰트 폴더의 기본 파일을 씁니다. '대사 글씨체'는 대사 글자 이미지를 그리는 폰트이므로 여기서 고른 폰트로 대사가 그려집니다. .ttf 파일만 쓸 수 있습니다.");
			ImGui::Spacing();
			for (int i = 0; i < kDtgeFontSlotCount; i++) {
				ImGui::PushID(i);
				ImGui::TextColored(kMutedColor, "%s", dtgeFontSlotRole(i));
				ImGui::SetNextItemWidth(inner - ImGui::GetFontSize() * 12.0f);
				string hint = string(u8"기본: 폰트 폴더의 ") + dtgeFontSlotDefault(i);
				if (ImGui::InputTextWithHint("##slot", hint.c_str(), d.fontOverride[i], sizeof(d.fontOverride[i]))) {
					changed = true;
				}
				ImGui::SameLine();
				if (ImGui::Button(u8"찾기...")) {
					d.pickingSlot = i;
					openPicker("DtgeSlotDlg", u8"이 자리에 쓸 폰트(.ttf)를 선택하세요", ".ttf", d.fontOverride[i]);
				}
				ImGui::SameLine();
				if (ImGui::Button(u8"기본값")) {
					d.fontOverride[i][0] = 0;
					changed = true;
				}
				ImGui::PopID();
			}
			if (d.pickingSlot >= 0 && d.pickingSlot < kDtgeFontSlotCount) {
				if (displayPicker("DtgeSlotDlg", d.fontOverride[d.pickingSlot], sizeof(d.fontOverride[d.pickingSlot]), false)) {
					changed = true;
				}
			}
			ImGui::Spacing();
		}

		ImGui::Checkbox(u8"시험용: HP 무감소판도 같이 만들기", &d.noDamage);
		ImGui::SameLine();
		ImGui::TextDisabled(u8"(한글판 _kor.exe 와 HP 가 닳지 않는 _kor_nodamage.exe 를 둘 다 만듭니다. 시간이 1분쯤 더 걸립니다)");
		ImGui::EndDisabled();
		if (changed) {
			d.checkedTick = 0;
		}

		ImGui::Spacing();
		bool canBuild = !running;
		for (size_t i = 0; i < d.checks.size(); i++) {
			if (d.checks[i].level == 0) {
				canBuild = false;
			}
		}
		ImGui::BeginDisabled(!canBuild);
		if (ImGui::Button(running ? u8"만드는 중..." : u8"만들기", ImVec2(ImGui::GetFontSize() * 9.0f, 0.0f))) {
			saveSettings(d);
			RunData* data = new RunData();
			data->state = &d;
			data->options = makeOptions(d);
			{
				std::lock_guard<std::mutex> guard(d.lock);
				d.running = true;
				d.finished = false;
				d.success = false;
				d.resultExe.clear();
				d.resultDir.clear();
				d.logLines.clear();
				for (int i = 0; i < kDtgeStepCount; i++) {
					d.stepState[i] = 0;
				}
			}
			d.thread = CreateThread(NULL, 0, dtgeThreadFunction, data, 0, NULL);
			if (d.thread == NULL) {
				std::lock_guard<std::mutex> guard(d.lock);
				d.running = false;
				d.logLines.push_back(u8"빌드 스레드를 시작하지 못했습니다");
				delete data;
			}
		}
		ImGui::EndDisabled();
		if (!canBuild && !running) {
			ImGui::SameLine();
			ImGui::TextColored(kBadColor, u8"  '확인' 표시가 있는 항목을 먼저 고치세요");
		}

		// 진행 상태와 로그
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		{
			std::lock_guard<std::mutex> guard(d.lock);
			for (int i = 0; i < kDtgeStepCount; i++) {
				const int st = d.stepState[i];
				const ImVec4& color = (st == 2) ? kOkColor : (st == 3 ? kBadColor : (st == 1 ? kAccentColor : kMutedColor));
				const char* word = (st == 2) ? u8"끝남" : (st == 3 ? u8"실패" : (st == 1 ? u8"진행 중..." : u8"대기"));
				ImGui::TextColored(kMutedColor, "%d", i + 1);
				ImGui::SameLine(ImGui::GetFontSize() * 2.0f);
				ImGui::Text("%s", dtgeStepName(i));
				ImGui::SameLine(ImGui::GetFontSize() * 14.0f);
				ImGui::TextColored(color, "%s", word);
			}
			ImGui::Spacing();

			if (d.finished && d.success) {
				ImGui::TextColored(kOkColor, u8"완성했습니다");
				string resultPath = utf8Of(d.resultExe);
				ImGui::TextWrapped("%s", resultPath.c_str());
				if (ImGui::Button(u8"결과 폴더 열기")) {
					ShellExecuteW(NULL, L"open", d.resultDir.c_str(), NULL, NULL, SW_SHOWNORMAL);
				}
				ImGui::SameLine();
				ImGui::TextDisabled(u8"결과 폴더에는 원본 사본(dtge.exe)과 추출한 파일(dtge 폴더)도 들어 있습니다. 필요 없으면 exe 만 꺼내고 폴더째 지우셔도 됩니다");
			}
			else if (d.finished) {
				ImGui::TextColored(kBadColor, u8"중단했습니다. 아래 로그를 확인하세요");
			}

			float logHeight = ImGui::GetContentRegionAvail().y;
			if (logHeight < ImGui::GetFontSize() * 6.0f) {
				logHeight = ImGui::GetFontSize() * 6.0f;
			}
			ImGui::BeginChild("##dtge_log", ImVec2(0.0f, logHeight), true);
			for (size_t i = 0; i < d.logLines.size(); i++) {
				ImGui::TextWrapped("%s", d.logLines[i].c_str());
			}
			if (d.logChanged) {
				ImGui::SetScrollHereY(1.0f);
				d.logChanged = false;
			}
			ImGui::EndChild();
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
}
