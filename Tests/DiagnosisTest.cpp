// 진단 판단 로직(buildDiagnosis)의 자체 테스트
//
// 디스크를 읽지 않고 사실(DiagnosisInput)만 넣어서, 항목의 상태와 진행 방향이 의도대로 나오는지 확인한다 (build_diagnosis_test.bat).
// 특히 "모르는 것은 알 수 없음으로 둔다"는 원칙이 지켜지는지 본다.

#include <cstdio>
#include <string>
#include "../ToolForClickteamFusion/Diagnosis/Diagnosis.h"

using namespace CTT::Diagnosis;
using namespace std;

static int failureCount = 0;

static void check(bool ok, const char* what)
{
	if (!ok) {
		failureCount++;
		printf("  FAIL: %s\n", what);
	}
}

static const Json::Value* findItem(const Json::Value& result, const char* id)
{
	const Json::Value& items = result["items"];
	for (Json::ArrayIndex i = 0; i < items.size(); i++) {
		if (items[i]["id"].asString() == id) {
			return &items[i];
		}
	}
	return NULL;
}

static string statusOf(const Json::Value& result, const char* id)
{
	const Json::Value* item = findItem(result, id);
	return (item == NULL) ? string("(없음)") : (*item)["status"].asString();
}

static bool guideHas(const Json::Value& result, const char* fragment)
{
	const Json::Value& guide = result["guide"];
	for (Json::ArrayIndex i = 0; i < guide.size(); i++) {
		if (guide[i]["text"].asString().find(fragment) != string::npos) {
			return true;
		}
	}
	return false;
}

static Json::Value makeFlow(bool reload, int charEvents, int handlers, int calc, int ini)
{
	Json::Value flow(Json::objectValue);
	Json::Value summary(Json::objectValue);
	summary["calculation"] = calc;
	summary["ini"] = ini;
	flow["summary"] = summary;
	Json::Value blitters(Json::arrayValue);
	Json::Value blitter(Json::objectValue);
	Json::Value texts(Json::arrayValue);
	texts.append("Hello");
	texts.append("Not an event text");
	blitter["texts"] = texts;
	blitters.append(blitter);
	flow["blitters"] = blitters;
	Json::Value fontReload(Json::objectValue);
	fontReload["detected"] = reload;
	fontReload["loadImage"] = reload ? 25 : 0;
	Json::Value loads(Json::arrayValue);
	if (reload) {
		for (int frame = 0; frame < 3; frame++) {
			Json::Value load(Json::objectValue);
			load["action"] = "loadImage";
			load["frame"] = frame;
			loads.append(load);
			loads.append(load);
		}
	}
	fontReload["loads"] = loads;
	flow["fontReload"] = fontReload;
	Json::Value events(Json::objectValue);
	events["blitters"] = charEvents;
	events["withHandlers"] = handlers;
	flow["charEvents"] = events;
	return flow;
}

static DiagnosisInput baseInput()
{
	DiagnosisInput in;
	in.fileName = "game.dat";
	in.kind = "dat";
	in.engine = "ClickteamFusion 2.5+";
	in.build = 295;
	in.codepageKnown = in.localeKnown = true;
	in.codepage = 1252;
	in.locale = 1033;
	in.folderExists = in.hasScript = in.hasText = in.hasFlow = true;
	in.objectScriptCount = 10;
	in.blitterCount = 4;
	in.txtbltFound = true;
	in.txtbltName = "txtblt.mfx";
	in.txtbltSize = 37376;
	in.eventTexts.push_back("Hello");
	in.eventTexts.push_back("inputGamma");
	in.flow = makeFlow(true, 1, 0, 3, 1);
	return in;
}

