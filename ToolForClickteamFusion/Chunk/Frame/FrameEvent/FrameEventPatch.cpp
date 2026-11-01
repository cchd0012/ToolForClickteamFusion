#include "FrameEventPatch.h"
#include "../../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::CommonUtil;


static const char* parserSkeletonJsonType = "parserSkeleton";
static const UINT32 parserSkeletonJsonVersion = 1;


// ------------------------------------------------------------------ 토큰 보조 함수

// 오브젝트의 문자열 값 토큰인지 (objectType >= 2, num 19, 본문 = [u16 objectInfo][i16 objectList][i16 문자열 번호])
static bool isObjectStringToken(const FrameEventExpressionToken& token, UINT16& objectInfo, INT16& index)
{
	if ((token.objectType < 2) || (token.num != 19) || (token.body.size() != 6)) {
		return false;
	}
	objectInfo = (UINT16)(token.body[0] | (token.body[1] << 8));
	index = (INT16)(token.body[4] | (token.body[5] << 8));
	return true;
}


// 확장 오브젝트(objectType >= 32)의 식 토큰인지 (본문 앞 2바이트 = objectInfo)
static bool isExtensionToken(const FrameEventExpressionToken& token, UINT16& objectInfo)
{
	if ((token.objectType < 32) || (token.body.size() < 4)) {
		return false;
	}
	objectInfo = (UINT16)(token.body[0] | (token.body[1] << 8));
	return true;
}


static void setObjectStringTokenIndex(FrameEventExpressionToken& token, INT16 index)
{
	token.body[4] = (BYTE)(index & 0xFF);
	token.body[5] = (BYTE)((index >> 8) & 0xFF);
}


static bool parseExpressionParam(const FrameEventParameter& param, FrameEventExpression& expression)
{
	if (!isFrameEventExpressionParameter(param.code)) {
		return false;
	}
	string errorMessage;
	return parseFrameEventExpression(param.body, expression, errorMessage);
}


// 비-ASCII 글자를 대체 글자로 바꾼 뼈대 문자열
static wstring makeSkeleton(const wstring& text, const wstring& placeholder)
{
	wstring result;
	for (size_t i = 0; i < text.size(); i++) {
		if (text[i] < 0x80) {
			result.push_back(text[i]);
		}
		else {
			result.append(placeholder);
		}
	}
	return result;
}


// ------------------------------------------------------------------ 감지

void CTT::Chunk::detectParserSkeletonTargets(const FrameEventChunkModel& model, vector<ParserSkeletonTarget>& targets)
{
	for (size_t b = 0; b < model.blocks.size(); b++) {
		const vector<FrameEventDataGroup>& groups = model.blocks[b].groups;
		for (size_t g = 0; g < groups.size(); g++) {
			for (size_t c = 0; c < groups[g].conditions.size(); c++) {
				const FrameEventCondition& condition = groups[g].conditions[c];
				if (condition.params.size() != 2) {
					continue;
				}
				FrameEventExpression left, right;
				if (!parseExpressionParam(condition.params[0], left) || !parseExpressionParam(condition.params[1], right)) {
					continue;
				}
				// 왼쪽: 오브젝트 문자열 하나
				UINT16 textObject;
				INT16 stringIndex;
				if ((left.tokens.size() != 1) || !isObjectStringToken(left.tokens[0], textObject, stringIndex)) {
					continue;
				}
				// 오른쪽: 확장 오브젝트의 식이 있고, 같은 문자열이 다시 들어 있다 (길이만큼 잘라 비교하는 형태)
				UINT16 parserObject = 0;
				bool hasExtension = false, hasSameString = false;
				for (size_t t = 0; t < right.tokens.size(); t++) {
					UINT16 info;
					INT16 index;
					if (isExtensionToken(right.tokens[t], info)) {
						hasExtension = true;
						parserObject = info;
					}
					else if (isObjectStringToken(right.tokens[t], info, index) && (info == textObject) && (index == stringIndex)) {
						hasSameString = true;
					}
				}
				if (!hasExtension || !hasSameString) {
					continue;
				}
				bool exists = false;
				for (size_t k = 0; k < targets.size(); k++) {
					if ((targets[k].textObject == textObject) && (targets[k].stringIndex == stringIndex) && (targets[k].parserObject == parserObject)) {
						exists = true;
						break;
					}
				}
				if (!exists) {
					ParserSkeletonTarget target;
					target.textObject = textObject;
					target.stringIndex = stringIndex;
					target.parserObject = parserObject;
					targets.push_back(target);
				}
			}
		}
	}
}


