#include <cstring>
#include "FrameEventTextFlow.h"
#include "../../../Util/CommonUtil.h"

using namespace CTT::Chunk;
using namespace CTT::CommonUtil;


static const char* frameEventTextFlowJsonType = "textFlow";
static const UINT32 frameEventTextFlowJsonVersion = 1;

// txtblt.mfx 의 "텍스트 설정" 동작 번호. 같은 모듈은 어느 게임에서나 같은 번호를 쓴다
static const INT32 kTextBlitterSetTextAction = 80;
// 폰트 이미지를 실행 중에 바꾸는 동작: 파일에서 불러오기 / 이미지 폭 / 이미지 높이
static const INT32 kTextBlitterLoadImageAction = 93;
static const INT32 kTextBlitterImageWidthAction = 95;
static const INT32 kTextBlitterImageHeightAction = 96;
// 오브젝트의 알터러블 문자열을 설정하는 동작 (오브젝트 공통 동작). 파라미터: 문자열 번호, 식
static const INT32 kSetAlterableStringAction = 49;
// 식: 오브젝트의 알터러블 문자열 / 알터러블 값
static const INT32 kAlterableStringExpression = 19;
static const INT32 kAlterableValueExpression = 16;
// 확장 오브젝트의 objectType 은 32 이상이다
static const INT32 kFirstExtensionObjectType = 32;

static const char* kQualifierPrefix = "qualifier#";


// 식 하나의 토큰을 보고 문자열 상수, 알터러블 문자열 참조, INI 조회, 계산식 여부를 요약한다
static void describeExpression(const Json::Value& param, FrameEventFlowExpression& expression)
{
	const Json::Value& tokens = param["tokens"];
	for (Json::ArrayIndex i = 0; i < tokens.size(); i++) {
		const Json::Value& token = tokens[i];
		const string kind = token.get("kind", "").asString();
		if (kind == "string") {
			expression.literals.push_back(token.get("value", "").asString());
		}
		else if (kind == "object") {
			INT32 num = token.get("num", 0).asInt();
			string objectName = token.get("objectName", "").asString();
			if (num == kAlterableStringExpression) {
				expression.references.push_back(make_pair(objectName, (INT32)token.get("value", 0).asInt()));
			}
			else if (num == kAlterableValueExpression) {
				expression.calculation = true;
			}
			else if (objectName.size() >= 3 && _strnicmp(objectName.c_str(), "ini", 3) == 0) {
				expression.ini = true;
			}
			else {
				expression.calculation = true;
			}
		}
	}
}


// 한 동작이 문자열을 얼마나 단순하게 만드는지: INI > 참조 > 상수 > 계산식
static const char* classifyExpression(const FrameEventFlowExpression& expression)
{
	if (expression.ini) {
		return "ini";
	}
	if (!expression.references.empty()) {
		return "reference";
	}
	if (!expression.literals.empty() && !expression.calculation) {
		return "literal";
	}
	return "calculation";
}


void FrameEventTextFlow::clear()
{
	assignments.clear();
	setters.clear();
	fontLoads.clear();
	eventConditions.clear();
	blitters.clear();
}


