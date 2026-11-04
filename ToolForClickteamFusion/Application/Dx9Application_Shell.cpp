#include "Dx9Application.h"
#include "../ImGUI/Headers.h"
#include "../Util/CommonUtil.h"

#include <shellapi.h>
#include <ctime>
#include <cstdio>

using namespace CTT::App;
using namespace CTT::Archive;

// 화면 틀(껍데기): 왼쪽 단계 목록, 위쪽 상태 줄, 가운데 작업 영역, 아래 문제/로그 패널 (docs/UI_와이어프레임.html)
// 기존 기능은 그대로 두고 단계 화면 안에서 부른다. 한글화 도구는 아직 별도 창으로 열리고(3단계에서 옮긴다),
// 인스펙터/미리보기 패널(오른쪽)은 이후 단계에서 채운다.

namespace {

	const ImVec4 kOkColor(0.38f, 0.82f, 0.55f, 1.0f);
	const ImVec4 kWarnColor(0.94f, 0.71f, 0.33f, 1.0f);
	const ImVec4 kAccentColor(0.32f, 0.77f, 0.82f, 1.0f);
	const ImVec4 kMutedColor(0.62f, 0.68f, 0.72f, 1.0f);
	const ImVec4 kErrorColor(1.0f, 0.52f, 0.46f, 1.0f);

	const ImGuiWindowFlags kPanelFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

	struct StepInfo
	{
		const char* name;
		const char* tip;
	};

	const StepInfo kSteps[SHELL_STEP_COUNT] = {
		{ u8"진단", u8"아카이브의 구조를 분석해서 한글화 진행 방향을 안내합니다 (준비 중)" },
		{ u8"추출", u8"아카이브의 데이터를 파일(PNG, JSON 등)로 꺼냅니다" },
		{ u8"문구", u8"번역할 문자열이 들어 있는 파일을 엽니다" },
		{ u8"글꼴·글자", u8"한글 폰트 아틀라스를 만들고 글자 오브젝트와 게임 모듈에 적용합니다" },
		{ u8"이미지", u8"이미지를 고치거나 추가합니다" },
		{ u8"재합성", u8"고친 파일을 아카이브에 합쳐 새 아카이브(~_new)로 저장합니다" },
		{ u8"확인", u8"프레임 배치도와 번역 글자 미리보기 (준비 중)" },
		{ u8"청크 트리", u8"아카이브의 청크 구조를 봅니다" },
		{ u8"스크립트 JSON", u8"디컴파일한 이벤트와 오브젝트 JSON을 고치고 리컴파일합니다" }
	};

	bool pathExists(const wstring& path, bool wantDirectory)
	{
		DWORD attr = GetFileAttributesW(path.c_str());
		if (attr == INVALID_FILE_ATTRIBUTES) {
			return false;
		}
		bool isDirectory = (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
		return isDirectory == wantDirectory;
	}

	// 둥근 테두리 배지 버튼. 눌렸으면 true
	bool pillButton(const char* id, const char* label, const ImVec4& color)
	{
		ImVec2 textSize = ImGui::CalcTextSize(label);
		ImVec2 pad(10.0f, 3.0f);
		ImVec2 size(textSize.x + pad.x * 2.0f, textSize.y + pad.y * 2.0f);
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImGui::PushID(id);
		bool clicked = ImGui::InvisibleButton("##pill", size);
		bool hovered = ImGui::IsItemHovered();
		ImGui::PopID();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 maxPos(pos.x + size.x, pos.y + size.y);
		ImU32 colorU32 = ImGui::GetColorU32(color);
		if (hovered) {
			ImVec4 fill = color;
			fill.w = 0.18f;
			drawList->AddRectFilled(pos, maxPos, ImGui::GetColorU32(fill), size.y * 0.5f);
		}
		drawList->AddRect(pos, maxPos, colorU32, size.y * 0.5f, 0, 1.2f);
		drawList->AddText(ImVec2(pos.x + pad.x, pos.y + pad.y), colorU32, label);
		return clicked;
	}

	float pillWidth(const char* label)
	{
		return ImGui::CalcTextSize(label).x + 20.0f;
	}

	// 상태 점: 0 아직, 1 끝남, 2 확인 필요, 3 변경 있음
	void statusDot(ImDrawList* drawList, ImVec2 center, int status)
	{
		const float radius = 5.0f;
		if (status == 1) {
			drawList->AddCircleFilled(center, radius, ImGui::GetColorU32(kOkColor));
		}
		else if (status == 2) {
			drawList->AddCircleFilled(center, radius, ImGui::GetColorU32(kWarnColor));
		}
		else if (status == 3) {
			drawList->AddCircleFilled(center, radius, ImGui::GetColorU32(kAccentColor));
		}
		else {
			drawList->AddCircle(center, radius - 1.0f, ImGui::GetColorU32(kMutedColor), 0, 1.6f);
		}
	}

	// 화면에 맞게 경로의 앞을 줄인다 (끝이 중요하다)
	string fitPathTail(const string& text, float maxWidth)
	{
		if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth) {
			return text;
		}
		string tail = text;
		while (tail.size() > 8 && ImGui::CalcTextSize((string("...") + tail).c_str()).x > maxWidth) {
			// UTF-8 문자 경계를 지켜 앞에서 한 글자씩 뺀다
			size_t cut = 1;
			while (cut < tail.size() && ((unsigned char)tail[cut] & 0xC0) == 0x80) {
				cut++;
			}
			tail.erase(0, cut);
		}
		return string("...") + tail;
	}

