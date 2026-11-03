// 텍스트 흐름 분석(FrameEventTextFlow)의 자체 테스트
//
// 디컴파일 JSON 과 같은 모양의 값을 직접 만들어서 분류(상수 / 참조 / 계산식 / INI), 참조 따라가기,
// 같은 이름의 블리터 합치기, 한정자 대상 분리가 의도대로 되는지 확인한다 (build_text_flow_test.bat).

#include <cstdio>
#include <string>
#include "../ToolForClickteamFusion/Chunk/Frame/FrameEvent/FrameEventTextFlow.h"

using namespace CTT::Chunk;
using namespace std;

static int failureCount = 0;

static void check(bool ok, const char* what)
{
	if (!ok) {
		failureCount++;
		printf("  FAIL: %s\n", what);
	}
}

static Json::Value stringToken(const char* value)
{
	Json::Value t(Json::objectValue);
	t["kind"] = "string";
	t["num"] = 3;
	t["objectType"] = -1;
	t["value"] = value;
	return t;
}

static Json::Value objectToken(const char* name, int num, int index)
{
	Json::Value t(Json::objectValue);
	t["kind"] = "object";
	t["num"] = num;
	t["objectType"] = 2;
	t["objectName"] = name;
	t["value"] = index;
	return t;
}

static Json::Value expressionParam(const Json::Value& token0, const Json::Value& token1 = Json::Value())
{
	Json::Value p(Json::objectValue);
	p["kind"] = "expression";
	Json::Value tokens(Json::arrayValue);
	tokens.append(token0);
	if (!token1.isNull()) {
		tokens.append(token1);
	}
	p["tokens"] = tokens;
	return p;
}

static Json::Value shortParam(int value)
{
	Json::Value p(Json::objectValue);
	p["kind"] = "short";
	p["value"] = value;
	return p;
}

static Json::Value action(int objectType, int num, const char* objectName, const Json::Value& p0, const Json::Value& p1 = Json::Value())
{
	Json::Value a(Json::objectValue);
	a["objectType"] = objectType;
	a["num"] = num;
	a["objectName"] = objectName;
	Json::Value params(Json::arrayValue);
	params.append(p0);
	if (!p1.isNull()) {
		params.append(p1);
	}
	a["params"] = params;
	return a;
}

static Json::Value group(int uid, const Json::Value& a)
{
	Json::Value g(Json::objectValue);
	g["uid"] = uid;
	Json::Value actions(Json::arrayValue);
	actions.append(a);
	g["actions"] = actions;
	return g;
}

static Json::Value objectScript(int index, const char* name, const char* identifier, const char* text)
{
	Json::Value o(Json::objectValue);
	o["index"] = index;
	o["name"] = name;
	o["identifier"] = identifier;
	Json::Value strings(Json::arrayValue);
	strings.append("small1");
	o["alterableStrings"] = strings;
	Json::Value blitter(Json::objectValue);
	blitter["text"] = text;
	o["textBlitter"] = blitter;
	return o;
}

