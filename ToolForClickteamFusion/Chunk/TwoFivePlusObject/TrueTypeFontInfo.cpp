#include "TrueTypeFontInfo.h"
#include "../../Archive/Archive.h"
#include "../../Util/ZlibUtil.h"
#include "../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::ZlibUtil;
using namespace CTT::CommonUtil;


TrueTypeFontInfo::TrueTypeFontInfo() {
	memset(&value, 0, sizeof(CommonTrueTypeFontInfo));
}


TrueTypeFontInfo::~TrueTypeFontInfo() {
}


bool TrueTypeFontInfo::exportJson(Json::Value& exportValue, vector<UINT32>& offsetList)
{
	// value로 추출
	exportValue["height"] = -(value.height);
	exportValue["width"] = value.width;
	exportValue["orientation"] = value.orientation;
	exportValue["escapement"] = value.escapement;
	exportValue["weight"] = value.weight;
	exportValue["italic"] = value.italic;
	exportValue["underline"] = value.underline;
	exportValue["strikeout"] = value.strikeout;
	exportValue["charset"] = value.charset;
	exportValue["out_precision"] = value.outPrecision;
	exportValue["clip_precision"] = value.clipPrecision;
	exportValue["quality"] = value.quality;
	exportValue["pitch_and_family"] = value.pitchAndFamily;
	exportValue["name"] = getAnsiStr(value.name, CodePage::CP_UTF_8);

	// fontPathName 지정
	INT32 offsetIndex = -1;
	if (contains(offsetList, value.offset, offsetIndex)) {
		wostringstream woss;
		woss << setw(6) << setfill(L'0') << offsetIndex;
		wstring fontPathName = woss.str() + L".ttf";
		exportValue["fontPathName"] = getAnsiStr(fontPathName, CodePage::CP_UTF_8);
	}
	else {
		exportValue["fontPathName"] = "--";
	}

	return true;
}


bool TrueTypeFontInfo::importJson(Json::Value& importValue, vector<UINT32>& offsetList)
{
	
	// value로부터 읽어들여 기록
	value.height = -(importValue["height"].asInt());
	value.width = importValue["width"].asUInt();
	value.escapement = importValue["escapement"].asUInt();
	value.orientation = importValue["orientation"].asUInt();
	value.weight = importValue["weight"].asUInt();
	value.italic = (BYTE)importValue["italic"].asUInt();
	value.underline = (BYTE)importValue["underline"].asUInt();
	value.strikeout = (BYTE)importValue["strikeout"].asUInt();
	value.charset = (BYTE)importValue["charset"].asUInt();
	value.outPrecision = (BYTE)importValue["out_precision"].asUInt();
	value.clipPrecision = (BYTE)importValue["clip_precision"].asUInt();
	value.quality = (BYTE)importValue["quality"].asUInt();
	value.pitchAndFamily = (BYTE)importValue["pitch_and_family"].asUInt();
	string nameAnsi = importValue["name"].asCString();
	wstring nameStr = getUniStr(nameAnsi, CodePage::CP_UTF_8);
	memcpy(value.name, nameStr.c_str(), ((0x1F < nameStr.size() ? 0x1F : nameStr.size()) * sizeof(WCHAR)));

	// fontPathName을 기반으로 offsetList에서 필요한 오프셋을 기록, 없거나 범위를 벗어나면 그냥 0으로 둔다
	string fontPathNameAnsi = importValue["fontPathName"].asCString();
	wstring fontPathName = getUniStr(fontPathNameAnsi, CodePage::CP_UTF_8);
	if (fontPathName != L"--") {
		vector<wstring> splited = split(fontPathName, L".");
		UINT32 offsetIndex = stoul(splited[0], NULL, 10);
		if (offsetIndex < offsetList.size()) {
			value.offset = offsetList[offsetIndex];
		}
	}

	return true;
}


bool TrueTypeFontInfo::loadFromBuffer(BYTE*& bufferPnt)
{
	memcpy(&value, bufferPnt, sizeof(CommonTrueTypeFontInfo));
	bufferPnt += sizeof(CommonTrueTypeFontInfo);
	return true;
}


bool TrueTypeFontInfo::saveIntoBuffer(BYTE*& bufferPnt)
{
	memcpy(bufferPnt, &value, sizeof(CommonTrueTypeFontInfo));
	bufferPnt += sizeof(CommonTrueTypeFontInfo);
	return true;
}


// 데이터 렌더링용 함수
void TrueTypeFontInfo::renderData(UINT32 index)
{
	stringstream oss;

	// 인덱스 표현
	oss << "  <" << index << ">{";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    height: " << -value.height << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    width: " << value.width << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    escapement: " << value.escapement << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    orientation: " << value.orientation << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    weight: " << value.weight << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    italic: " << (UINT32)value.italic << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    underline: " << (UINT32)value.underline << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    strikeout: " << (UINT32)value.strikeout << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    charset: " << (UINT32)value.charset << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    out_precision: " << (UINT32)value.outPrecision << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    clip_precision: " << (UINT32)value.clipPrecision << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    quality: " << (UINT32)value.quality << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    pitch_and_family: " << (UINT32)value.pitchAndFamily << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    name: \"" << getAnsiStr(value.name, CodePage::CP_UTF_8) << "\"";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	ImGui::Text("  }");
}