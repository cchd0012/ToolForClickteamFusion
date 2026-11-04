#include "Dx9Application.h"
#include "../ImGUI/Headers.h"
#include "../Util/CommonUtil.h"

#include <algorithm>
#include <set>

using namespace CTT::App;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;
using namespace CTT::JsonUtil;

// 이벤트 찾기 화면 (Application/EventSearchUi.h 참고)

EventSearchUiState::EventSearchUiState()
{
	memset(objectName, 0, sizeof(objectName));
	memset(numText, 0, sizeof(numText));
	memset(paramText, 0, sizeof(paramText));
	memset(frameText, 0, sizeof(frameText));
	scope = 0;
	running = 0;
	done = 0;
	bufferGroups = bufferFrames = 0;
	bufferTruncated = false;
	totalGroups = totalFrames = 0;
	truncated = false;
	searched = false;
}


void EventSearchUiState::reset()
{
	if (running) {
		return;
	}
	results.clear();
	hints.clear();
	bufferResults.clear();
	bufferHints.clear();
	totalGroups = totalFrames = 0;
	truncated = false;
	message.clear();
	searched = false;
	done = 0;
}


namespace {

	const int kMaxResults = 300;
	const int kMaxPlainConditions = 10;
	const int kMaxPlainActions = 14;

	const ImVec4 kConditionColor(0.32f, 0.77f, 0.82f, 1.0f);
	const ImVec4 kMatchColor(0.94f, 0.71f, 0.33f, 1.0f);
	const ImVec4 kMutedColor(0.62f, 0.68f, 0.72f, 1.0f);
	const ImVec4 kHintColor(0.55f, 0.82f, 0.55f, 1.0f);

	string lowerAscii(string s)
	{
		for (size_t i = 0; i < s.size(); i++) {
			if (s[i] >= 'A' && s[i] <= 'Z') {
				s[i] = (char)(s[i] - 'A' + 'a');
			}
		}
		return s;
	}

	bool containsIgnoreCase(const string& hay, const string& needle)
	{
		if (needle.empty()) {
			return true;
		}
		return lowerAscii(hay).find(lowerAscii(needle)) != string::npos;
	}

	// 오브젝트 종류 이름 (표준 번호). 모르면 번호만 적는다
	string objectTypeName(int type)
	{
		switch (type) {
			case -1: return u8"시스템";
			case -2: return u8"소리";
			case -3: return u8"프레임";
			case -4: return u8"타이머";
			case -5: return u8"생성";
			case 2: return u8"액티브";
			case 7: return u8"카운터";
		}
		return to_string(type);
	}

	// 이 도구가 데이터로 뜻을 확인한 동작/조건 번호만 이름을 붙인다 (그 밖은 번호만 보여 준다)
	const char* eventNumName(int objectType, int num, bool isCondition)
	{
		if (!isCondition) {
			if (objectType == 7) {
				switch (num) {
					case 80: return u8"값 정하기";
					case 81: return u8"더하기";
					case 82: return u8"빼기";
					case 83: return u8"최소값 정하기";
					case 84: return u8"최대값 정하기";
					case 85: return u8"색 정하기";
				}
			}
			if (objectType == -3 && num == 2) {
				return u8"프레임 이동";
			}
			if (objectType == 2 && num == 31) {
				return u8"알터러블 값 정하기";
			}
		}
		else {
			if (objectType == 7 && num == -81) {
				return u8"값 비교";
			}
			if (objectType == 2 && num == -42) {
				return u8"알터러블 값 비교";
			}
		}
		return NULL;
	}

