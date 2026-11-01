#include "LogFont.h"
#include "../../Archive/Archive.h"
#include "../../Util/ZlibUtil.h"
#include "../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::ZlibUtil;
using namespace CTT::CommonUtil;


LogFont::LogFont() {
	handle = 0;
	originalSize = 0;
	memset(&info, 0, sizeof(CommonLogFontInfo));
	compressedData = NULL;
	compressedSize = 0;
}


LogFont::~LogFont() {
}


bool LogFont::exportJson(Json::Value& value, vector<UINT32>& offsetList)
{

	// value로 추출

	value["handle"] = handle;
	value["checksum"] = info.checksum;
	value["reference"] = info.reference;
	value["org_size"] = info.orgSize;
	value["height"] = info.height;
	value["width"] = info.width;
	value["escapement"] = info.escapement;
	value["orientation"] = info.orientation;
	value["weight"] = info.weight;
	value["italic"] = info.italic;
	value["underline"] = info.underline;
	value["strikeout"] = info.strikeout;
	value["charset"] = info.charset;
	value["out_precision"] = info.outPrecision;
	value["clip_precision"] = info.clipPrecision;
	value["quality"] = info.quality;
	value["pitch_and_family"] = info.pitchAndFamily;
	value["name"] = getAnsiStr(info.name, CodePage::CP_UTF_8);

	return true;
}


bool LogFont::importJson(Json::Value& value, vector<UINT32>& offsetList)
{

	// value로부터 읽어들이기

	handle = value["handle"].asUInt();
	info.checksum = value["checksum"].asUInt();
	info.reference = value["reference"].asUInt();
	info.orgSize = value["org_size"].asUInt();
	info.height = value["height"].asInt();
	info.width = value["width"].asUInt();
	info.escapement = value["escapement"].asUInt();
	info.orientation = value["orientation"].asUInt();
	info.weight = value["weight"].asUInt();
	info.italic = (BYTE)value["italic"].asUInt();
	info.underline = (BYTE)value["underline"].asUInt();
	info.strikeout = (BYTE)value["strikeout"].asUInt();
	info.charset = (BYTE)value["charset"].asUInt();
	info.outPrecision = (BYTE)value["out_precision"].asUInt();
	info.clipPrecision = (BYTE)value["clip_precision"].asUInt();
	info.quality = (BYTE)value["quality"].asUInt();
	info.pitchAndFamily = (BYTE)value["pitch_and_family"].asUInt();
	string nameAnsi = value["name"].asCString();
	wstring nameStr = getUniStr(nameAnsi, CodePage::CP_UTF_8);
	memcpy(info.name, nameStr.c_str(), ((0x1F < nameStr.size() ? 0x1F : nameStr.size()) * sizeof(WCHAR)));

	return true;
}


bool LogFont::loadFromBuffer(BYTE*& bufferPnt)
{
	handle = *(UINT32*)bufferPnt;
	if (getArchive()->getBuildVersion() >= 0x11C) {
		handle--;
	}
	bufferPnt += sizeof(UINT32);

	originalSize = *(UINT32*)bufferPnt;
	bufferPnt += sizeof(UINT32);

	if (compressedData != NULL) {
		free(compressedData);
		compressedData = NULL;
	}
	compressedSize = 0;

	compressedSize = *(UINT32*)bufferPnt;
	bufferPnt += sizeof(UINT32);

	compressedData = (BYTE*)malloc(compressedSize);
	memcpy(compressedData, bufferPnt, compressedSize);
	bufferPnt += compressedSize;

	// 용량도 얼마 안 되는데 그냥 여기서 처리하자
	return deriveFontData();
}


bool LogFont::saveIntoBuffer(BYTE*& bufferPnt)
{
	// 용량도 얼마 안 되는데 그냥 여기서 처리하자
	if (!deriveCompressedData()) {
		return false;
	}

	*(UINT32*)bufferPnt = handle;
	if (getArchive()->getBuildVersion() >= 0x11C) {
		(*(UINT32*)(bufferPnt))++;
	}
	bufferPnt += sizeof(UINT32);

	*(UINT32*)bufferPnt = originalSize;
	bufferPnt += sizeof(UINT32);

	*(UINT32*)bufferPnt = compressedSize;
	bufferPnt += sizeof(UINT32);

	memcpy(bufferPnt, compressedData, compressedSize);
	bufferPnt += compressedSize;

	return true;
}


// 데이터 렌더링용 함수
void LogFont::renderData(UINT32 index)
{
	stringstream oss;

	// 인덱스 표현
	oss << "  <" << index << ">{";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    checksum: " << info.checksum << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    reference: " << info.reference << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    org_size: " << info.orgSize << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    height: " << info.height << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    width: " << info.width << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    escapement: " << info.escapement << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    orientation: " << info.orientation << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    weight: " << info.weight << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    italic: " << (UINT32)info.italic << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    underline: " << (UINT32)info.underline << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    strikeout: " << (UINT32)info.strikeout << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    charset: " << (UINT32)info.charset << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    out_precision: " << (UINT32)info.outPrecision << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    clip_precision: " << (UINT32)info.clipPrecision << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    quality: " << (UINT32)info.quality << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    pitch_and_family: " << (UINT32)info.pitchAndFamily << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    name: \"" << getAnsiStr(info.name, CodePage::CP_UTF_8) << "\"";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	ImGui::Text("  }");
}


bool LogFont::deriveFontData()
{
	if ((compressedData == NULL) || (compressedSize == 0)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 디코딩을 수행할 데이터가 없습니다." << endl;
		return false;
	}

	// 압축 해제용 변수
	UINT32 tmpSize = originalSize;
	BYTE* tmpData = NULL;

	// 압축푼 데이터: [CommonFontInfo], 전체 크기는 originalSize 에 저장됨
	if (!ZlibUtil::decode(compressedData, compressedSize, tmpData, tmpSize)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 사운드 데이터 디코딩을 실패했습니다." << endl;
		if (tmpData != NULL) {
			free(tmpData);
		}
		return false;
	}

	// 정보 반영
	memcpy(&info, tmpData, sizeof(CommonLogFontInfo));
	info.height = -(info.height);

	// 역할 끝난 임시변수 제거
	free(tmpData);

	// 메모리 관리를 위하여 압축 데이터 제거
	free(compressedData);
	compressedData = NULL;
	compressedSize = 0;

	return true;
}


bool LogFont::deriveCompressedData()
{
	if (compressedData != NULL) {
		free(compressedData);
		compressedData = NULL;
	}
	compressedSize = 0;

	UINT32 tmpSize = 0;
	BYTE* tmpData = NULL;

	// 정보 및 크기 세팅, info.orgSize는 그냥 0으로 저장된다
	info.height = -(info.height);
	originalSize = sizeof(CommonLogFontInfo);
	info.orgSize = 0;

	// 이름과 fontData를 그대로 복사해서 세팅한 후 압축 수행
	tmpSize = originalSize;
	tmpData = (BYTE*)malloc(tmpSize);
	memset(tmpData, 0, tmpSize);
	memcpy(tmpData, &info, sizeof(CommonLogFontInfo));
	if (!ZlibUtil::encode(tmpData, tmpSize, compressedData, compressedSize)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": LogFont 데이터 인코딩을 실패했습니다." << endl;
		return false;
	}

	// 필요없는 버퍼들 추가 해제
	free(tmpData);

	return true;
}