void FrameEventTextFlow::addFrameScript(UINT32 frameIndex, const Json::Value& frameScript)
{
	const string frameName = frameScript.get("name", "").asString();
	const Json::Value& blocks = frameScript["events"]["blocks"];
	for (Json::ArrayIndex b = 0; b < blocks.size(); b++) {
		const Json::Value& groups = blocks[b]["groups"];
		for (Json::ArrayIndex g = 0; g < groups.size(); g++) {
			const Json::Value& conditions = groups[g]["conditions"];
			for (Json::ArrayIndex c = 0; c < conditions.size(); c++) {
				// 확장 오브젝트의 확장 전용 조건(번호 0 이상)은 그 확장이 보내는 이벤트(글자별 이벤트 등)를 받는 곳이다
				if ((conditions[c].get("objectType", 0).asInt() >= kFirstExtensionObjectType) && (conditions[c].get("num", -1).asInt() >= 0)) {
					eventConditions.push_back(make_pair(conditions[c].get("objectName", "").asString(), frameIndex));
				}
			}
			const Json::Value& actions = groups[g]["actions"];
			for (Json::ArrayIndex a = 0; a < actions.size(); a++) {
				const Json::Value& action = actions[a];
				const Json::Value& params = action["params"];
				INT32 objectType = action.get("objectType", 0).asInt();
				INT32 num = action.get("num", 0).asInt();
				const string objectName = action.get("objectName", "").asString();

				if ((num == kSetAlterableStringAction) && (objectType >= 2) && (params.size() >= 2) && (params[0].get("kind", "").asString() == "short")) {
					FrameEventFlowSetter setter;
					setter.frame = frameIndex;
					setter.object = objectName;
					setter.index = params[0].get("value", 0).asInt();
					FrameEventFlowExpression expression;
					describeExpression(params[1], expression);
					setter.literals = expression.literals;
					setters.push_back(setter);
				}
				else if ((num == kTextBlitterSetTextAction) && (objectType >= kFirstExtensionObjectType) && (params.size() >= 1)
					&& (params[0].get("kind", "").asString() == "expression")) {
					FrameEventFlowAssignment assignment;
					assignment.frame = frameIndex;
					assignment.frameName = frameName;
					assignment.group = groups[g].get("uid", 0).asInt();
					assignment.target = objectName;
					describeExpression(params[0], assignment.expression);
					assignments.push_back(assignment);
				}
				else if (((num == kTextBlitterLoadImageAction) || (num == kTextBlitterImageWidthAction) || (num == kTextBlitterImageHeightAction))
					&& (objectType >= kFirstExtensionObjectType) && (params.size() >= 1) && (params[0].get("kind", "").asString() == "expression")) {
					FrameEventFlowFontLoad load;
					load.frame = frameIndex;
					load.frameName = frameName;
					load.group = groups[g].get("uid", 0).asInt();
					load.target = objectName;
					load.num = num;
					describeExpression(params[0], load.expression);
					fontLoads.push_back(load);
				}
			}
		}
	}
}


void FrameEventTextFlow::addObjectScript(const Json::Value& objectScript)
{
	if ((objectScript.get("identifier", "").asString() != "TLBT") || !objectScript.isMember("textBlitter")) {
		return;
	}
	FrameEventFlowBlitter blitter;
	blitter.index = objectScript.get("index", 0).asInt();
	blitter.name = objectScript.get("name", "").asString();
	const Json::Value& strings = objectScript["alterableStrings"];
	if (strings.isArray() && (strings.size() > 0)) {
		blitter.font = strings[0].asString();
	}
	blitter.text = objectScript["textBlitter"].get("text", "").asString();
	const Json::Value& flagNames = objectScript["textBlitter"]["flagNames"];
	for (Json::ArrayIndex i = 0; i < flagNames.size(); i++) {
		blitter.flagNames.push_back(flagNames[i].asString());
	}
	blitters.push_back(blitter);
}


// 같은 문자열이 이미 있거나 빈 문자열이면 넣지 않는다
static void appendUnique(vector<string>& list, const string& value)
{
	if (value.empty()) {
		return;
	}
	for (size_t i = 0; i < list.size(); i++) {
		if (list[i] == value) {
			return;
		}
	}
	list.push_back(value);
}


static Json::Value toJsonArray(const vector<string>& list)
{
	Json::Value array(Json::arrayValue);
	for (size_t i = 0; i < list.size(); i++) {
		array.append(list[i]);
	}
	return array;
}


// 한 동작을 JSON 으로 만든다. 참조는 같은 프레임의 문자열 설정 동작으로 상수까지 따라간다
static Json::Value assignmentToJson(const FrameEventFlowAssignment& assignment, const vector<FrameEventFlowSetter>& setters, vector<string>& reachable)
{
	Json::Value item(Json::objectValue);
	item["frame"] = assignment.frame;
	item["frameName"] = assignment.frameName;
	item["group"] = assignment.group;
	item["kind"] = classifyExpression(assignment.expression);
	item["literals"] = toJsonArray(assignment.expression.literals);
	for (size_t i = 0; i < assignment.expression.literals.size(); i++) {
		appendUnique(reachable, assignment.expression.literals[i]);
	}
	Json::Value references(Json::arrayValue);
	for (size_t r = 0; r < assignment.expression.references.size(); r++) {
		const pair<string, INT32>& reference = assignment.expression.references[r];
		vector<string> literals;
		UINT32 setCount = 0;
		for (size_t s = 0; s < setters.size(); s++) {
			if ((setters[s].frame == assignment.frame) && (setters[s].object == reference.first) && (setters[s].index == reference.second)) {
				setCount++;
				for (size_t l = 0; l < setters[s].literals.size(); l++) {
					appendUnique(literals, setters[s].literals[l]);
				}
			}
		}
		Json::Value entry(Json::objectValue);
		entry["object"] = reference.first;
		entry["string"] = reference.second;
		entry["setCount"] = setCount;
		entry["literals"] = toJsonArray(literals);
		references.append(entry);
		for (size_t l = 0; l < literals.size(); l++) {
			appendUnique(reachable, literals[l]);
		}
	}
	if (references.size() > 0) {
		item["references"] = references;
	}
	if (assignment.expression.ini) {
		item["ini"] = true;
	}
	if (assignment.expression.calculation) {
		item["calculation"] = true;
	}
	return item;
}