	string paramSummary(const Json::Value& p)
	{
		const string kind = p.get("kind", "").asString();
		if (kind == "expression") {
			return p.get("text", "").asString();
		}
		if (kind == "object") {
			return string(u8"오브젝트:") + p.get("objectName", to_string(p.get("objectInfo", 0).asInt())).asString();
		}
		if (kind == "key") {
			return string(u8"키:") + to_string(p.get("value", 0).asInt());
		}
		if (kind == "group") {
			return string(u8"그룹 ") + to_string(p.get("id", 0).asInt()) + (p.isMember("name") ? string(" ") + p["name"].asString() : string());
		}
		if (kind == "altValue") {
			return string(u8"알터러블 값 ") + to_string(p.get("value", 0).asInt());
		}
		if (kind == "globalValue") {
			return string(u8"전역 값 ") + to_string(p.get("value", 0).asInt());
		}
		const Json::Value& v = p["value"];
		if (v.isString()) {
			return kind + ":" + v.asString();
		}
		if (v.isIntegral()) {
			return kind + ":" + to_string(v.asInt());
		}
		return kind;
	}

	bool paramsContain(const Json::Value& e, const string& needle)
	{
		const Json::Value& params = e["params"];
		for (Json::ArrayIndex i = 0; i < params.size(); i++) {
			if (containsIgnoreCase(paramSummary(params[i]), needle)) {
				return true;
			}
			const Json::Value& name = params[i]["name"];
			if (name.isString() && containsIgnoreCase(name.asString(), needle)) {
				return true;
			}
		}
		return false;
	}

	string eventLine(const Json::Value& e, bool isCondition)
	{
		const int type = e.get("objectType", 0).asInt();
		const int num = e.get("num", 0).asInt();
		string line = isCondition ? u8"조건 #" : u8"동작 #";
		line += to_string(num);
		const char* meaning = eventNumName(type, num, isCondition);
		if (meaning != NULL) {
			line += string("(") + meaning + ")";
		}
		line += " ";
		line += e.get("objectName", "").asString();
		line += string("[") + objectTypeName(type) + "]";
		const Json::Value& params = e["params"];
		if (params.isArray() && params.size() > 0) {
			line += " : ";
			for (Json::ArrayIndex i = 0; i < params.size(); i++) {
				if (i > 0) {
					line += " | ";
				}
				line += paramSummary(params[i]);
			}
		}
		if (line.size() > 420) {
			line.resize(420);
			line += "...";
		}
		return line;
	}

	bool eventMatches(const Json::Value& e, bool isCondition, const EventSearchCriteria& c)
	{
		if ((c.scope == 1 && !isCondition) || (c.scope == 2 && isCondition)) {
			return false;
		}
		if (!c.objectName.empty() && !containsIgnoreCase(e.get("objectName", "").asString(), c.objectName)) {
			return false;
		}
		if (c.hasNum && e.get("num", 0).asInt() != c.num) {
			return false;
		}
		if (!c.paramText.empty() && !paramsContain(e, c.paramText)) {
			return false;
		}
		return true;
	}

	// 조건과 동작이 모두 있는 JSON 객체(= 이벤트 그룹)를 모은다
	void collectGroups(const Json::Value& node, vector<const Json::Value*>& out)
	{
		if (node.isObject()) {
			if (node["conditions"].isArray() && node["actions"].isArray()) {
				out.push_back(&node);
			}
			for (Json::Value::const_iterator it = node.begin(); it != node.end(); ++it) {
				if ((*it).isObject() || (*it).isArray()) {
					collectGroups(*it, out);
				}
			}
		}
		else if (node.isArray()) {
			for (Json::ArrayIndex i = 0; i < node.size(); i++) {
				collectGroups(node[i], out);
			}
		}
	}

	const char* hintText(int key)
	{
		switch (key) {
			case 1:
				return u8"카운터에서 값을 빼는 동작입니다. 체력, 목숨, 시간 같은 카운터라면 피해나 소모가 일어나는 곳입니다. 시험할 때 닳지 않게 하려면 이 동작의 식을 0 으로 고친 뒤 3.교체의 '스크립트'로 재합성하세요 (위 프레임 JSON 에서 그 동작의 params 안 text 와 tokens). 값을 0 으로 정하는 동작(#80)은 게임 오버 처리일 수 있으니 건드리지 마세요. dtge 는 docs/dtge/patch_nodamage.py 가 이 일을 합니다.";
			case 2:
				return u8"프레임 이동 동작입니다. 값은 프레임 번호가 아니라 내부 번호일 수 있습니다 (dtge 가이드의 '시험할 때 쓸 수 있는 방법' 참고). 특정 화면으로 바로 가서 확인하고 싶을 때 이 동작의 값을 바꿔서 시험합니다.";
			case 3:
				return u8"오브젝트의 알터러블 값을 비교하는 조건입니다. 게임의 모드나 단계를 이 값으로 나누는 경우가 많습니다. 값 번호가 어떤 상태를 뜻하는지는 같은 오브젝트에 값을 정하는 동작(#31)을 찾아 보면 알 수 있습니다.";
			case 4:
				return u8"오브젝트의 알터러블 값을 정하는 동작입니다. 같은 오브젝트와 값 번호를 비교하는 조건(#-42)을 찾으면 그 값이 무엇을 정하는지 알 수 있습니다.";
		}
		return "";
	}

