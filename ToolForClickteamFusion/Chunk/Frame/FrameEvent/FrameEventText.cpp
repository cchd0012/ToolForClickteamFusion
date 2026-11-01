#include "FrameEventText.h"
#include "../../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::CommonUtil;


static const char* frameEventTextJsonType = "frameEventText";
static const UINT32 frameEventTextJsonVersion = 1;


// 모든 그룹의 모든 조건/액션의 파라미터마다 콜백을 호출한다
template <typename Callback>
static void forEachFrameEventParameter(vector<FrameEventDataGroup>& groups, Callback callback)
{
	for (size_t g = 0; g < groups.size(); g++) {
		for (size_t c = 0; c < groups[g].conditions.size(); c++) {
			for (size_t p = 0; p < groups[g].conditions[c].params.size(); p++) {
				callback(groups[g].conditions[c].params[p]);
			}
		}
		for (size_t a = 0; a < groups[g].actions.size(); a++) {
			for (size_t p = 0; p < groups[g].actions[a].params.size(); p++) {
				callback(groups[g].actions[a].params[p]);
			}
		}
	}
}


FrameEventTextTable::FrameEventTextTable()
{
	loaded = false;
}


void FrameEventTextTable::clear()
{
	entries.clear();
	indexByOriginal.clear();
	loaded = false;
}


UINT32 FrameEventTextTable::addFrameEventText(const FrameEventChunkModel& model, UINT32 frameIndex)
{
	UINT32 tokenCount = 0;
	for (size_t b = 0; b < model.blocks.size(); b++) {
		const vector<FrameEventDataGroup>& groups = model.blocks[b].groups;
		for (size_t g = 0; g < groups.size(); g++) {
			// 그룹의 모든 조건/액션의 모든 파라미터
			vector<const FrameEventParameter*> params;
			for (size_t c = 0; c < groups[g].conditions.size(); c++) {
				for (size_t p = 0; p < groups[g].conditions[c].params.size(); p++) {
					params.push_back(&groups[g].conditions[c].params[p]);
				}
			}
			for (size_t a = 0; a < groups[g].actions.size(); a++) {
				for (size_t p = 0; p < groups[g].actions[a].params.size(); p++) {
					params.push_back(&groups[g].actions[a].params[p]);
				}
			}
			for (size_t i = 0; i < params.size(); i++) {
				if (!isFrameEventExpressionParameter(params[i]->code)) {
					continue;
				}
				FrameEventExpression expression;
				string errorMessage;
				if (!parseFrameEventExpression(params[i]->body, expression, errorMessage)) {
					continue;
				}
				for (size_t t = 0; t < expression.tokens.size(); t++) {
					wstring text;
					if (!getFrameEventExpressionString(expression.tokens[t], text) || text.empty()) {
						continue;
					}
					tokenCount++;
					map<wstring, size_t>::const_iterator it = indexByOriginal.find(text);
					size_t index;
					if (it == indexByOriginal.end()) {
						FrameEventTextEntry entry;
						entry.original = text;
						entry.firstFrame = frameIndex;
						entry.lastFrame = frameIndex;
						entry.frameCount = 1;
						entries.push_back(entry);
						index = entries.size() - 1;
						indexByOriginal[text] = index;
					}
					else {
						index = it->second;
						if (entries[index].lastFrame != frameIndex) {
							entries[index].lastFrame = frameIndex;
							entries[index].frameCount++;
						}
					}
					entries[index].count++;
				}
			}
		}
	}
	return tokenCount;
}


const wstring* FrameEventTextTable::findTranslation(const wstring& original) const
{
	map<wstring, size_t>::const_iterator it = indexByOriginal.find(original);
	if (it == indexByOriginal.end()) {
		return NULL;
	}
	const FrameEventTextEntry& entry = entries[it->second];
	if (entry.translation.empty() || (entry.translation == entry.original)) {
		return NULL;
	}
	return &entry.translation;
}


bool FrameEventTextTable::exportJson(Json::Value& value) const
{
	value = Json::Value(Json::objectValue);
	value["type"] = frameEventTextJsonType;
	value["version"] = frameEventTextJsonVersion;
	Json::Value texts(Json::arrayValue);
	for (size_t i = 0; i < entries.size(); i++) {
		const FrameEventTextEntry& entry = entries[i];
		Json::Value item(Json::objectValue);
		item["original"] = getAnsiStr(entry.original, CodePage::CP_UTF_8);
		item["translation"] = getAnsiStr(entry.translation, CodePage::CP_UTF_8);
		item["count"] = entry.count;
		item["frameCount"] = entry.frameCount;
		item["firstFrame"] = entry.firstFrame;
		texts.append(item);
	}
	value["texts"] = texts;
	return true;
}


bool FrameEventTextTable::importJson(Json::Value& value)
{
	clear();
	if (!value.isObject() || (value["type"].asString() != frameEventTextJsonType) || !value["texts"].isArray()) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": frame event text json 형식이 아닙니다." << endl;
		return false;
	}
	const Json::Value& texts = value["texts"];
	for (Json::ArrayIndex i = 0; i < texts.size(); i++) {
		const Json::Value& item = texts[i];
		FrameEventTextEntry entry;
		entry.original = getUniStr(item["original"].asString(), CodePage::CP_UTF_8);
		entry.translation = getUniStr(item["translation"].asString(), CodePage::CP_UTF_8);
		entry.count = item["count"].asUInt();
		entry.frameCount = item["frameCount"].asUInt();
		entry.firstFrame = item["firstFrame"].asUInt();
		if (entry.original.empty() || (indexByOriginal.find(entry.original) != indexByOriginal.end())) {
			continue;
		}
		entries.push_back(entry);
		indexByOriginal[entry.original] = entries.size() - 1;
	}
	loaded = true;
	return true;
}




bool CTT::Chunk::replaceFrameEventText(FrameEventChunkModel& model, const FrameEventTextTable& table, UINT32& replacedCount, string& errorMessage)
{
	replacedCount = 0;
	try {
		for (size_t b = 0; b < model.blocks.size(); b++) {
			forEachFrameEventParameter(model.blocks[b].groups, [&](FrameEventParameter& param) {
				if (!isFrameEventExpressionParameter(param.code)) {
					return;
				}
				FrameEventExpression expression;
				string parseError;
				if (!parseFrameEventExpression(param.body, expression, parseError)) {
					return;
				}
				bool changed = false;
				for (size_t t = 0; t < expression.tokens.size(); t++) {
					wstring text;
					if (!getFrameEventExpressionString(expression.tokens[t], text) || text.empty()) {
						continue;
					}
					const wstring* translation = table.findTranslation(text);
					if (translation != NULL) {
						setFrameEventExpressionString(expression.tokens[t], *translation);
						changed = true;
						replacedCount++;
					}
				}
				if (changed) {
					param.body = buildFrameEventExpression(expression);
				}
			});
		}
	}
	catch (const exception& e) {
		errorMessage = e.what();
		return false;
	}
	return true;
}
