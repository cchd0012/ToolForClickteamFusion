#include "AppHeaderChunk.h"
#include "../../Util/JsonUtil.h"
#include "../../Util/CommonUtil.h"
#include "../../ImGUI/Headers.h"

using namespace CTT::Chunk;
using namespace CTT::JsonUtil;
using namespace CTT::CommonUtil;

// 비트 위치별로 포함되는 정보
wstring commonFlagStr[] = {
	L"HeadingMaximized",
	L"NoHeading",
	L"FitInsideBars",
	L"MachineIndependentSpeed",
	L"ResizeDisplay",
	L"MusicOn",
	L"SoundOn",
	L"DontDisplayMenu",
	L"MenuBar",
	L"MaximizedOnBoot",
	L"MultiSamples",
	L"ChangeResolutionMode",
	L"SwitchToFromFullscreen",
	L"Protected",
	L"Copyright",
	L"OneFile"
};

wstring windowFlagStr[] = {
	L"SamplesOverFrames",
	L"RelocFiles",
	L"RunFrame",
	L"PlaySamplesWhenUnfocused",
	L"NoMinimizeBox",
	L"NoMaximizeBox",
	L"NoThickFrame",
	L"DoNotCenterFrame",
	L"IgnoreInputOnScreensaver",
	L"DisableClose",
	L"HiddenAtStart",
	L"VisualThemes",
	L"VSync",
	L"RunWhenMinimized",
	L"MDI",
	L"RunWhileResizing"
};

wstring deviceFlagStr[] = {
	L"DebuggerShortcuts",
	L"Unknown1",
	L"Unknown2",
	L"DontShareSubData",
	L"Unknown3",
	L"Unknown4",
	L"Unknown5",
	L"ShowDebugger",
	L"Unknown6",
	L"Unknown7",
	L"Unknown8",
	L"Unknown9",
	L"Unknown10",
	L"Unknown11",
	L"Direct3D9or11",
	L"Direct3D8or11"
};


vector<string> CTT::Chunk::getCommonFlagProperties(UINT16 commonFlag)
{
	vector<string> result;
	for (UINT32 i = 0; i < 0x10; i++) {
		if ((commonFlag & (1 << i)) != 0) {
			string flagStrAnsi = getAnsiStr(commonFlagStr[i], CodePage::CP_UTF_8);
			result.push_back(flagStrAnsi);
		}
	}
	return result;
}

vector<string> CTT::Chunk::getWindowFlagProperties(UINT16 windowFlag)
{
	vector<string> result;
	for (UINT32 i = 0; i < 0x10; i++) {
		if ((windowFlag & (1 << i)) != 0) {
			string flagStrAnsi = getAnsiStr(windowFlagStr[i], CodePage::CP_UTF_8);
			result.push_back(flagStrAnsi);
		}
	}
	return result;
}

vector<string> CTT::Chunk::getDeviceFlagProperties(UINT16 deviceFlag)
{
	vector<string> result;
	for (UINT32 i = 0; i < 0x10; i++) {
		if ((deviceFlag & (1 << i)) != 0) {
			string flagStrAnsi = getAnsiStr(deviceFlagStr[i], CodePage::CP_UTF_8);
			result.push_back(flagStrAnsi);
		}
	}
	return result;
}


AppHeaderChunk::AppHeaderChunk()
{
	memset(&value, 0, sizeof(AppHeader));
}


AppHeaderChunk::~AppHeaderChunk()
{
}


bool AppHeaderChunk::loadFromOrgBuffer()
{
	// chunk 데이터: [AppHeader]
	memcpy(&value, orgBuffer, sizeof(AppHeader));
	if (value.size != orgSize) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": AppHeader Chunk: Data가 깨졌습니다." << endl;
		return false;
	}
	value.initialScore ^= 0xFFFFFFFF;
	value.initialLives ^= 0xFFFFFFFF;

	free(orgBuffer);
	orgBuffer = NULL;
	return true;
}