	string nowTimeText()
	{
		time_t now = time(NULL);
		struct tm local;
		localtime_s(&local, &now);
		char buffer[16];
		strftime(buffer, sizeof(buffer), "%H:%M:%S", &local);
		return buffer;
	}

}


void Dx9Application::addShellLog(ShellLogLevel level, const string& text)
{
	ShellLogLine line;
	line.level = level;
	line.time = nowTimeText();
	line.text = text;
	shell.logs.push_back(line);
	if (shell.logs.size() > 500) {
		shell.logs.erase(shell.logs.begin(), shell.logs.begin() + (shell.logs.size() - 500));
	}
	shell.scrollLogToEnd = true;
}


// 추출 폴더 (아카이브와 같은 위치, 같은 이름)
static wstring shellBaseDir(CTT::Archive::Archive* archive)
{
	wstring archivePath = archive->getArchivePath();
	return CTT::CommonUtil::getExtentionRemovedPath(archivePath);
}


static void openFolder(const wstring& folder)
{
	ShellExecuteW(NULL, L"open", folder.c_str(), NULL, NULL, SW_SHOWNORMAL);
}


// 폴더 열기 버튼: 폴더가 없으면 비활성화하고 이유를 알려 준다
static void folderButton(const wstring& baseDir, const wchar_t* sub, const char* label, const char* tip)
{
	wstring folder = baseDir + L"/" + sub;
	bool exists = pathExists(folder, true);
	if (!exists) {
		ImGui::BeginDisabled(true);
	}
	if (ImGui::Button(label)) {
		openFolder(folder);
	}
	if (!exists) {
		ImGui::EndDisabled();
	}
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		ImGui::SetTooltip(exists ? tip : u8"추출한 폴더가 없습니다. 먼저 '추출'을 하세요");
	}
}


// 단계 점 상태: 0 아직, 1 끝남, 2 확인 필요, 3 변경 있음
int Dx9Application::shellStepStatus(int step)
{
	wstring baseDir = shellBaseDir(archive);
	switch (step) {
		case SHELL_STEP_DIAGNOSE:
			return shell.problems.empty() ? 0 : 2;
		case SHELL_STEP_EXTRACT:
			return pathExists(baseDir, true) ? 1 : 0;
		case SHELL_STEP_REBUILD:
			return states.isDirty ? 3 : 0;
		default:
			return 0;
	}
}