int main()
{
	Json::Value result;

	// 1) 아직 추출하지 않은 게임: 모르는 것은 알 수 없음으로 둔다
	{
		DiagnosisInput in;
		in.fileName = "game.exe";
		in.kind = "exe";
		in.engine = "ClickteamFusion 2.0";
		buildDiagnosis(in, result);
		check(result["type"].asString() == "diagnosis", "type");
		check(statusOf(result, "extract") == "warn", "not extracted is a warning");
		check(statusOf(result, "glyph") == "unknown", "glyph method is unknown without script");
		check(findItem(result, "reload") == NULL, "no reload item without blitters");
		check(statusOf(result, "strings") == "unknown", "strings are unknown without text");
		check(statusOf(result, "imagetext") == "unknown", "image text is always unknown");
		check(statusOf(result, "codepage") == "unknown", "unknown codepage stays unknown");
		check(guideHas(result, "전체 추출"), "guide starts with extraction");
	}

	// 2) 글꼴을 다시 불러오는 37376바이트판 게임 (NCSAGE 같은 경우)
	{
		DiagnosisInput in = baseInput();
		buildDiagnosis(in, result);
		check(statusOf(result, "extract") == "ok", "extracted");
		check(statusOf(result, "glyph") == "ok", "37376 is the glyph table build");
		check(statusOf(result, "reload") == "warn", "reload detected");
		check(findItem(result, "reload")->get("detail", "").asString().find("3개 프레임") != string::npos, "reload frames are counted once per frame");
		check(statusOf(result, "charevent") == "ok", "char event without handlers is harmless");
		check(statusOf(result, "assign") == "info", "assignment progress is info");
		check(statusOf(result, "strings") == "warn", "unresolved expressions are a warning");
		check(findItem(result, "strings")->get("detail", "").asString().find("확인된 문구는 1개") != string::npos, "shown event texts are counted");
		check(statusOf(result, "codepage") == "warn", "1252 needs 949");
		check(guideHas(result, "이미지 뱅크에 넣거나"), "reload guide");
		check(guideHas(result, "코드페이지 949"), "codepage guide");
		check(result["summary"]["warn"].asInt() == 3 && result["summary"]["unknown"].asInt() == 1, "summary counts");
	}

	// 3) 글꼴을 다시 불러오지 않는 게임 (Sally 같은 경우), 코드페이지가 이미 949
	{
		DiagnosisInput in = baseInput();
		in.flow = makeFlow(false, 0, 0, 0, 0);
		in.codepage = 949;
		in.locale = 1042;
		buildDiagnosis(in, result);
		check(statusOf(result, "reload") == "ok", "no reload");
		check(statusOf(result, "charevent") == "ok", "no char event objects");
		check(statusOf(result, "strings") == "ok", "all strings resolved");
		check(statusOf(result, "codepage") == "ok", "949 / 1042 is fine");
		check(guideHas(result, "이미지 뱅크에 넣고 글자 오브젝트"), "bank guide");
		check(!guideHas(result, "코드페이지 949, 로케일"), "no codepage change in guide");
	}

	// 4) 글자별 이벤트를 받는 이벤트가 있으면 경고
	{
		DiagnosisInput in = baseInput();
		in.flow = makeFlow(false, 2, 1, 0, 0);
		buildDiagnosis(in, result);
		check(statusOf(result, "charevent") == "warn", "handlers make the char event a warning");
	}

	// 5) 접기 방식 대상 모듈(dtge): 경고, 동작 번호가 달라 글꼴 불러오기는 알 수 없음
	{
		DiagnosisInput in = baseInput();
		in.txtbltSize = 125440;
		in.txtbltName = "0004_8424C67E_txtblt.mfx";
		buildDiagnosis(in, result);
		check(statusOf(result, "glyph") == "warn", "fold build is a warning");
		check(statusOf(result, "reload") == "unknown", "reload is unknown on the fold build");
		check(guideHas(result, "접기 방식으로 패치"), "fold guide");
	}

	// 6) 도구가 모르는 txtblt 판과 모듈을 찾지 못한 경우
	{
		DiagnosisInput in = baseInput();
		in.txtbltSize = 50000;
		buildDiagnosis(in, result);
		check(statusOf(result, "glyph") == "unknown", "unknown build is unknown");
		check(guideHas(result, "txtblt 판을 알 수 없습니다"), "unknown build guide");
		in.txtbltSize = 0;
		in.txtbltFound = false;
		buildDiagnosis(in, result);
		check(statusOf(result, "glyph") == "unknown", "missing module is unknown");
	}

	// 7) 이미 글자표를 심은 모듈, 텍스트 흐름이 없는 경우
	{
		DiagnosisInput in = baseInput();
		in.txtbltHasKfont = true;
		in.txtbltSize = 296000;
		in.flow = Json::Value();
		in.hasFlow = false;
		buildDiagnosis(in, result);
		check(statusOf(result, "glyph") == "ok", "patched module is fine");
		check(statusOf(result, "reload") == "unknown", "reload is unknown without flow");
		check(statusOf(result, "strings") == "info", "strings are informational without flow");
		check(!guideHas(result, "글자표를 심은 게임 모듈을 만들어"), "no module step when already patched");
		check(statusOf(result, "extract") == "warn", "missing flow is reported");
	}

	// 8) 데이터가 여러 파일로 나뉜 게임
	{
		DiagnosisInput in = baseInput();
		buildDiagnosis(in, result);
		check(statusOf(result, "related") == "ok", "single archive is fine");

		in.relatedArchives.push_back("part2.ccn");
		in.relatedArchives.push_back("Data/extra.dat");
		buildDiagnosis(in, result);
		check(statusOf(result, "related") == "warn", "other archives in the folder are reported");
		check(findItem(result, "related")->get("detail", "").asString().find("part2.ccn, Data/extra.dat") != string::npos, "related archive names are listed");
		check(guideHas(result, "각각 열어서 추출"), "guide mentions the other archives");

		in.relatedArchives.clear();
		in.subApplicationCount = 2;
		buildDiagnosis(in, result);
		check(statusOf(result, "related") == "unknown", "sub applications without files are unknown");

		in.hasScript = false;
		buildDiagnosis(in, result);
		check(statusOf(result, "related") == "info", "sub applications are not counted without script");
	}

	if (failureCount == 0) {
		printf("DiagnosisTest: all checks passed\n");
		return 0;
	}
	printf("DiagnosisTest: %d failures\n", failureCount);
	return 1;
}