int main()
{
	FrameEventTextFlow flow;

	Json::Value groups(Json::arrayValue);
	// Dlg.문자열[1] 에 상수 두 개를 설정한다
	groups.append(group(1, action(2, 49, "Dlg", shortParam(1), expressionParam(stringToken("Hello")))));
	groups.append(group(2, action(2, 49, "Dlg", shortParam(1), expressionParam(stringToken("World")))));
	// Box 에 Dlg.문자열[1] 을 보낸다 (참조)
	groups.append(group(3, action(43, 80, "Box", expressionParam(objectToken("Dlg", 19, 1)))));
	// Box 에 상수
	groups.append(group(4, action(43, 80, "Box", expressionParam(stringToken("Static")))));
	// 한정자 대상
	groups.append(group(5, action(43, 80, "qualifier#33", expressionParam(stringToken("Q")))));
	// 상수 + 알터러블 값 (계산식)
	groups.append(group(6, action(43, 80, "Box", expressionParam(stringToken("A: "), objectToken("Val", 16, 3)))));
	// INI 조회
	groups.append(group(7, action(43, 80, "Box", expressionParam(objectToken("Ini 2", 85, 0)))));
	// 확장 오브젝트가 아닌 대상의 동작 번호 80 은 무시한다
	groups.append(group(8, action(2, 80, "Box", expressionParam(stringToken("Ignored")))));
	// 폰트 이미지 바꾸기: 블리터에 파일 불러오기(93) / 이미지 폭(95), 한정자에 불러오기(참조), 블리터가 아닌 대상과 확장이 아닌 대상은 센다/세지 않는다
	groups.append(group(9, action(43, 93, "Box", expressionParam(stringToken("/Data/Fonts/small1.png")))));
	groups.append(group(10, action(43, 95, "Box", expressionParam(objectToken("Ini 2", 85, 0)))));
	groups.append(group(11, action(43, 93, "qualifier#33", expressionParam(stringToken("/Data/Fonts/"), objectToken("Dlg", 19, 0)))));
	groups.append(group(12, action(43, 93, "Other", expressionParam(stringToken("other.png")))));
	groups.append(group(13, action(2, 93, "Box", expressionParam(stringToken("not-extension.png")))));

	// 조건: Box 의 확장 조건(번호 0 이상)은 세고, 시스템 조건(음수)과 확장이 아닌 오브젝트의 조건은 세지 않는다
	Json::Value conditions(Json::arrayValue);
	for (int i = 0; i < 3; i++) {
		Json::Value c(Json::objectValue);
		c["objectType"] = (i == 2) ? 2 : 43;
		c["num"] = (i == 1) ? -42 : 3;
		c["objectName"] = "Box";
		conditions.append(c);
	}
	groups[0]["conditions"] = conditions;
	Json::Value block(Json::objectValue);
	block["groups"] = groups;
	Json::Value blocks(Json::arrayValue);
	blocks.append(block);
	Json::Value events(Json::objectValue);
	events["blocks"] = blocks;
	Json::Value frame(Json::objectValue);
	frame["name"] = "F";
	frame["events"] = events;
	flow.addFrameScript(3, frame);

	Json::Value boxScript = objectScript(4, "Box", "TLBT", "Init");
	boxScript["textBlitter"]["flagNames"].append("ESCAPE");
	boxScript["textBlitter"]["flagNames"].append("CHAR_EVENT");
	boxScript["textBlitter"]["flagNames"].append("0x4000");
	flow.addObjectScript(boxScript);
	// 처음 만든 오브젝트에 옵션 플래그 이름을 붙인다 (오브젝트 스크립트 JSON 의 textBlitter.flagNames 와 같은 모양)
	flow.addObjectScript(objectScript(9, "Box", "TLBT", "Init2"));
	flow.addObjectScript(objectScript(5, "Other", "SPRI", "NotABlitter"));

	Json::Value result;
	check(flow.exportJson(result), "exportJson");
	check(result["type"].asString() == "textFlow", "type");
	check(flow.getBlitterCount() == 2, "blitter object count");

	const Json::Value& summary = result["summary"];
	check(summary["blitters"].asUInt() == 1, "blitters are merged by name");
	check(summary["assignments"].asUInt() == 5, "4 blitter assignments + 1 qualifier assignment");
	check(summary["literal"].asUInt() == 2, "literal count");
	check(summary["reference"].asUInt() == 1, "reference count");
	check(summary["calculation"].asUInt() == 1, "calculation count");
	check(summary["ini"].asUInt() == 1, "ini count");

	const Json::Value& box = result["blitters"][0];
	check(box["name"].asString() == "Box", "name");
	check(box["indices"].size() == 2 && box["indices"][1].asInt() == 9, "indices");
	check(box["assignments"].size() == 4, "assignments of Box");
	check(box["assignments"][0]["kind"].asString() == "reference", "first is a reference");
	const Json::Value& reference = box["assignments"][0]["references"][0];
	check(reference["setCount"].asUInt() == 2, "reference follows both setters");
	check(reference["literals"].size() == 2 && reference["literals"][0].asString() == "Hello", "reference literals");

	// 나올 수 있는 모든 문자열: 처음 문자열 + 상수 + 참조에서 따라간 상수
	set<string> texts;
	for (Json::ArrayIndex i = 0; i < box["texts"].size(); i++) {
		texts.insert(box["texts"][i].asString());
	}
	check(texts.count("Init") && texts.count("Init2") && texts.count("Hello") && texts.count("World") && texts.count("Static") && texts.count("A: "), "reachable texts");
	check(texts.count("Ignored") == 0, "non-extension action is ignored");
	check(result["qualifierAssignments"].size() == 1 && result["qualifierAssignments"][0]["target"].asString() == "qualifier#33", "qualifier assignment");

	const Json::Value& reload = result["fontReload"];
	check(reload["detected"].asBool(), "font reload detected");
	check(reload["loadImage"].asUInt() == 2, "2 load actions (Box + qualifier), non-blitter and non-extension are ignored");
	check(reload["imageWidth"].asUInt() == 1 && reload["imageHeight"].asUInt() == 0, "image width/height counts");
	check(reload["loads"].size() == 3, "3 reload entries");
	check(reload["loads"][0]["action"].asString() == "loadImage" && reload["loads"][0]["literals"][0].asString() == "/Data/Fonts/small1.png", "load entry literal");
	check(reload["loads"][1]["action"].asString() == "imageWidth" && reload["loads"][1]["kind"].asString() == "ini", "width entry is ini");
	check(reload["loads"][2]["target"].asString() == "qualifier#33" && reload["loads"][2]["kind"].asString() == "reference", "qualifier load is a reference");
	check(box["fontReloads"].asUInt() == 1, "Box has 1 reload");
	check(summary["assignments"].asUInt() == 5, "font actions are not text assignments");

	check(box["flags"].size() == 3 && box["flags"][1].asString() == "CHAR_EVENT" && box["flags"][2].asString() == "0x4000", "flag names are listed per blitter");
	const Json::Value& charEvents = result["charEvents"];
	check(charEvents["blitters"].asUInt() == 1 && charEvents["objects"][0]["name"].asString() == "Box", "one blitter uses CHAR_EVENT");
	check(charEvents["objects"][0]["handlers"].asUInt() == 1 && charEvents["withHandlers"].asUInt() == 1, "only the extension condition counts as a handler");

	if (failureCount == 0) {
		printf("TextFlowTest: all checks passed\n");
		return 0;
	}
	printf("TextFlowTest: %d failures\n", failureCount);
	return 1;
}