// ------------------------------------------------------------------ 적용

// 식 하나 안에서 대상 문자열 토큰을 뼈대 번호로 바꾼다. 바꾼 토큰 수를 돌려준다
static UINT32 retargetExpression(FrameEventExpression& expression, const ParserSkeletonTarget& target)
{
	UINT32 count = 0;
	for (size_t t = 0; t < expression.tokens.size(); t++) {
		UINT16 info;
		INT16 index;
		if (isObjectStringToken(expression.tokens[t], info, index) && (info == target.textObject) && (index == target.stringIndex)) {
			setObjectStringTokenIndex(expression.tokens[t], target.skeletonIndex);
			count++;
		}
	}
	return count;
}


static UINT32 retargetParameter(FrameEventParameter& param, const ParserSkeletonTarget& target)
{
	FrameEventExpression expression;
	if (!parseExpressionParam(param, expression)) {
		return 0;
	}
	UINT32 count = retargetExpression(expression, target);
	if (count > 0) {
		param.body = buildFrameEventExpression(expression);
	}
	return count;
}


static bool conditionUsesExtension(const FrameEventCondition& condition, UINT16 parserObject)
{
	for (size_t p = 0; p < condition.params.size(); p++) {
		FrameEventExpression expression;
		if (!parseExpressionParam(condition.params[p], expression)) {
			continue;
		}
		for (size_t t = 0; t < expression.tokens.size(); t++) {
			UINT16 info;
			if (isExtensionToken(expression.tokens[t], info) && (info == parserObject)) {
				return true;
			}
		}
	}
	return false;
}


static INT16 readShort(const ByteBuffer& body)
{
	return (body.size() >= 2) ? (INT16)(body[0] | (body[1] << 8)) : (INT16)-1;
}


static void writeShort(ByteBuffer& body, INT16 value)
{
	if (body.size() < 2) {
		body.resize(2, 0);
	}
	body[0] = (BYTE)(value & 0xFF);
	body[1] = (BYTE)((value >> 8) & 0xFF);
}