bool AppHeaderChunk::saveIntoOrgBuffer()
{
	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}
	orgSize = sizeof(AppHeader);
	orgBuffer = (BYTE*)malloc(orgSize);

	value.initialScore ^= 0xFFFFFFFF;
	value.initialLives ^= 0xFFFFFFFF;
	memcpy(orgBuffer, &value, sizeof(AppHeader));

	return true;
}


Json::Value AppHeaderChunk::exportAsJson()
{
	Json::Value exportValues;

	Json::Value commonFlagPropertyList;
	vector<string> commonFlagPropertyStrings = getCommonFlagProperties(value.commonFlag);
	for (size_t i = 0; commonFlagPropertyStrings.size(); i++) {
		commonFlagPropertyList.append(commonFlagPropertyStrings[i]);
	}
	exportValues["common_property"] = commonFlagPropertyList;

	Json::Value windowFlagPropertyList;
	vector<string> windowFlagPropertyStrings = getWindowFlagProperties(value.windowFlag);
	for (size_t i = 0; windowFlagPropertyStrings.size(); i++) {
		windowFlagPropertyList.append(windowFlagPropertyStrings[i]);
	}
	exportValues["window_property"] = windowFlagPropertyList;

	exportValues["graphic_mode"] = value.graphicMode;

	Json::Value deviceFlagPropertyList;
	vector<string> deviceFlagPropertyStrings = getDeviceFlagProperties(value.deviceFlag);
	for (size_t i = 0; deviceFlagPropertyStrings.size(); i++) {
		deviceFlagPropertyList.append(deviceFlagPropertyStrings[i]);
	}
	exportValues["device_property"] = deviceFlagPropertyList;

	exportValues["window_width"] = value.windowWidth;

	exportValues["window_height"] = value.windowHeight;

	exportValues["initial_score"] = value.initialScore;

	exportValues["initial_lives"] = value.initialLives;

	Json::Value controlList;
	for (UINT32 i = 0; i < 0x4; i++) {

		Json::Value playerControl;
		playerControl["control_type"] = value.controls[i].controlType;

		Json::Value playerKeys;
		playerKeys["up"] = value.controls[i].keys.up;
		playerKeys["down"] = value.controls[i].keys.down;
		playerKeys["left"] = value.controls[i].keys.left;
		playerKeys["right"] = value.controls[i].keys.right;
		playerKeys["button1"] = value.controls[i].keys.button1;
		playerKeys["button2"] = value.controls[i].keys.button2;
		playerKeys["button3"] = value.controls[i].keys.button3;
		playerKeys["button4"] = value.controls[i].keys.button4;
		playerControl["keys"] = playerKeys;

		controlList.append(playerControl);
	}
	exportValues["controls"] = controlList;

	wstring borderColorUni = value.borderColor.toString();
	exportValues["border_color"] = getAnsiStr(borderColorUni, CodePage::CP_UTF_8);

	exportValues["number_of_frames"] = value.numberOfFrames;

	exportValues["frame_rate"] = value.frameRate;

	exportValues["window_menu_index"] = value.windowMenuIndex;

	return exportValues;
}