	struct EventSearchJob
	{
		EventSearchUiState* ui;
		EventSearchCriteria criteria;
		wstring frameDir;
	};

	DWORD WINAPI eventSearchThread(LPVOID param)
	{
		EventSearchJob* job = (EventSearchJob*)param;
		EventSearchUiState* ui = job->ui;
		const EventSearchCriteria& c = job->criteria;

		vector<EventSearchResult> results;
		set<int> hintKeys;
		int groupCount = 0, frameCount = 0;
		bool truncated = false;

		vector<wstring> files;
		WIN32_FIND_DATAW data;
		HANDLE handle = FindFirstFileW((job->frameDir + L"\\*.json").c_str(), &data);
		if (handle != INVALID_HANDLE_VALUE) {
			do {
				if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
					files.push_back(data.cFileName);
				}
			} while (FindNextFileW(handle, &data));
			FindClose(handle);
		}
		sort(files.begin(), files.end());

		for (size_t f = 0; f < files.size() && !truncated; f++) {
			const int frameNumber = _wtoi(files[f].c_str());
			if (c.frame >= 0 && frameNumber != c.frame) {
				continue;
			}
			Json::Value root;
			wstring path = job->frameDir + L"\\" + files[f];
			if (!readFromJson(root, path)) {
				continue;
			}
			frameCount++;
			vector<const Json::Value*> groups;
			collectGroups(root, groups);
			groupCount += (int)groups.size();
			for (size_t g = 0; g < groups.size() && !truncated; g++) {
				const Json::Value& group = *groups[g];
				const Json::Value& conditions = group["conditions"];
				const Json::Value& actions = group["actions"];
				vector<char> condMatched(conditions.size(), 0), actMatched(actions.size(), 0);
				bool any = false;
				for (Json::ArrayIndex i = 0; i < conditions.size(); i++) {
					if (eventMatches(conditions[i], true, c)) {
						condMatched[i] = 1;
						any = true;
					}
				}
				for (Json::ArrayIndex i = 0; i < actions.size(); i++) {
					if (eventMatches(actions[i], false, c)) {
						actMatched[i] = 1;
						any = true;
					}
				}
				if (!any) {
					continue;
				}
				if ((int)results.size() >= kMaxResults) {
					truncated = true;
					break;
				}
				EventSearchResult result;
				result.frame = frameNumber;
				result.line = group.get("line", 0).asInt();
				result.uid = group.get("uid", 0).asInt();
				int plainCond = 0, plainAct = 0;
				for (Json::ArrayIndex i = 0; i < conditions.size(); i++) {
					if (!condMatched[i] && plainCond >= kMaxPlainConditions) {
						result.hiddenLines++;
						continue;
					}
					plainCond += condMatched[i] ? 0 : 1;
					EventSearchLine line;
					line.isCondition = true;
					line.matched = condMatched[i] != 0;
					line.text = eventLine(conditions[i], true);
					result.lines.push_back(line);
				}
				for (Json::ArrayIndex i = 0; i < actions.size(); i++) {
					if (!actMatched[i] && plainAct >= kMaxPlainActions) {
						result.hiddenLines++;
						continue;
					}
					plainAct += actMatched[i] ? 0 : 1;
					EventSearchLine line;
					line.isCondition = false;
					line.matched = actMatched[i] != 0;
					line.text = eventLine(actions[i], false);
					result.lines.push_back(line);
				}
				// 맞은 이벤트에 뜻을 아는 것이 있으면 안내를 붙인다
				for (Json::ArrayIndex i = 0; i < actions.size(); i++) {
					if (!actMatched[i]) {
						continue;
					}
					const int type = actions[i].get("objectType", 0).asInt(), num = actions[i].get("num", 0).asInt();
					if (type == 7 && num == 82) {
						hintKeys.insert(1);
					}
					else if (type == -3 && num == 2) {
						hintKeys.insert(2);
					}
					else if (type == 2 && num == 31) {
						hintKeys.insert(4);
					}
				}
				for (Json::ArrayIndex i = 0; i < conditions.size(); i++) {
					if (condMatched[i] && conditions[i].get("objectType", 0).asInt() == 2 && conditions[i].get("num", 0).asInt() == -42) {
						hintKeys.insert(3);
					}
				}
				results.push_back(result);
			}
		}