void Dx9Application::renderShell()
{
	ImGuiIO& io = ImGui::GetIO();
	const float width = io.DisplaySize.x;
	const float height = io.DisplaySize.y;
	const float top = (float)states.menuHeight;
	const float fontSize = ImGui::GetFontSize();

	if (!states.isArchiveOpened) {
		renderShellStart(0.0f, top, width, height - top);
		return;
	}

	const float statusHeight = fontSize * 2.4f;
	const float navWidth = fontSize * 12.5f;
	float bottomHeight = fontSize * 2.4f;
	if (shell.bottomOpen) {
		bottomHeight = height * 0.24f;
		if (bottomHeight < fontSize * 9.0f) {
			bottomHeight = fontSize * 9.0f;
		}
		if (bottomHeight > fontSize * 15.0f) {
			bottomHeight = fontSize * 15.0f;
		}
	}
	const float middleTop = top + statusHeight;
	const float middleHeight = height - middleTop - bottomHeight;

	renderShellStatus(0.0f, top, width, statusHeight);
	renderShellNav(0.0f, middleTop, navWidth, middleHeight);
	renderShellWork(navWidth, middleTop, width - navWidth, middleHeight);
	renderShellBottom(0.0f, height - bottomHeight, width, bottomHeight);
}


// 아카이브를 열기 전 첫 화면
void Dx9Application::renderShellStart(float x, float y, float w, float h)
{
	ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 20.0f));
	if (ImGui::Begin("##shell_start", NULL, kPanelFlags)) {
		ImGui::Text(u8"ClickteamFusion 게임을 한글화하는 도구입니다");
		ImGui::Spacing();
		ImGui::TextWrapped(u8"아카이브 파일(.exe, .dat, .ccn)을 열고, 아래 순서로 작업합니다.");
		ImGui::Spacing();
		for (int i = SHELL_STEP_DIAGNOSE; i <= SHELL_STEP_CHECK; i++) {
			ImGui::TextColored(kAccentColor, "%d", i + 1);
			ImGui::SameLine(48.0f);
			ImGui::Text("%s", kSteps[i].name);
			ImGui::SameLine(170.0f);
			ImGui::TextDisabled("%s", kSteps[i].tip);
		}
		ImGui::Spacing();
		ImGui::Spacing();
		if (ImGui::Button(u8"아카이브 열기")) {
			states.isOpenModalShow = true;
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
}


// 위쪽 상태 줄: 열린 파일, 진단 결과, 저장 필요 여부
void Dx9Application::renderShellStatus(float x, float y, float w, float h)
{
	ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 6.0f));
	if (ImGui::Begin("##shell_status", NULL, kPanelFlags | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
		string pathUtf8 = getAnsiStr(archive->getArchivePath(), CodePage::CP_UTF_8);
		size_t slash = pathUtf8.find_last_of("\\/");
		string fileName = (slash == string::npos) ? pathUtf8 : pathUtf8.substr(slash + 1);
		string folderPath = (slash == string::npos) ? string() : pathUtf8.substr(0, slash + 1);

		char diagnoseLabel[64];
		sprintf_s(diagnoseLabel, u8"진단: 문제 %d", (int)shell.problems.size());
		const char* saveLabel = states.isDirty ? u8"저장 필요" : u8"변경 없음";
		float pillsWidth = pillWidth(diagnoseLabel) + pillWidth(saveLabel) + ImGui::GetStyle().ItemSpacing.x;

		// 줄 가운데에 맞춘다
		float lineHeight = ImGui::GetTextLineHeight() + 6.0f;
		ImGui::SetCursorPosY((h - lineHeight) * 0.5f);
		ImGui::AlignTextToFramePadding();
		ImGui::Text("%s", fileName.c_str());
		ImGui::SameLine();
		float used = ImGui::GetCursorPosX();
		float room = w - used - pillsWidth - 36.0f;
		if (room > 40.0f) {
			ImGui::TextDisabled("%s", fitPathTail(folderPath, room).c_str());
		}
		ImGui::SameLine(w - pillsWidth - 12.0f);
		ImGui::SetCursorPosY((h - lineHeight) * 0.5f);
		if (pillButton("diag", diagnoseLabel, shell.problems.empty() ? kMutedColor : kWarnColor)) {
			shell.step = SHELL_STEP_DIAGNOSE;
		}
		ImGui::SameLine();
		if (pillButton("save", saveLabel, states.isDirty ? kAccentColor : kMutedColor) && states.isDirty) {
			states.isSaveModalShow = true;
			states.totalSavingDataCount = 0;
			states.currentSavingDataIndex = 0;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(states.isDirty ? u8"아카이브 정보(게임명, 로케일, 코드페이지)를 바꾼 뒤 저장하지 않았습니다. 누르면 저장합니다" : u8"아카이브 정보에 저장하지 않은 변경이 없습니다");
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
}


// 왼쪽 단계 목록
void Dx9Application::renderShellNav(float x, float y, float w, float h)
{
	ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 10.0f));
	if (ImGui::Begin("##shell_nav", NULL, kPanelFlags)) {
		const float itemHeight = ImGui::GetFontSize() * 1.9f;
		for (int i = SHELL_STEP_DIAGNOSE; i < SHELL_STEP_COUNT; i++) {
			if (i == SHELL_STEP_DIAGNOSE) {
				ImGui::TextDisabled(u8"작업 단계");
			}
			if (i == SHELL_STEP_CHUNK_TREE) {
				ImGui::Spacing();
				ImGui::TextDisabled(u8"고급");
			}
			ImGui::PushID(i);
			bool selected = (shell.step == i);
			if (ImGui::Selectable("##step", selected, 0, ImVec2(0.0f, itemHeight))) {
				shell.step = i;
			}
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
				ImGui::SetTooltip("%s", kSteps[i].tip);
			}
			ImVec2 minPos = ImGui::GetItemRectMin();
			ImVec2 maxPos = ImGui::GetItemRectMax();
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			float textY = minPos.y + (itemHeight - ImGui::GetFontSize()) * 0.5f;
			if (i <= SHELL_STEP_CHECK) {
				char number[8];
				sprintf_s(number, "%d", i + 1);
				drawList->AddText(ImVec2(minPos.x + 8.0f, textY), ImGui::GetColorU32(kMutedColor), number);
			}
			drawList->AddText(ImVec2(minPos.x + 30.0f, textY), ImGui::GetColorU32(ImGuiCol_Text), kSteps[i].name);
			if (i <= SHELL_STEP_REBUILD || i == SHELL_STEP_CHECK) {
				statusDot(drawList, ImVec2(maxPos.x - 14.0f, minPos.y + itemHeight * 0.5f), shellStepStatus(i));
			}
			ImGui::PopID();
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
}


// 가운데 작업 영역: 단계마다 화면이 바뀐다
void Dx9Application::renderShellWork(float x, float y, float w, float h)
{
	ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 12.0f));
	if (ImGui::Begin("##shell_work", NULL, kPanelFlags)) {
		ImGui::Text("%s", kSteps[shell.step].name);
		ImGui::SameLine();
		ImGui::TextDisabled("  %s", kSteps[shell.step].tip);
		ImGui::Separator();
		ImGui::Spacing();
		switch (shell.step) {
			case SHELL_STEP_DIAGNOSE:	renderShellStepDiagnose(); break;
			case SHELL_STEP_EXTRACT:	renderShellStepExtract(); break;
			case SHELL_STEP_TEXT:		renderShellStepText(); break;
			case SHELL_STEP_FONT:		renderShellStepFont(); break;
			case SHELL_STEP_IMAGE:		renderShellStepImage(); break;
			case SHELL_STEP_REBUILD:	renderShellStepRebuild(); break;
			case SHELL_STEP_CHECK:		renderShellStepCheck(); break;
			case SHELL_STEP_CHUNK_TREE:	renderShellStepChunkTree(); break;
			case SHELL_STEP_SCRIPT_JSON: renderShellStepScriptJson(); break;
			default: break;
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
}


// 아래 문제/로그 패널
void Dx9Application::renderShellBottom(float x, float y, float w, float h)
{
	ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 6.0f));
	if (ImGui::Begin("##shell_bottom", NULL, kPanelFlags)) {
		if (!shell.bottomOpen) {
			ImGui::AlignTextToFramePadding();
			ImGui::TextDisabled(u8"문제 %d  로그 %d", (int)shell.problems.size(), (int)shell.logs.size());
			ImGui::SameLine();
			if (ImGui::SmallButton(u8"열기")) {
				shell.bottomOpen = true;
			}
		}
		else {
			char problemLabel[48];
			char logLabel[48];
			sprintf_s(problemLabel, u8"문제 %d###problems", (int)shell.problems.size());
			sprintf_s(logLabel, u8"로그 %d###logs", (int)shell.logs.size());
			if (ImGui::BeginTabBar("##bottomtabs")) {
				if (ImGui::BeginTabItem(problemLabel)) {
					shell.bottomTab = 0;
					ImGui::BeginChild("##problemlist", ImVec2(0.0f, 0.0f), false);
					if (shell.problems.empty()) {
						ImGui::TextDisabled(u8"문제가 없습니다. 진단 기능이 들어오면 진단 결과가 여기에 모입니다.");
					}
					for (size_t i = 0; i < shell.problems.size(); i++) {
						const ShellProblem& problem = shell.problems[i];
						ImGui::PushID((int)i);
						ImGui::TextColored(problem.severity == 0 ? kWarnColor : kMutedColor, "%s", problem.severity == 0 ? u8"확인 필요" : u8"알 수 없음");
						ImGui::SameLine();
						ImGui::TextWrapped("%s", problem.text.c_str());
						ImGui::SameLine();
						if (ImGui::SmallButton(u8"이동")) {
							shell.step = problem.goStep;
						}
						ImGui::PopID();
					}
					ImGui::EndChild();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(logLabel)) {
					shell.bottomTab = 1;
					ImGui::BeginChild("##loglist", ImVec2(0.0f, 0.0f), false);
					if (shell.logs.empty()) {
						ImGui::TextDisabled(u8"아직 기록이 없습니다.");
					}
					for (size_t i = 0; i < shell.logs.size(); i++) {
						const ShellLogLine& line = shell.logs[i];
						ImVec4 color = (line.level == ShellLogLevel::Error) ? kErrorColor : (line.level == ShellLogLevel::Warn) ? kWarnColor : ImGui::GetStyleColorVec4(ImGuiCol_Text);
						ImGui::TextColored(kMutedColor, "%s", line.time.c_str());
						ImGui::SameLine();
						ImGui::TextColored(color, "%s", line.text.c_str());
					}
					if (shell.scrollLogToEnd) {
						ImGui::SetScrollHereY(1.0f);
						shell.scrollLogToEnd = false;
					}
					ImGui::EndChild();
					ImGui::EndTabItem();
				}
				// 오른쪽 끝: 접기
				float closeWidth = ImGui::CalcTextSize(u8"접기").x + ImGui::GetStyle().FramePadding.x * 2.0f;
				ImGui::SameLine(w - closeWidth - 24.0f);
				if (ImGui::SmallButton(u8"접기")) {
					shell.bottomOpen = false;
				}
				ImGui::EndTabBar();
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
}


// ------------------- BEGIN [단계 화면] ---------------------


void Dx9Application::renderShellStepDiagnose()
{
	if (ImGui::CollapsingHeader(u8"프로젝트 정보", ImGuiTreeNodeFlags_DefaultOpen)) {
		renderArchiveInfoContent();
	}
	ImGui::Spacing();
	ImGui::TextWrapped(u8"진단 기능은 아직 없습니다. 만들어지면 이곳에 구조 분석 결과와 진행 방향이 나옵니다. 지금은 아래 순서로 작업하세요.");
	ImGui::Spacing();
	const int order[6] = { SHELL_STEP_EXTRACT, SHELL_STEP_TEXT, SHELL_STEP_FONT, SHELL_STEP_IMAGE, SHELL_STEP_REBUILD, SHELL_STEP_CHECK };
	for (int i = 0; i < 6; i++) {
		ImGui::PushID(i);
		ImGui::TextColored(kAccentColor, "%d", i + 1);
		ImGui::SameLine(40.0f);
		ImGui::Text("%s", kSteps[order[i]].name);
		ImGui::SameLine(170.0f);
		ImGui::TextDisabled("%s", kSteps[order[i]].tip);
		ImGui::SameLine(ImGui::GetWindowWidth() - 70.0f);
		if (ImGui::SmallButton(u8"이동")) {
			shell.step = order[i];
		}
		ImGui::PopID();
	}
}


void Dx9Application::renderShellStepExtract()
{
	wstring baseDir = shellBaseDir(archive);
	bool exists = pathExists(baseDir, true);
	ImGui::TextWrapped(u8"원본에서 작업 폴더로 데이터를 풀어냅니다. 처음에는 전체 추출을 권합니다.");
	ImGui::Spacing();
	if (ImGui::Button(u8"전체 추출")) {
		states.isExtractModalShow = true;
		states.totalDataCount = 0;
		states.currentDataIndex = 0;
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(u8"추출 가능한 모든 Chunk 데이터를\n아카이브와 같은 이름을 가진 폴더에 추출합니다");
	}
	ImGui::SameLine();
	if (ImGui::Button(u8"지정한 유형만 추출...")) {
		states.isPartExtractModalShow = true;
		states.totalDataCount = 0;
		states.currentDataIndex = 0;
		// 메뉴의 '지정한 유형 Chunk만 추출'과 같은 기본 체크 (투명은 기본적으로 x)
		states.usingImage = true;
		states.usingImageAndExtractNoneTransparent = false;
		states.usingSound = true;
		states.usingFont = true;
		states.usingModule = true;
		states.usingShader = true;
		states.usingBinaryData = true;
		states.usingText = true;
		states.usingScript = true;
		states.usingFlow = true;
	}
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();
	string baseUtf8 = getAnsiStr(baseDir, CodePage::CP_UTF_8);
	ImGui::Text(u8"작업 폴더");
	ImGui::SameLine(110.0f);
	ImGui::TextDisabled("%s", baseUtf8.c_str());
	ImGui::Text(u8"상태");
	ImGui::SameLine(110.0f);
	if (exists) {
		ImGui::TextColored(kOkColor, u8"추출한 폴더가 있습니다");
		ImGui::SameLine();
		if (ImGui::SmallButton(u8"폴더 열기")) {
			openFolder(baseDir);
		}
	}
	else {
		ImGui::TextColored(kMutedColor, u8"아직 추출하지 않았습니다");
	}
}


void Dx9Application::renderShellStepText()
{
	wstring baseDir = shellBaseDir(archive);
	ImGui::TextWrapped(u8"번역할 문자열은 세 곳에 있습니다. 파일을 열어 translation 칸에 번역을 적고, 재합성할 때 '텍스트'와 '스크립트'를 체크합니다.");
	ImGui::Spacing();
	struct Row { const char* name; const wchar_t* path; const char* where; };
	const Row rows[3] = {
		{ u8"이벤트 문자열", L"/text/frameEventText.json", u8"이벤트가 쓰는 모든 문자열 상수 (대사 외에 키와 경로도 섞여 있습니다)" },
		{ u8"오브젝트 문자열", L"/text/objectText.json", u8"텍스트 오브젝트와 문단의 글자" },
		{ u8"글자 오브젝트 처음 문자열", L"/script/object", u8"script/object 의 textBlitter.text" }
	};
	for (int i = 0; i < 3; i++) {
		bool isDir = (i == 2);
		bool exists = pathExists(baseDir + rows[i].path, isDir);
		ImGui::Text("%s", rows[i].name);
		ImGui::SameLine(210.0f);
		if (exists) {
			ImGui::TextColored(kOkColor, u8"있음");
		}
		else {
			ImGui::TextColored(kMutedColor, u8"없음");
		}
		ImGui::SameLine(270.0f);
		ImGui::TextDisabled("%s", rows[i].where);
	}
	ImGui::Spacing();
	folderButton(baseDir, L"text", u8"텍스트 폴더 열기", u8"frameEventText.json, objectText.json 의 translation 칸에 번역을 적습니다");
	ImGui::SameLine();
	folderButton(baseDir, L"script", u8"스크립트 폴더 열기", u8"디컴파일한 frame/ 의 이벤트, object/ 의 오브젝트 JSON 을 고칩니다");
}


void Dx9Application::renderShellStepFont()
{
	wstring baseDir = shellBaseDir(archive);
	ImGui::TextWrapped(u8"한글 폰트 아틀라스를 만들고, 글자를 그리는 오브젝트마다 쓸 아틀라스를 정한 뒤, 게임 모듈에 심습니다. 아래 버튼은 한글화 도구 창을 엽니다 (이후 단계에서 이 화면 안으로 옮깁니다).");
	ImGui::Spacing();
	if (ImGui::Button(u8"가. 폰트 아틀라스 만들기...")) {
		hangul.showWindow = true;
		hangul.requestTab = 0;
	}
	ImGui::SameLine();
	if (ImGui::Button(u8"나. 오브젝트에 적용...")) {
		hangul.showWindow = true;
		hangul.requestTab = 1;
	}
	ImGui::SameLine();
	if (ImGui::Button(u8"다. 게임에 넣기...")) {
		hangul.showWindow = true;
		hangul.requestTab = 2;
	}
	ImGui::Spacing();
	folderButton(baseDir, L"font", u8"폰트 폴더 열기", u8"TrueType/LogFont 폰트 파일을 바꿉니다");
}


void Dx9Application::renderShellStepImage()
{
	wstring baseDir = shellBaseDir(archive);
	ImGui::TextWrapped(u8"PNG를 같은 이름으로 고치거나 appendImages.json으로 새 이미지를 추가합니다. 로고나 메뉴 그림 속의 글자는 여기서 고칩니다.");
	ImGui::Spacing();
	folderButton(baseDir, L"image", u8"이미지 폴더 열기", u8"PNG 를 같은 이름으로 고치거나 appendImages.json 으로 새 이미지를 추가합니다");
}


void Dx9Application::renderShellStepRebuild()
{
	ImGui::TextWrapped(u8"고친 파일을 아카이브에 합쳐 새 아카이브(~_new)를 만듭니다. 원본은 바뀌지 않습니다. 한글을 쓰는 게임은 코드페이지 949, 로케일 1042로 교체합니다 (진단 단계의 프로젝트 정보에서 바꿀 수 있습니다).");
	ImGui::Spacing();
	if (ImGui::Button(u8"전체 교체")) {
		states.isReplaceModalShow = true;
		states.totalDataCount = 0;
		states.currentDataIndex = 0;
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(u8"추출되어 있는 모든 Chunk 데이터를 교체하여\n'~_new' 가 붙은 새 아카이브 파일로 저장합니다");
	}
	ImGui::SameLine();
	if (ImGui::Button(u8"지정한 유형만 교체...")) {
		states.isPartReplaceModalShow = true;
		states.totalDataCount = 0;
		states.currentDataIndex = 0;
		// 메뉴의 '지정한 유형 Chunk만 교체'와 같은 기본 체크 (투명은 기본적으로 x, usingShader는 예외)
		states.usingImage = true;
		states.usingImageAndExtractNoneTransparent = false;
		states.usingSound = true;
		states.usingFont = true;
		states.usingModule = true;
		states.usingShader = false;
		states.usingBinaryData = true;
		states.usingText = true;
		states.usingScript = true;
	}
}


void Dx9Application::renderShellStepCheck()
{
	ImGui::TextWrapped(u8"프레임 배치도와 번역한 글자의 미리보기가 이 단계에 들어옵니다. 아직 만들어지지 않았습니다.");
}


void Dx9Application::renderShellStepChunkTree()
{
	const float listWidth = (float)states.chunkListWindowWidth;
	ImGui::BeginChild("##chunklist", ImVec2(listWidth, 0.0f), true);
	renderArchiveChunkListContent();
	ImGui::EndChild();
	ImGui::SameLine();
	ImGui::BeginChild("##chunkinfo", ImVec2(0.0f, 0.0f), true);
	renderArchiveEachChunkInfoContent();
	ImGui::EndChild();
}


void Dx9Application::renderShellStepScriptJson()
{
	wstring baseDir = shellBaseDir(archive);
	ImGui::TextWrapped(u8"디컴파일한 이벤트(script/frame)와 오브젝트(script/object) JSON을 고치고, 재합성할 때 '스크립트'를 체크하면 아카이브에 합쳐집니다. 수정하지 않은 JSON으로 합치면 내용이 바뀌지 않습니다.");
	ImGui::Spacing();
	folderButton(baseDir, L"script", u8"스크립트 폴더 열기", u8"고친 내용은 재합성의 '스크립트'(리컴파일)로 아카이브에 합쳐집니다");
}


// ------------------- END [단계 화면] ---------------------