void AppHeaderChunk::importFromJson(Json::Value& importValues)
{
	if (importValues.isMember("common_property")) {
		Json::Value commonProperty = importValues["common_property"];
		value.commonFlag = 0;
		vector<wstring> commonPropertyStrList;
		for (UINT32 i = 0; i < commonProperty.size();i++) {
			wstring commonPropertyStrUni = getUniStr(commonProperty[i].asCString(), CodePage::CP_UTF_8);
			commonPropertyStrList.push_back(commonPropertyStrUni);
		}
		for (UINT32 i = 0; i < 0x10;i++) {
			if (contains(commonPropertyStrList, commonFlagStr[i])) {
				value.commonFlag |= (1 << i);
			}
		}
	}

	if (importValues.isMember("window_property")) {
		Json::Value windowProperty = importValues["window_property"];
		value.windowFlag = 0;
		vector<wstring> windowPropertyStrList;
		for (UINT32 i = 0; i < windowProperty.size(); i++) {
			wstring windowPropertyStrUni = getUniStr(windowProperty[i].asCString(), CodePage::CP_UTF_8);
			windowPropertyStrList.push_back(windowPropertyStrUni);
		}
		for (UINT32 i = 0; i < 0x10; i++) {
			if (contains(windowPropertyStrList, windowFlagStr[i])) {
				value.windowFlag |= (1 << i);
			}
		}
	}

	if (importValues.isMember("graphic_mode")) {
		value.graphicMode = (UINT16)importValues["graphic_mode"].asUInt();
	}

	if (importValues.isMember("device_property")) {
		Json::Value deviceProperty = importValues["device_property"];
		value.deviceFlag = 0;
		vector<wstring> devicePropertyStrList;
		for (UINT32 i = 0; i < deviceProperty.size(); i++) {
			wstring devicePropertyStrUni = getUniStr(deviceProperty[i].asCString(), CodePage::CP_UTF_8);
			devicePropertyStrList.push_back(devicePropertyStrUni);
		}
		for (UINT32 i = 0; i < 0x10; i++) {
			if (contains(devicePropertyStrList, deviceFlagStr[i])) {
				value.deviceFlag |= (1 << i);
			}
		}
	}

	if (importValues.isMember("window_width")) {
		value.windowWidth = (UINT16)importValues["window_width"].asUInt();
	}

	if (importValues.isMember("window_height")) {
		value.windowHeight = (UINT16)importValues["window_height"].asUInt();
	}

	if (importValues.isMember("initial_score")) {
		value.initialScore = (UINT16)importValues["initial_score"].asUInt();
	}

	if (importValues.isMember("initial_lives")) {
		value.initialLives = (UINT16)importValues["initial_lives"].asUInt();
	}

	if (importValues.isMember("constols")) {
		Json::Value controlList = importValues["constols"];
		UINT32 maxCount = (controlList.size() > 4) ? 4 : controlList.size();
		memset(&value.controls, 0, (maxCount * sizeof(PlayerControl)));
		for (UINT32 i = 0; i < maxCount;i++) {
			Json::Value playerControl = controlList[i];
			value.controls[i].controlType = (UINT16)playerControl["control_type"].asUInt();
			if (playerControl.isMember("keys")) {
				Json::Value playerKeys = playerControl["keys"];
				value.controls[i].keys.up = (UINT16)playerKeys["up"].asUInt();
				value.controls[i].keys.down = (UINT16)playerKeys["down"].asUInt();
				value.controls[i].keys.left = (UINT16)playerKeys["left"].asUInt();
				value.controls[i].keys.right = (UINT16)playerKeys["right"].asUInt();
				value.controls[i].keys.button1 = (UINT16)playerKeys["button1"].asUInt();
				value.controls[i].keys.button2 = (UINT16)playerKeys["button2"].asUInt();
				value.controls[i].keys.button3 = (UINT16)playerKeys["button3"].asUInt();
				value.controls[i].keys.button4 = (UINT16)playerKeys["button4"].asUInt();
			}
		}
	}

	if (importValues.isMember("border_color")) {
		wstring borderColorUni = getUniStr(importValues["border_color"].asCString(), CodePage::CP_UTF_8);
		value.borderColor.fromString(borderColorUni);
	}

	if (importValues.isMember("number_of_frames")) {
		value.numberOfFrames = (UINT32)importValues["number_of_frames"].asUInt();
	}

	if (importValues.isMember("frame_rate")) {
		value.frameRate = (UINT32)importValues["frame_rate"].asUInt();
	}

	if (importValues.isMember("window_menu_index")) {
		value.windowMenuIndex = (UINT32)importValues["window_menu_index"].asUInt();
	}
}