		ui->bufferResults.swap(results);
		ui->bufferHints.clear();
		for (set<int>::const_iterator it = hintKeys.begin(); it != hintKeys.end(); ++it) {
			ui->bufferHints.push_back(hintText(*it));
		}
		ui->bufferGroups = groupCount;
		ui->bufferFrames = frameCount;
		ui->bufferTruncated = truncated;
		ui->bufferMessage = files.empty() ? string(u8"추출한 프레임 스크립트가 없습니다. 추출 단계에서 '스크립트'를 추출하세요.") : string();
		delete job;
		InterlockedExchange(&ui->done, 1);
		return 0;
	}

}


void Dx9Application::renderShellStepEventSearch()
{
	EventSearchUiState& s = eventSearch;

	// 끝난 찾기 결과를 받는다
	if (InterlockedCompareExchange(&s.done, 0, 1) == 1) {
		s.results.swap(s.bufferResults);
		s.hints.swap(s.bufferHints);
		s.totalGroups = s.bufferGroups;
		s.totalFrames = s.bufferFrames;
		s.truncated = s.bufferTruncated;
		s.message = s.bufferMessage;
		s.searched = true;
		InterlockedExchange(&s.running, 0);
	}

	ImGui::TextWrapped(u8"디컴파일한 이벤트(script/frame/*.json)에서 이벤트 그룹을 찾습니다. 오브젝트 이름, 동작/조건 번호, 파라미터에 든 글자 중 아는 것만 적으세요. 비워 둔 것은 따지지 않습니다.");
	ImGui::Spacing();

	bool startSearch = false;
	const float fieldWidth = ImGui::GetFontSize() * 14.0f;
	ImGui::SetNextItemWidth(fieldWidth);
	startSearch |= ImGui::InputText(u8"오브젝트 이름", s.objectName, IM_ARRAYSIZE(s.objectName), ImGuiInputTextFlags_EnterReturnsTrue);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5.0f);
	startSearch |= ImGui::InputText(u8"번호", s.numText, IM_ARRAYSIZE(s.numText), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsDecimal);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(u8"동작 또는 조건 번호입니다. 조건 번호는 음수입니다 (예: -81). 오른쪽 종류로 조건만 / 동작만 고를 수 있습니다");
	}
	ImGui::SameLine();
	const char* scopes[3] = { u8"조건과 동작", u8"조건만", u8"동작만" };
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
	ImGui::Combo(u8"종류", &s.scope, scopes, 3);
	ImGui::SetNextItemWidth(fieldWidth);
	startSearch |= ImGui::InputText(u8"파라미터 글자", s.paramText, IM_ARRAYSIZE(s.paramText), ImGuiInputTextFlags_EnterReturnsTrue);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(u8"파라미터의 식이나 값에 이 글자가 있는 이벤트를 찾습니다. 문자열 상수나 그룹 이름을 찾을 때 씁니다");
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5.0f);
	startSearch |= ImGui::InputText(u8"프레임", s.frameText, IM_ARRAYSIZE(s.frameText), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsDecimal);

	const bool searchingNow = (s.running != 0);		// 누르는 프레임에 바뀌어도 BeginDisabled/EndDisabled 의 짝이 맞도록 한 번만 읽는다
	if (searchingNow) {
		ImGui::BeginDisabled(true);
	}
	if (ImGui::Button(u8"찾기") || startSearch) {
		if (!s.running) {
			EventSearchJob* job = new EventSearchJob();
			job->ui = &s;
			job->criteria.objectName = s.objectName;
			job->criteria.hasNum = strlen(s.numText) > 0 && strcmp(s.numText, "-") != 0;
			job->criteria.num = job->criteria.hasNum ? atoi(s.numText) : 0;
			job->criteria.scope = s.scope;
			job->criteria.paramText = s.paramText;
			job->criteria.frame = strlen(s.frameText) > 0 ? atoi(s.frameText) : -1;
			wstring archivePath = archive->getArchivePath();
			job->frameDir = getExtentionRemovedPath(archivePath) + L"\\script\\frame";
			InterlockedExchange(&s.done, 0);
			InterlockedExchange(&s.running, 1);
			CreateThread(NULL, 0, eventSearchThread, job, 0, NULL);
		}
	}
	if (searchingNow) {
		ImGui::EndDisabled();
	}
	if (s.running) {
		ImGui::SameLine();
		ImGui::TextDisabled(u8"찾는 중...");
	}

	if (!s.searched) {
		ImGui::Spacing();
		ImGui::TextDisabled(u8"찾을 조건을 적고 [찾기]를 누르세요. 이벤트가 큰 게임은 몇 초 걸립니다.");
		return;
	}

	ImGui::Spacing();
	if (!s.message.empty()) {
		ImGui::TextWrapped("%s", s.message.c_str());
		return;
	}
	ImGui::TextDisabled(u8"프레임 %d개, 이벤트 그룹 %d개를 훑어서 %d개를 찾았습니다%s", s.totalFrames, s.totalGroups, (int)s.results.size(), s.truncated ? u8" (너무 많아 앞의 일부만 보입니다. 조건을 더 좁히세요)" : "");

	// 찾은 이벤트에 대한 안내
	if (!s.hints.empty()) {
		ImGui::Spacing();
		ImGui::SeparatorText(u8"안내");
		for (size_t i = 0; i < s.hints.size(); i++) {
			ImGui::PushStyleColor(ImGuiCol_Text, kHintColor);
			ImGui::TextWrapped("%s", s.hints[i].c_str());
			ImGui::PopStyleColor();
			ImGui::Spacing();
		}
	}
	if (!s.results.empty()) {
		ImGui::Spacing();
		ImGui::SeparatorText(u8"찾은 이벤트");
		ImGui::TextDisabled(u8"고치려면 해당 프레임의 JSON(script/frame/<프레임 번호 6자리>.json)에서 그 동작을 찾아 고치고, 3.교체의 '스크립트'로 재합성하세요. 번호 뒤 괄호의 이름은 이 도구가 확인한 것만 붙습니다.");
	}

	for (size_t i = 0; i < s.results.size(); i++) {
		const EventSearchResult& r = s.results[i];
		char label[160];
		sprintf_s(label, u8"프레임 %d  ·  이벤트 줄 %d  ·  그룹 uid %d###res%d", r.frame, r.line, r.uid, (int)i);
		if (s.results.size() <= 12) {
			ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
		}
		if (ImGui::CollapsingHeader(label)) {
			for (size_t k = 0; k < r.lines.size(); k++) {
				const EventSearchLine& line = r.lines[k];
				ImVec4 color = line.matched ? kMatchColor : (line.isCondition ? kConditionColor : ImGui::GetStyleColorVec4(ImGuiCol_Text));
				if (!line.matched) {
					color.w = 0.78f;
				}
				ImGui::PushStyleColor(ImGuiCol_Text, color);
				ImGui::TextWrapped("%s%s", line.matched ? u8"> " : "   ", line.text.c_str());
				ImGui::PopStyleColor();
			}
			if (r.hiddenLines > 0) {
				ImGui::TextDisabled(u8"   ...(맞지 않는 %d줄은 줄였습니다)", r.hiddenLines);
			}
		}
	}
}
