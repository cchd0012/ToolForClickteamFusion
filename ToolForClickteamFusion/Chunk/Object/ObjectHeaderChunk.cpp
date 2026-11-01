#include "ObjectHeaderChunk.h"
#include "../../Util/JsonUtil.h"
#include "../../Util/CommonUtil.h"
#include "../../ImGUI/Headers.h"

using namespace CTT::Chunk;
using namespace CTT::JsonUtil;
using namespace CTT::CommonUtil;


ObjectHeaderChunk::ObjectHeaderChunk()
{
	memset(&value, 0, sizeof(ObjectHeader));
}


ObjectHeaderChunk::~ObjectHeaderChunk()
{
}


bool ObjectHeaderChunk::loadFromOrgBuffer()
{
	// chunk 데이터: [ObjectHeader]
	memcpy(&value, orgBuffer, sizeof(ObjectHeader));

	free(orgBuffer);
	orgBuffer = NULL;
	return true;
}


bool ObjectHeaderChunk::saveIntoOrgBuffer()
{
	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}
	orgSize = sizeof(ObjectHeader);
	orgBuffer = (BYTE*)malloc(orgSize);

	memcpy(orgBuffer, &value, sizeof(ObjectHeader));

	return true;
}


Json::Value ObjectHeaderChunk::exportAsJson()
{
	Json::Value exportValues;

	exportValues["handle"] = value.handle;

	exportValues["type"] = value.type;

	exportValues["flag1"] = value.flag1;

	exportValues["flag2"] = value.flag2;

	exportValues["unk1"] = value.unk1;

	exportValues["ink_effect"] = value.shaderIndex;

	exportValues["flag3"] = value.flag3;

	exportValues["unk2"] = value.unk2;

	wstring colorUni = value.color.toString();
	exportValues["color"] = getAnsiStr(colorUni, CodePage::CP_UTF_8);

	return exportValues;
}


void ObjectHeaderChunk::importFromJson(Json::Value& importValues)
{

	if (importValues.isMember("handle")) {
		value.handle = (UINT16)importValues["handle"].asUInt();
	}

	if (importValues.isMember("type")) {
		value.type = (UINT16)importValues["type"].asUInt();
	}

	if (importValues.isMember("flag1")) {
		value.flag1 = (BYTE)importValues["flag1"].asUInt();
	}

	if (importValues.isMember("flag2")) {
		value.flag2 = (BYTE)importValues["flag2"].asUInt();
	}

	if (importValues.isMember("unk1")) {
		value.unk1 = (UINT16)importValues["unk1"].asUInt();
	}

	if (importValues.isMember("ink_effect")) {
		value.shaderIndex = (BYTE)importValues["ink_effect"].asUInt();
	}

	if (importValues.isMember("flag3")) {
		value.flag3 = (BYTE)importValues["flag3"].asUInt();
	}

	if (importValues.isMember("unk2")) {
		value.unk2 = (UINT16)importValues["unk2"].asUInt();
	}

	if (importValues.isMember("color")) {
		string colorAnsi = importValues["color"].asCString();
		value.color.fromString(getUniStr(colorAnsi, CodePage::CP_UTF_8));
	}
}


// 데이터 렌더링용
void ObjectHeaderChunk::renderData()
{
	ImGui::TextDisabled(u8"--");
}