// 데이터 렌더링용
void AppHeaderChunk::renderData()
{
	stringstream oss;

	ImGui::Text("{");

	oss << "  size: " << value.size << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	ImGui::Text("  common_flag: [");

	vector<string> commonFlagPropertyStrings = getCommonFlagProperties(value.commonFlag);
	for (size_t i = 0; i < commonFlagPropertyStrings.size(); i++) {
		oss << "     " << commonFlagPropertyStrings[i];
		if ((i + 1) < commonFlagPropertyStrings.size()) {
			oss << ",";
		}
		ImGui::Text(oss.str().c_str());
		oss.str("");
	}

	ImGui::Text("  ],");

	vector<string> windowFlagPropertyStrings = getWindowFlagProperties(value.windowFlag);

	ImGui::Text("  window_flag: [");

	for (size_t i = 0; i < windowFlagPropertyStrings.size(); i++) {
		oss << "     " << windowFlagPropertyStrings[i];
		if ((i + 1) < windowFlagPropertyStrings.size()) {
			oss << ",";
		}
		ImGui::Text(oss.str().c_str());
		oss.str("");
	}

	ImGui::Text("  ],");

	ImGui::Text("  device_flag: [");

	vector<string> deviceFlagPropertyStrings = getDeviceFlagProperties(value.deviceFlag);
	for (size_t i = 0; i < deviceFlagPropertyStrings.size(); i++) {
		oss << "     " << deviceFlagPropertyStrings[i];
		if ((i + 1) < deviceFlagPropertyStrings.size()) {
			oss << ",";
		}
		ImGui::Text(oss.str().c_str());
		oss.str("");
	}

	ImGui::Text("  ],");

	oss << "  window_width: " << (UINT32)value.windowWidth << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "  window_height: " << (UINT32)value.windowHeight << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "  initial_score: " << value.initialScore << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "  initial_lives: " << value.initialLives << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "  controls: [";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	for (UINT32 i = 0; i < 4;i++) {


		oss << "    <" << i << ">{";
		ImGui::Text(oss.str().c_str());
		oss.str("");

		oss << "      control_type: " << (UINT32)value.controls[i].controlType << ",";
		ImGui::Text(oss.str().c_str());
		oss.str("");

		ImGui::Text("      keys: [");

		oss << "        up: " << (UINT32)value.controls[i].keys.up << ",";
		ImGui::Text(oss.str().c_str());
		oss.str("");

		oss << "        down: " << (UINT32)value.controls[i].keys.down << ",";
		ImGui::Text(oss.str().c_str());
		oss.str("");

		oss << "        left: " << (UINT32)value.controls[i].keys.left << ",";
		ImGui::Text(oss.str().c_str());
		oss.str("");

		oss << "        right: " << (UINT32)value.controls[i].keys.right << ",";
		ImGui::Text(oss.str().c_str());
		oss.str("");

		oss << "        button_1: " << (UINT32)value.controls[i].keys.button1 << ",";
		ImGui::Text(oss.str().c_str());
		oss.str("");

		oss << "        button_2: " << (UINT32)value.controls[i].keys.button2 << ",";
		ImGui::Text(oss.str().c_str());
		oss.str("");

		oss << "        button_3: " << (UINT32)value.controls[i].keys.button3 << ",";
		ImGui::Text(oss.str().c_str());
		oss.str("");

		oss << "        button_4: " << (UINT32)value.controls[i].keys.button4;
		ImGui::Text(oss.str().c_str());
		oss.str("");

		oss << "      ]";
		ImGui::Text(oss.str().c_str());
		oss.str("");

		if (i < 3) {
			ImGui::Text("    },");
		}
		else {
			ImGui::Text("    }");
		}

	}

	ImGui::Text("  ],");

	// 컬러 창은 나타내되 선택 불가능하도록
	oss << "  border_color:";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	ImGuiColorEditFlags colorEditFlags = ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel;
	Color borderColor = value.borderColor;
	ImVec4 borderColorNm = ImVec4((float(borderColor.r) / 255.0f), (float(borderColor.g) / 255.0f), (float(borderColor.b) / 255.0f), (float(borderColor.a) / 255.0f));
	// -- 같은 라인 --
	ImGui::SameLine();
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);
	ImGui::ColorEdit4("MyColor##1", (float*)&borderColorNm, colorEditFlags);
	// -- 같은 라인 --
	ImGui::SameLine(ImGui::CalcTextSize(u8"  border_color:").x + 36.0f);
	ImGui::Text(",");
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);

	oss << "  number_of_frames: " << value.numberOfFrames << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "  frame_rate: " << value.frameRate << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "  window_menu_index: " << value.windowMenuIndex;
	ImGui::Text(oss.str().c_str());
	oss.str("");

	ImGui::Text("}");

}