static const char* fontLoadActionName(INT32 num)
{
	if (num == kTextBlitterLoadImageAction) {
		return "loadImage";
	}
	return (num == kTextBlitterImageWidthAction) ? "imageWidth" : "imageHeight";
}


bool FrameEventTextFlow::exportJson(Json::Value& value) const
{
	value = Json::Value(Json::objectValue);
	value["type"] = frameEventTextFlowJsonType;
	value["version"] = frameEventTextFlowJsonVersion;

	map<string, UINT32> counts;
	UINT32 total = 0;
	Json::Value charEventArray(Json::arrayValue);
	UINT32 charEventHandled = 0;
	Json::Value blitterArray(Json::arrayValue);
	set<string> handled;
	for (size_t i = 0; i < blitters.size(); i++) {
		const string& name = blitters[i].name;
		if (handled.count(name) > 0) {
			continue;
		}
		handled.insert(name);

		Json::Value indices(Json::arrayValue);
		vector<string> initialTexts;
		vector<string> strings0;
		vector<string> flagUnion;
		for (size_t k = i; k < blitters.size(); k++) {
			if (blitters[k].name == name) {
				indices.append(blitters[k].index);
				appendUnique(initialTexts, blitters[k].text);
				appendUnique(strings0, blitters[k].font);
				for (size_t f = 0; f < blitters[k].flagNames.size(); f++) {
					appendUnique(flagUnion, blitters[k].flagNames[f]);
				}
			}
		}
		vector<string> reachable = initialTexts;
		Json::Value assignmentArray(Json::arrayValue);
		for (size_t a = 0; a < assignments.size(); a++) {
			if (assignments[a].target != name) {
				continue;
			}
			assignmentArray.append(assignmentToJson(assignments[a], setters, reachable));
			counts[classifyExpression(assignments[a].expression)]++;
			total++;
		}
		// 글꼴 이름(알터러블 문자열 0)을 런타임에 바꾸는 동작
		vector<string> fontValues;
		for (size_t s = 0; s < setters.size(); s++) {
			if ((setters[s].object == name) && (setters[s].index == 0)) {
				for (size_t l = 0; l < setters[s].literals.size(); l++) {
					appendUnique(fontValues, setters[s].literals[l]);
				}
			}
		}

		Json::Value blitter(Json::objectValue);
		blitter["name"] = name;
		blitter["indices"] = indices;
		blitter["initialTexts"] = toJsonArray(initialTexts);
		blitter["string0"] = toJsonArray(strings0);
		if (!flagUnion.empty()) {
			blitter["flags"] = toJsonArray(flagUnion);
		}
		for (size_t f = 0; f < flagUnion.size(); f++) {
			if (flagUnion[f] == "CHAR_EVENT") {
				UINT32 handlers = 0;
				for (size_t c = 0; c < eventConditions.size(); c++) {
					if (eventConditions[c].first == name) {
						handlers++;
					}
				}
				Json::Value entry(Json::objectValue);
				entry["name"] = name;
				entry["handlers"] = handlers;
				charEventArray.append(entry);
				if (handlers > 0) {
					charEventHandled++;
				}
			}
		}
		if (!fontValues.empty()) {
			blitter["string0Values"] = toJsonArray(fontValues);
		}
		blitter["assignments"] = assignmentArray;
		UINT32 reloads = 0;
		for (size_t f = 0; f < fontLoads.size(); f++) {
			if ((fontLoads[f].target == name) && (fontLoads[f].num == kTextBlitterLoadImageAction)) {
				reloads++;
			}
		}
		if (reloads > 0) {
			blitter["fontReloads"] = reloads;
		}
		blitter["texts"] = toJsonArray(reachable);
		blitterArray.append(blitter);
	}

	// 한정자(여러 오브젝트를 한꺼번에 가리키는 대상)로 가는 동작은 어느 블리터인지 알 수 없어 따로 나열한다
	Json::Value qualifierArray(Json::arrayValue);
	for (size_t a = 0; a < assignments.size(); a++) {
		if (assignments[a].target.compare(0, strlen(kQualifierPrefix), kQualifierPrefix) == 0) {
			vector<string> unused;
			Json::Value item = assignmentToJson(assignments[a], setters, unused);
			item["target"] = assignments[a].target;
			qualifierArray.append(item);
			counts[classifyExpression(assignments[a].expression)]++;
			total++;
		}
	}

	Json::Value summary(Json::objectValue);
	summary["blitters"] = (UINT32)blitterArray.size();
	summary["assignments"] = total;
	const char* kinds[4] = { "literal", "reference", "calculation", "ini" };
	for (int k = 0; k < 4; k++) {
		summary[kinds[k]] = counts[kinds[k]];
	}
	value["summary"] = summary;

	// 폰트 이미지를 실행 중에 바꾸는 동작: 블리터(또는 한정자)로 가는 것만 센다. 다른 확장의 같은 번호 동작은 제외한다
	set<string> blitterNames;
	for (size_t i = 0; i < blitters.size(); i++) {
		blitterNames.insert(blitters[i].name);
	}
	Json::Value loadArray(Json::arrayValue);
	UINT32 loadCount = 0, widthCount = 0, heightCount = 0;
	for (size_t f = 0; f < fontLoads.size(); f++) {
		const FrameEventFlowFontLoad& load = fontLoads[f];
		bool qualifier = (load.target.compare(0, strlen(kQualifierPrefix), kQualifierPrefix) == 0);
		if (!qualifier && (blitterNames.count(load.target) == 0)) {
			continue;
		}
		if (load.num == kTextBlitterLoadImageAction) {
			loadCount++;
		}
		else if (load.num == kTextBlitterImageWidthAction) {
			widthCount++;
		}
		else {
			heightCount++;
		}
		Json::Value item(Json::objectValue);
		item["frame"] = load.frame;
		item["frameName"] = load.frameName;
		item["group"] = load.group;
		item["target"] = load.target;
		item["action"] = fontLoadActionName(load.num);
		item["kind"] = classifyExpression(load.expression);
		item["literals"] = toJsonArray(load.expression.literals);
		if (!load.expression.references.empty()) {
			Json::Value references(Json::arrayValue);
			for (size_t r = 0; r < load.expression.references.size(); r++) {
				Json::Value entry(Json::objectValue);
				entry["object"] = load.expression.references[r].first;
				entry["string"] = load.expression.references[r].second;
				references.append(entry);
			}
			item["references"] = references;
		}
		loadArray.append(item);
	}
	Json::Value fontReload(Json::objectValue);
	fontReload["detected"] = (loadCount > 0);
	fontReload["loadImage"] = loadCount;
	fontReload["imageWidth"] = widthCount;
	fontReload["imageHeight"] = heightCount;
	fontReload["loads"] = loadArray;
	value["fontReload"] = fontReload;
	value["blitters"] = blitterArray;
	// 글자별 이벤트(CHAR_EVENT)를 쓰는 블리터. handlers 는 그 블리터에 걸린 확장 조건 수다 (한정자로 건 조건은 세지 않는다).
	// 0 이면 켜져 있어도 받는 이벤트가 없다는 뜻이다. 한글화 모듈은 글자표가 있는 블리터의 글자별 이벤트를 끈다
	Json::Value charEvents(Json::objectValue);
	charEvents["blitters"] = (UINT32)charEventArray.size();
	charEvents["withHandlers"] = charEventHandled;
	charEvents["objects"] = charEventArray;
	value["charEvents"] = charEvents;
	if (qualifierArray.size() > 0) {
		value["qualifierAssignments"] = qualifierArray;
	}
	return true;
}