bool CTT::Chunk::applyParserSkeletonPatch(FrameEventChunkModel& model, const ParserSkeletonConfig& config, ParserSkeletonStats& stats, string& errorMessage)
{
	try {
		for (size_t k = 0; k < config.targets.size(); k++) {
			const ParserSkeletonTarget& target = config.targets[k];
			if (target.skeletonIndex < 0) {
				continue;
			}
			for (size_t b = 0; b < model.blocks.size(); b++) {
				vector<FrameEventDataGroup>& groups = model.blocks[b].groups;
				for (size_t g = 0; g < groups.size(); g++) {
					FrameEventDataGroup& group = groups[g];

					// 1) 대사를 지정하는 동작 바로 뒤에 뼈대 지정 동작을 넣는다 (이미 있으면 갱신한다)
					vector<FrameEventAction> actions;
					for (size_t a = 0; a < group.actions.size(); a++) {
						const FrameEventAction& action = group.actions[a];
						bool isAssign = (action.objectInfo == target.textObject) && (action.num == 49) && (action.params.size() >= 2)
							&& (readShort(action.params[0].body) == target.stringIndex)
							&& isFrameEventExpressionParameter(action.params[1].code);
						// 직전에 만든 뼈대 지정 동작은 건너뛴다
						bool isSkeleton = (action.objectInfo == target.textObject) && (action.num == 49) && (action.params.size() >= 2)
							&& (readShort(action.params[0].body) == target.skeletonIndex);
						if (isSkeleton) {
							continue;	// 원본에 이미 있던 뼈대 동작(다시 적용하는 경우)은 아래에서 새로 만든다
						}
						actions.push_back(action);
						if (!isAssign) {
							continue;
						}
						FrameEventAction skeleton = action;
						writeShort(skeleton.params[0].body, target.skeletonIndex);
						FrameEventExpression expression;
						if (parseExpressionParam(skeleton.params[1], expression) && (expression.tokens.size() == 1) && expression.tokens[0].isString()) {
							wstring text;
							getFrameEventExpressionString(expression.tokens[0], text);
							setFrameEventExpressionString(expression.tokens[0], makeSkeleton(text, config.placeholder));
							skeleton.params[1].body = buildFrameEventExpression(expression);
							stats.skeletonActions++;
						}
						else {
							// 상수 문자열이 아닌 대사 지정: 원문 식을 그대로 복사한다 (비-ASCII 이면 한계가 있다)
							stats.copiedActions++;
						}
						actions.push_back(skeleton);
					}
					group.actions.swap(actions);

					// 2) 확장 오브젝트에 대사를 넘기는 동작이 뼈대를 읽게 한다
					for (size_t a = 0; a < group.actions.size(); a++) {
						FrameEventAction& action = group.actions[a];
						if (action.objectInfo != target.parserObject) {
							continue;
						}
						for (size_t p = 0; p < action.params.size(); p++) {
							stats.retargetedTokens += retargetParameter(action.params[p], target);
						}
					}

					// 3) 확장 오브젝트의 식을 쓰는 조건(대사 변경 검사 등)이 뼈대를 읽게 한다
					for (size_t c = 0; c < group.conditions.size(); c++) {
						FrameEventCondition& condition = group.conditions[c];
						if (!conditionUsesExtension(condition, target.parserObject)) {
							continue;
						}
						for (size_t p = 0; p < condition.params.size(); p++) {
							stats.retargetedTokens += retargetParameter(condition.params[p], target);
						}
					}
				}
			}
		}
	}
	catch (const exception& e) {
		errorMessage = e.what();
		return false;
	}
	return true;
}


// ------------------------------------------------------------------ 설정 json

void CTT::Chunk::exportParserSkeletonConfig(const ParserSkeletonConfig& config, Json::Value& value)
{
	value = Json::Value(Json::objectValue);
	value["type"] = parserSkeletonJsonType;
	value["version"] = parserSkeletonJsonVersion;
	value["enabled"] = config.enabled;
	value["placeholder"] = getAnsiStr(config.placeholder, CodePage::CP_UTF_8);
	Json::Value targets(Json::arrayValue);
	for (size_t i = 0; i < config.targets.size(); i++) {
		const ParserSkeletonTarget& target = config.targets[i];
		Json::Value item(Json::objectValue);
		item["textObject"] = (Json::UInt)target.textObject;
		if (!target.textObjectName.empty()) {
			item["textObjectName"] = target.textObjectName;
		}
		item["stringIndex"] = (Json::Int)target.stringIndex;
		item["parserObject"] = (Json::UInt)target.parserObject;
		if (!target.parserObjectName.empty()) {
			item["parserObjectName"] = target.parserObjectName;
		}
		targets.append(item);
	}
	value["targets"] = targets;
}


bool CTT::Chunk::importParserSkeletonConfig(const Json::Value& value, ParserSkeletonConfig& config)
{
	config = ParserSkeletonConfig();
	if (!value.isObject() || (value["type"].asString() != parserSkeletonJsonType) || !value["targets"].isArray()) {
		return false;
	}
	config.enabled = value.get("enabled", true).asBool();
	wstring placeholder = getUniStr(value.get("placeholder", "x").asString(), CodePage::CP_UTF_8);
	if (!placeholder.empty()) {
		config.placeholder = placeholder;
	}
	const Json::Value& targets = value["targets"];
	for (Json::ArrayIndex i = 0; i < targets.size(); i++) {
		ParserSkeletonTarget target;
		target.textObject = (UINT16)targets[i]["textObject"].asUInt();
		target.stringIndex = (INT16)targets[i]["stringIndex"].asInt();
		target.parserObject = (UINT16)targets[i]["parserObject"].asUInt();
		target.textObjectName = targets[i].get("textObjectName", "").asString();
		target.parserObjectName = targets[i].get("parserObjectName", "").asString();
		config.targets.push_back(target);
	}
	return true;
}
