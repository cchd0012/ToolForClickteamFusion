#include "FrameLayer.h"
#include "../../Archive/Archive.h"
#include "../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;

// 비트 위치별로 포함되는 정보
wstring frameLayerFlagStr[] = {
	L"XCoefficient",
	L"YCoefficient",
	L"DoNotSaveBackground",
	L"",
	L"Visible",
	L"WrapHorizontally",
	L"WrapVertically",
	L"",
	L"",
	L"",
	L"",
	L"",
	L"",
	L"",
	L"",
	L"",
	L"Redraw",
	L"ToHide",
	L"ToShow",
	L"",
};


vector<string> CTT::Chunk::getFrameLayerFlagProperties(UINT32 frameLayerFlag)
{
	vector<string> result;
	for (UINT32 i = 0; i < 0x20; i++) {
		if ((frameLayerFlag & (1 << i)) != 0) {
			string flagStrAnsi = getAnsiStr(frameLayerFlagStr[i], CodePage::CP_UTF_8);
			result.push_back(flagStrAnsi);
		}
	}
	return result;
}


FrameLayer::FrameLayer() {
	memset(&info, 0, sizeof(FrameLayerInfo));
}


FrameLayer::~FrameLayer() {
}


bool FrameLayer::exportJson(Json::Value& exportValue, vector<UINT32>& offsetList)
{
	// value로 추출

	exportValue["name"] = getAnsiStr(name, CodePage::CP_UTF_8);

	Json::Value frameLayerFlagPropertyList;
	vector<string> frameLayerFlagPropertyStrings = getCommonFlagProperties(info.flag);
	for (size_t i = 0; frameLayerFlagPropertyStrings.size(); i++) {
		frameLayerFlagPropertyList.append(frameLayerFlagPropertyStrings[i]);
	}
	exportValue["frame_layer_property"] = frameLayerFlagPropertyList;

	exportValue["x_coeff"] = info.XCoeff;
	exportValue["y_coeff"] = info.YCoeff;
	exportValue["number_of_backgrounds"] = info.NumberOfBackgrounds;
	exportValue["backgroud_index"] = info.BackgroudIndex;

	return true;
}


bool FrameLayer::importJson(Json::Value& importValue, vector<UINT32>& offsetList)
{
	// value로부터 읽어들이기

	string nameAnsi = importValue["name"].asCString();
	name = getUniStr(nameAnsi, CodePage::CP_UTF_8);

	if (importValue.isMember("frame_layer_property")) {
		Json::Value frameLayerProperty = importValue["frame_layer_property"];
		info.flag = 0;
		vector<wstring> frameLayerPropertyStrList;
		for (UINT32 i = 0; i < frameLayerProperty.size(); i++) {
			wstring commonPropertyStrUni = getUniStr(frameLayerProperty[i].asCString(), CodePage::CP_UTF_8);
			frameLayerPropertyStrList.push_back(commonPropertyStrUni);
		}
		for (UINT32 i = 0; i < 0x20; i++) {
			if (contains(frameLayerPropertyStrList, frameLayerFlagStr[i])) {
				info.flag |= (1 << i);
			}
		}
	}

	info.XCoeff = importValue["x_coeff"].asFloat();
	info.YCoeff = importValue["y_coeff"].asFloat();
	info.NumberOfBackgrounds = importValue["number_of_backgrounds"].asUInt();
	info.BackgroudIndex = importValue["backgroud_index"].asUInt();

	return true;
}


bool FrameLayer::loadFromBuffer(BYTE*& bufferPnt)
{
	// info 확보
	memcpy(&info, bufferPnt, sizeof(FrameLayerInfo));
	bufferPnt += sizeof(FrameLayerInfo);

	// 이름 확보
	name = (WCHAR*)(bufferPnt);
	bufferPnt += ((name.length() + 1) * sizeof(WCHAR));

	return true;
}


bool FrameLayer::saveIntoBuffer(BYTE*& bufferPnt)
{
	// info 기록
	memcpy(bufferPnt, &info, sizeof(FrameLayerInfo));
	bufferPnt += sizeof(FrameLayerInfo);

	// 이름 기록
	memcpy(bufferPnt, name.data(), (name.length() * sizeof(WCHAR)));
	bufferPnt += ((name.length() + 1) * sizeof(WCHAR));

	return true;
}


// 데이터 렌더링용 함수
void FrameLayer::renderData(UINT32 index)
{
	stringstream oss;

	// 인덱스 표현
	oss << "  <" << index << ">{";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    name: \"" << getAnsiStr(name, CodePage::CP_UTF_8) << "\",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    flag: [";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	vector<string> frameLayerFlagPropertyStrings = getFrameLayerFlagProperties(info.flag);
	for (size_t i = 0; i < frameLayerFlagPropertyStrings.size(); i++) {
		oss << "      \"" << frameLayerFlagPropertyStrings[i] << "\"";
		if ((i + 1) < frameLayerFlagPropertyStrings.size()) {
			oss << ",";
		}
		ImGui::Text(oss.str().c_str());
		oss.str("");
	}

	oss << "    ]\n";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    x_coeff: " << info.XCoeff << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    y_coeff: " << info.YCoeff << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    number_of_backgrounds: " << info.NumberOfBackgrounds << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    backgroud_index: " << info.BackgroudIndex;
	ImGui::Text(oss.str().c_str());
	oss.str("");

	ImGui::Text("  }");
}