#include "FrameObject.h"
#include "../../Archive/Archive.h"
#include "../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;


FrameObject::FrameObject() {
	memset(&info, 0, sizeof(FrameObjectInfo));
}


FrameObject::~FrameObject() {
}


bool FrameObject::exportJson(Json::Value& exportValue, vector<UINT32>& offsetList)
{
	// value로 추출

	exportValue["handle"] = info.handle;
	exportValue["object_info"] = info.objectInfo;
	exportValue["pos_x"] = info.posX;
	exportValue["pos_y"] = info.posY;
	exportValue["parent_type"] = info.parentType;
	exportValue["layer"] = info.layer;
	exportValue["instance"] = info.instance;
	exportValue["parent_handle"] = info.parentHandle;

	return true;
}


bool FrameObject::importJson(Json::Value& importValue, vector<UINT32>& offsetList)
{
	// value로부터 읽어들이기

	info.handle = (UINT16)importValue["handle"].asUInt();
	info.objectInfo = (UINT16)importValue["object_info"].asUInt();
	info.posX = importValue["pos_x"].asInt();
	info.posY = importValue["pos_y"].asInt();
	info.parentType = (UINT16)importValue["parent_type"].asUInt();
	info.layer = (UINT16)importValue["layer"].asUInt();
	info.instance = (UINT16)importValue["instance"].asUInt();
	info.parentHandle = (UINT16)importValue["parent_handle"].asUInt();

	return true;
}


bool FrameObject::loadFromBuffer(BYTE*& bufferPnt)
{
	// info 확보
	memcpy(&info, bufferPnt, sizeof(FrameObjectInfo));
	bufferPnt += sizeof(FrameObjectInfo);
	return true;
}


bool FrameObject::saveIntoBuffer(BYTE*& bufferPnt)
{
	// info 기록
	memcpy(bufferPnt, &info, sizeof(FrameObjectInfo));
	bufferPnt += sizeof(FrameObjectInfo);
	return true;
}


// 데이터 렌더링용 함수
void FrameObject::renderData(UINT32 index)
{
	stringstream oss;

	// 인덱스 표현
	oss << "  <" << index << ">{";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    handle: " << info.handle << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    object_info: " << info.objectInfo << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    pos_x: " << info.posX << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    pos_y: " << info.posY << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    parent_type: " << info.parentType << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    layer: " << info.layer << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    instance: " << info.instance << ",";
	ImGui::Text(oss.str().c_str());
	oss.str("");

	oss << "    parent_handle: " << info.parentHandle;
	ImGui::Text(oss.str().c_str());
	oss.str("");

	ImGui::Text("  }");
}