#include "ObjectText.h"
#include "../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::CommonUtil;


static const char* objectTextJsonType = "objectText";
static const UINT32 objectTextJsonVersion = 1;


ObjectTextTable::ObjectTextTable()
{
	loaded = false;
}


void ObjectTextTable::clear()
{
	entries.clear();
	indexByHandle.clear();
	loaded = false;
}


bool ObjectTextTable::addObject(INT32 handle, const string& name, ObjectCommon& common)
{
	ObjectTextEntry entry;
	entry.handle = handle;
	entry.name = name;
	ByteBuffer identifier = common.getIdentifier();
	entry.identifier = string(identifier.begin(), identifier.end());

	if (common.hasStrings()) {
		vector<wstring>& strings = common.getStrings();
		for (size_t i = 0; i < strings.size(); i++) {
			// 비어 있는 문자열은 번역할 것이 없으므로 모으지 않는다 (index 는 그대로 유지된다)
			if (strings[i].empty()) {
				continue;
			}
			ObjectTextString item;
			item.index = (INT32)i;
			item.original = strings[i];
			entry.strings.push_back(item);
		}
	}
	if (common.hasText()) {
		vector<ObjectTextParagraph>& paragraphs = common.getText().paragraphs;
		for (size_t i = 0; i < paragraphs.size(); i++) {
			if (paragraphs[i].text.empty()) {
				continue;
			}
			ObjectTextParagraphEntry item;
			item.index = (INT32)i;
			item.font = paragraphs[i].font;
			item.original = paragraphs[i].text;
			entry.paragraphs.push_back(item);
		}
	}

	if (entry.strings.empty() && entry.paragraphs.empty()) {
		return false;
	}
	entries.push_back(entry);
	indexByHandle[handle] = entries.size() - 1;
	return true;
}


const ObjectTextEntry* ObjectTextTable::findEntry(INT32 handle) const
{
	map<INT32, size_t>::const_iterator it = indexByHandle.find(handle);
	if (it == indexByHandle.end()) {
		return NULL;
	}
	return &entries[it->second];
}


bool ObjectTextTable::exportJson(Json::Value& value) const
{
	value = Json::Value(Json::objectValue);
	value["type"] = objectTextJsonType;
	value["version"] = objectTextJsonVersion;
	Json::Value objects(Json::arrayValue);
	for (size_t i = 0; i < entries.size(); i++) {
		const ObjectTextEntry& entry = entries[i];
		Json::Value object(Json::objectValue);
		object["handle"] = entry.handle;
		object["name"] = entry.name;
		object["identifier"] = entry.identifier;
		Json::Value strings(Json::arrayValue);
		for (size_t s = 0; s < entry.strings.size(); s++) {
			Json::Value item(Json::objectValue);
			item["index"] = entry.strings[s].index;
			item["original"] = getAnsiStr(entry.strings[s].original, CodePage::CP_UTF_8);
			item["translation"] = getAnsiStr(entry.strings[s].translation, CodePage::CP_UTF_8);
			strings.append(item);
		}
		object["strings"] = strings;
		Json::Value paragraphs(Json::arrayValue);
		for (size_t p = 0; p < entry.paragraphs.size(); p++) {
			Json::Value item(Json::objectValue);
			item["index"] = entry.paragraphs[p].index;
			item["font"] = entry.paragraphs[p].font;
			item["original"] = getAnsiStr(entry.paragraphs[p].original, CodePage::CP_UTF_8);
			item["translation"] = getAnsiStr(entry.paragraphs[p].translation, CodePage::CP_UTF_8);
			paragraphs.append(item);
		}
		object["paragraphs"] = paragraphs;
		objects.append(object);
	}
	value["objects"] = objects;
	return true;
}


bool ObjectTextTable::importJson(Json::Value& value)
{
	clear();
	if (!value.isObject() || (value["type"].asString() != objectTextJsonType) || !value["objects"].isArray()) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": object text json 형식이 아닙니다." << endl;
		return false;
	}
	const Json::Value& objects = value["objects"];
	for (Json::ArrayIndex i = 0; i < objects.size(); i++) {
		const Json::Value& object = objects[i];
		ObjectTextEntry entry;
		entry.handle = object["handle"].asInt();
		entry.name = object["name"].asString();
		entry.identifier = object["identifier"].asString();
		const Json::Value& strings = object["strings"];
		for (Json::ArrayIndex s = 0; s < strings.size(); s++) {
			ObjectTextString item;
			item.index = strings[s]["index"].asInt();
			item.original = getUniStr(strings[s]["original"].asString(), CodePage::CP_UTF_8);
			item.translation = getUniStr(strings[s]["translation"].asString(), CodePage::CP_UTF_8);
			entry.strings.push_back(item);
		}
		const Json::Value& paragraphs = object["paragraphs"];
		for (Json::ArrayIndex p = 0; p < paragraphs.size(); p++) {
			ObjectTextParagraphEntry item;
			item.index = paragraphs[p]["index"].asInt();
			item.font = (UINT16)paragraphs[p]["font"].asUInt();
			item.original = getUniStr(paragraphs[p]["original"].asString(), CodePage::CP_UTF_8);
			item.translation = getUniStr(paragraphs[p]["translation"].asString(), CodePage::CP_UTF_8);
			entry.paragraphs.push_back(item);
		}
		if (indexByHandle.find(entry.handle) != indexByHandle.end()) {
			continue;
		}
		entries.push_back(entry);
		indexByHandle[entry.handle] = entries.size() - 1;
	}
	loaded = true;
	return true;
}




bool CTT::Chunk::replaceObjectText(ObjectCommon& common, const ObjectTextEntry& entry, UINT32& replacedCount, UINT32& skippedCount)
{
	replacedCount = 0;
	skippedCount = 0;

	for (size_t i = 0; i < entry.strings.size(); i++) {
		const ObjectTextString& item = entry.strings[i];
		if (item.translation.empty() || (item.translation == item.original)) {
			continue;
		}
		if (!common.hasStrings() || (item.index < 0) || ((size_t)item.index >= common.getStrings().size()) || (common.getStrings()[item.index] != item.original)) {
			skippedCount++;
			continue;
		}
		common.getStrings()[item.index] = item.translation;
		common.markStringsChanged();
		replacedCount++;
	}

	for (size_t i = 0; i < entry.paragraphs.size(); i++) {
		const ObjectTextParagraphEntry& item = entry.paragraphs[i];
		if (item.translation.empty() || (item.translation == item.original)) {
			continue;
		}
		if (!common.hasText() || (item.index < 0) || ((size_t)item.index >= common.getText().paragraphs.size()) || (common.getText().paragraphs[item.index].text != item.original)) {
			skippedCount++;
			continue;
		}
		common.getText().paragraphs[item.index].text = item.translation;
		common.markTextChanged();
		replacedCount++;
	}
	return true;
}
