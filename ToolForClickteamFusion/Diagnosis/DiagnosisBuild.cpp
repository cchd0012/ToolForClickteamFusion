#include "Diagnosis.h"

#include <cstdlib>
#include <set>

using namespace CTT::Diagnosis;

namespace {

	const INT64 kGlyphTableModuleSize = 37376;				// 글자표 방식을 쓸 수 있는 txtblt.mfx 원본의 크기
	const char* kFoldModuleHash = "8424C67E";				// 접기 방식 대상 txtblt(119KB판)의 모듈 번호. 추출한 파일 이름에 들어 있다

	void addItem(Json::Value& items, const char* id, const char* status, const string& title, const string& detail, int step)
	{
		Json::Value item(Json::objectValue);
		item["id"] = id;
		item["status"] = status;
		item["title"] = title;
		item["detail"] = detail;
		item["step"] = step;
		items.append(item);
	}

	void addGuide(Json::Value& guide, const string& text, int step)
	{
		Json::Value entry(Json::objectValue);
		entry["text"] = text;
		entry["step"] = step;
		guide.append(entry);
	}

	string join(const vector<string>& list, const char* separator)
	{
		string result;
		for (size_t i = 0; i < list.size(); i++) {
			if (i > 0) {
				result += separator;
			}
			result += list[i];
		}
		return result;
	}

	// ybn001.ccn ~ ybn007.ccn 처럼 번호만 다른 이름이 3개 이상이면 한 덩어리로 줄여 보인다
	vector<string> summarizeNumberedNames(const vector<string>& names)
	{
		struct Parsed { string key; string text; long long number; bool numbered; };
		vector<Parsed> parsed;
		for (size_t i = 0; i < names.size(); i++) {
			Parsed p;
			p.text = names[i];
			p.number = 0;
			p.numbered = false;
			size_t dot = names[i].find_last_of('.');
			string stem = (dot == string::npos) ? names[i] : names[i].substr(0, dot);
			string ext = (dot == string::npos) ? string() : names[i].substr(dot);
			size_t digits = 0;
			while (digits < stem.size() && stem[stem.size() - 1 - digits] >= '0' && stem[stem.size() - 1 - digits] <= '9') {
				digits++;
			}
			if (digits > 0 && digits <= 9) {
				p.numbered = true;
				p.number = atoll(stem.substr(stem.size() - digits).c_str());
				p.key = stem.substr(0, stem.size() - digits) + "#" + to_string(digits) + ext;
			}
			parsed.push_back(p);
		}
		vector<string> result;
		set<string> done;
		for (size_t i = 0; i < parsed.size(); i++) {
			if (!parsed[i].numbered) {
				result.push_back(parsed[i].text);
				continue;
			}
			if (done.count(parsed[i].key) > 0) {
				continue;
			}
			vector<size_t> group;
			for (size_t k = 0; k < parsed.size(); k++) {
				if (parsed[k].numbered && parsed[k].key == parsed[i].key) {
					group.push_back(k);
				}
			}
			done.insert(parsed[i].key);
			if (group.size() < 3) {
				for (size_t k = 0; k < group.size(); k++) {
					result.push_back(parsed[group[k]].text);
				}
				continue;
			}
			size_t first = group[0], last = group[0];
			for (size_t k = 0; k < group.size(); k++) {
				if (parsed[group[k]].number < parsed[first].number) { first = group[k]; }
				if (parsed[group[k]].number > parsed[last].number) { last = group[k]; }
			}
			bool consecutive = (parsed[last].number - parsed[first].number + 1 == (long long)group.size());
			result.push_back(parsed[first].text + " ~ " + parsed[last].text + " (" + to_string(group.size()) + u8"개" + (consecutive ? "" : u8", 번호가 이어지지 않음") + ")");
		}
		return result;
	}

	// 텍스트 흐름에서 블리터에 나오는 문구가 이벤트 문자열 목록에 몇 개 있는지 센다
	int countShownEventTexts(const Json::Value& flow, const vector<string>& eventTexts)
	{
		set<string> events(eventTexts.begin(), eventTexts.end());
		set<string> shown;
		const Json::Value& blitters = flow["blitters"];
		for (Json::ArrayIndex i = 0; i < blitters.size(); i++) {
			const Json::Value& texts = blitters[i]["texts"];
			for (Json::ArrayIndex k = 0; k < texts.size(); k++) {
				const string text = texts[k].asString();
				if (events.count(text) > 0) {
					shown.insert(text);
				}
			}
		}
		return (int)shown.size();
	}

	int countReloadFrames(const Json::Value& reload)
	{
		set<UINT32> frames;
		const Json::Value& loads = reload["loads"];
		for (Json::ArrayIndex i = 0; i < loads.size(); i++) {
			if (loads[i].get("action", "").asString() == "loadImage") {
				frames.insert(loads[i].get("frame", 0).asUInt());
			}
		}
		return (int)frames.size();
	}

}


void CTT::Diagnosis::buildDiagnosis(const DiagnosisInput& in, Json::Value& out)
{
	Json::Value items(Json::arrayValue);
	Json::Value guide(Json::arrayValue);
	const bool flowValid = in.flow.isObject() && in.flow.isMember("fontReload");
	bool glyphTable = false;
	bool fold = false;
	bool reload = false;

	// 아카이브
	addItem(items, "archive", "ok", u8"아카이브", in.kind + u8" · " + in.engine + u8" · 빌드 " + to_string(in.build), DIAG_STEP_NONE);

	// 추출
	vector<string> missing;
	if (!in.folderExists) {
		addItem(items, "extract", "warn", u8"추출", u8"아직 추출하지 않았습니다. 추출하면 글자 오브젝트와 문구까지 진단할 수 있습니다.", DIAG_STEP_EXTRACT);
	}
	else {
		if (!in.hasScript) {
			missing.push_back(u8"스크립트");
		}
		if (!in.hasText) {
			missing.push_back(u8"텍스트");
		}
		if (!in.hasFlow) {
			missing.push_back(u8"텍스트 흐름");
		}
		if (missing.empty()) {
			addItem(items, "extract", "ok", u8"추출", u8"진단에 필요한 종류(스크립트, 텍스트, 텍스트 흐름)를 모두 추출했습니다.", DIAG_STEP_NONE);
		}
		else {
			addItem(items, "extract", "warn", u8"추출", u8"추출하지 않은 종류: " + join(missing, ", ") + u8". 이 종류가 필요한 항목은 진단하지 못합니다.", DIAG_STEP_EXTRACT);
		}
	}

	// 이 게임이 여러 아카이브 파일로 나뉘어 있는가
	const int relatedCount = (int)in.relatedArchives.size();
	if (relatedCount > 0) {
		string names = join(summarizeNumberedNames(in.relatedArchives), ", ");
		addItem(items, "related", "warn", u8"관련 아카이브", u8"같은 폴더에 아카이브로 보이는 파일이 " + to_string(relatedCount) + u8"개 더 있습니다: " + names + u8". 데이터가 여러 파일로 나뉜 게임이면 파일마다 따로 열어서 추출하고 교체해야 합니다. 이 진단은 지금 연 파일만 본 결과입니다.", DIAG_STEP_NONE);
	}
	else if (in.hasScript && in.subApplicationCount > 0) {
		addItem(items, "related", "unknown", u8"관련 아카이브", u8"서브 애플리케이션 오브젝트가 " + to_string(in.subApplicationCount) + u8"개 있지만 같은 폴더에서 다른 아카이브를 찾지 못했습니다. 이 파일 안에 들어 있거나 다른 위치에 있을 수 있습니다.", DIAG_STEP_NONE);
	}
	else if (!in.hasScript) {
		addItem(items, "related", "info", u8"관련 아카이브", u8"같은 폴더에서 다른 아카이브를 찾지 못했습니다. 스크립트를 추출하지 않아 서브 애플리케이션은 세지 못했습니다.", DIAG_STEP_NONE);
	}
	else {
		addItem(items, "related", "ok", u8"관련 아카이브", u8"같은 폴더에서 다른 아카이브를 찾지 못했고 서브 애플리케이션도 없습니다. 이 파일 하나로 구성된 것으로 보입니다.", DIAG_STEP_NONE);
	}

	// 글자를 그리는 방식
	const string blitterHead = u8"텍스트 블리터 " + to_string(in.blitterCount) + u8"개. ";
	if (!in.hasScript) {
		addItem(items, "glyph", "unknown", u8"글자 방식", u8"스크립트를 추출하지 않아 글자 오브젝트를 알 수 없습니다.", DIAG_STEP_EXTRACT);
	}
	else if (in.objectScriptCount == 0) {
		addItem(items, "glyph", "info", u8"글자 방식", u8"이 파일에는 오브젝트가 없습니다. 이미지나 사운드 같은 리소스만 담은 파일일 수 있습니다. 글자를 그리는 오브젝트는 다른 아카이브에 있을 수 있습니다.", DIAG_STEP_NONE);
	}
	else if (in.blitterCount == 0) {
		addItem(items, "glyph", "ok", u8"글자 방식", u8"텍스트 블리터가 없습니다. 일반 텍스트 오브젝트는 폰트 교체로 처리합니다.", DIAG_STEP_FONT);
	}
	else if (in.txtbltHasKfont) {
		glyphTable = true;
		addItem(items, "glyph", "ok", u8"글자 방식", blitterHead + u8"글자표 방식이 이미 적용된 모듈입니다.", DIAG_STEP_FONT);
	}
	else if (in.txtbltHasKfold) {
		fold = true;
		addItem(items, "glyph", "ok", u8"글자 방식", blitterHead + u8"접기 방식이 이미 적용된 모듈입니다.", DIAG_STEP_FONT);
	}
	else if (!in.txtbltFound) {
		addItem(items, "glyph", "unknown", u8"글자 방식", blitterHead + u8"txtblt 모듈 파일을 찾지 못해 글자를 그리는 방식을 판단하지 못했습니다.", DIAG_STEP_FONT);
	}
	else if (in.txtbltSize == kGlyphTableModuleSize) {
		glyphTable = true;
		addItem(items, "glyph", "ok", u8"글자 방식", blitterHead + u8"모듈이 37376바이트판이라 글자표 방식으로 한글화할 수 있습니다.", DIAG_STEP_FONT);
	}
	else if (in.txtbltName.find(kFoldModuleHash) != string::npos) {
		fold = true;
		addItem(items, "glyph", "warn", u8"글자 방식", blitterHead + u8"모듈이 접기 방식 대상판입니다. 한 문자열에 음절 약 51개, KS X 1001 2350자까지 쓸 수 있습니다.", DIAG_STEP_FONT);
	}
	else {
		addItem(items, "glyph", "unknown", u8"글자 방식", blitterHead + u8"도구가 모르는 txtblt 판입니다 (" + to_string(in.txtbltSize) + u8"바이트). 글자표와 접기 방식 모두 쓸 수 없습니다.", DIAG_STEP_FONT);
	}

	// 글꼴 이미지를 실행 중에 다시 불러오는가
	if (in.hasScript && in.blitterCount > 0) {
		if (!flowValid) {
			addItem(items, "reload", "unknown", u8"글꼴 불러오기", u8"텍스트 흐름을 추출하지 않았거나 오래된 형식이라 알 수 없습니다. 다시 추출하세요.", DIAG_STEP_EXTRACT);
		}
		else if (!glyphTable) {
			addItem(items, "reload", "unknown", u8"글꼴 불러오기", u8"동작 번호가 37376바이트판 기준이라 이 모듈판은 판단하지 못했습니다.", DIAG_STEP_NONE);
		}
		else {
			const Json::Value& fontReload = in.flow["fontReload"];
			if (fontReload.get("detected", false).asBool()) {
				reload = true;
				addItem(items, "reload", "warn", u8"글꼴 불러오기",
					u8"게임이 실행 중에 글꼴 이미지를 다시 불러옵니다 (" + to_string(fontReload.get("loadImage", 0).asInt()) + u8"곳, " + to_string(countReloadFrames(fontReload)) + u8"개 프레임). 아틀라스를 이미지 뱅크에 넣는 방식을 쓰세요. 이 도구가 만든 모듈은 글자표가 있는 오브젝트의 불러오기를 무시합니다.", DIAG_STEP_FONT);
			}
			else {
				addItem(items, "reload", "ok", u8"글꼴 불러오기", u8"실행 중에 글꼴 이미지를 다시 불러오지 않습니다. 이미지 뱅크의 아틀라스가 그대로 쓰입니다.", DIAG_STEP_NONE);
			}
		}
	}

	// 글자별 이벤트
	if (in.hasScript && in.blitterCount > 0 && flowValid && in.flow.isMember("charEvents")) {
		const Json::Value& charEvents = in.flow["charEvents"];
		int count = charEvents.get("blitters", 0).asInt();
		int handled = charEvents.get("withHandlers", 0).asInt();
		if (count == 0) {
			addItem(items, "charevent", "ok", u8"글자별 이벤트", u8"글자별 이벤트를 쓰는 오브젝트가 없습니다.", DIAG_STEP_NONE);
		}
		else if (handled == 0) {
			addItem(items, "charevent", "ok", u8"글자별 이벤트", to_string(count) + u8"개가 켜져 있지만 받는 이벤트가 없습니다. 한글화해도 잃는 것이 없습니다 (한정자로 건 이벤트는 확인하지 못했습니다).", DIAG_STEP_NONE);
		}
		else {
			addItem(items, "charevent", "warn", u8"글자별 이벤트", to_string(count) + u8"개가 켜져 있고 그중 " + to_string(handled) + u8"개는 받는 이벤트가 있습니다. 글자표가 있는 오브젝트는 글자별 이벤트가 꺼져서 글자별 효과가 사라집니다.", DIAG_STEP_FONT);
		}
	}

	// 글자표 배정 진행
	if (glyphTable && in.blitterCount > 0) {
		addItem(items, "assign", "info", u8"글자표 배정", u8"글자표를 배정한 글자 오브젝트: " + to_string(in.blittersAssigned) + u8"개 / " + to_string(in.blitterCount) + u8"개", DIAG_STEP_FONT);
	}

	// 문구
	if (!in.hasText) {
		addItem(items, "strings", "unknown", u8"문구", u8"텍스트를 추출하지 않아 문구를 세지 못했습니다.", DIAG_STEP_EXTRACT);
	}
	else {
		string detail = u8"이벤트 문자열 " + to_string(in.eventTexts.size()) + u8"개.";
		const char* status = "ok";
		if (flowValid && in.flow.isMember("summary")) {
			const Json::Value& summary = in.flow["summary"];
			int unresolved = summary.get("calculation", 0).asInt() + summary.get("ini", 0).asInt();
			detail += u8" 블리터에 나오는 것으로 확인된 문구는 " + to_string(countShownEventTexts(in.flow, in.eventTexts)) + u8"개입니다.";
			if (unresolved > 0) {
				status = "warn";
				detail += u8" 계산식 " + to_string(summary.get("calculation", 0).asInt()) + u8"곳과 INI " + to_string(summary.get("ini", 0).asInt()) + u8"곳으로 만드는 문구는 내용을 알 수 없습니다.";
			}
		}
		else {
			status = "info";
			detail += u8" 텍스트 흐름이 없어 화면에 나오는 문구는 구분하지 못했습니다.";
		}
		detail += u8" 대사 외에 내부 키와 경로도 섞여 있으니 어디에 쓰이는지 확인하며 번역하세요 (이 도구는 구분하지 않습니다).";
		addItem(items, "strings", status, u8"문구", detail, DIAG_STEP_TEXT);
	}

	// 그림 속 글자는 판단하지 못한다
	addItem(items, "imagetext", "unknown", u8"그림 속 글자", u8"확인할 수 없습니다. 로고와 메뉴 그림은 글꼴이 아니라 이미지 수정 영역입니다.", DIAG_STEP_IMAGE);

	// 코드페이지와 로케일
	bool codepageOk = in.codepageKnown && in.localeKnown && in.codepage == 949 && in.locale == 1042;
	if (!in.codepageKnown && !in.localeKnown) {
		addItem(items, "codepage", "unknown", u8"코드페이지", u8"아카이브에서 코드페이지와 로케일을 읽지 못했습니다.", DIAG_STEP_DIAGNOSE);
	}
	else if (codepageOk) {
		addItem(items, "codepage", "ok", u8"코드페이지", u8"코드페이지 949, 로케일 1042입니다.", DIAG_STEP_NONE);
	}
	else {
		addItem(items, "codepage", "warn", u8"코드페이지",
			u8"코드페이지 " + to_string(in.codepage) + u8", 로케일 " + to_string(in.locale) + u8"입니다. 한글이 나오게 하려면 949 / 1042로 바꿔서 재합성해야 합니다 (이 화면의 프로젝트 정보에서 바꿀 수 있습니다).", DIAG_STEP_DIAGNOSE);
	}

	// 진행 방향
	if (!in.folderExists) {
		addGuide(guide, u8"전체 추출을 합니다", DIAG_STEP_EXTRACT);
	}
	else if (!missing.empty()) {
		addGuide(guide, u8"빠진 종류를 추출합니다 (" + join(missing, ", ") + u8")", DIAG_STEP_EXTRACT);
	}
	if (glyphTable) {
		addGuide(guide, u8"폰트 아틀라스를 만들고 번호를 붙입니다", DIAG_STEP_FONT);
		if (in.blitterCount > 0) {
			addGuide(guide, u8"글자 오브젝트 " + to_string(in.blitterCount) + u8"개에 아틀라스를 배정합니다", DIAG_STEP_FONT);
		}
		if (reload) {
			addGuide(guide, u8"아틀라스를 이미지 뱅크에 넣거나, 게임이 불러오는 글꼴 PNG를 아틀라스로 바꿉니다 (글꼴을 실행 중에 다시 불러옵니다)", DIAG_STEP_FONT);
		}
		else {
			addGuide(guide, u8"아틀라스를 이미지 뱅크에 넣고 글자 오브젝트가 그것을 가리키게 합니다", DIAG_STEP_FONT);
		}
		if (!in.txtbltHasKfont) {
			addGuide(guide, u8"글자표를 심은 게임 모듈을 만들어 교체합니다", DIAG_STEP_FONT);
		}
	}
	else if (fold) {
		addGuide(guide, u8"대사 글자 아틀라스를 만들고 게임 모듈을 접기 방식으로 패치합니다", DIAG_STEP_FONT);
	}
	else if (in.hasScript && in.blitterCount > 0) {
		addGuide(guide, u8"글자를 그리는 방식을 직접 확인합니다 (txtblt 판을 알 수 없습니다)", DIAG_STEP_FONT);
	}
	addGuide(guide, u8"이벤트 문자열과 오브젝트 문자열을 번역합니다", DIAG_STEP_TEXT);
	addGuide(guide, codepageOk ? u8"재합성합니다" : u8"코드페이지 949, 로케일 1042로 바꾸고 재합성합니다", DIAG_STEP_REBUILD);
	if (relatedCount > 0) {
		addGuide(guide, u8"같은 폴더의 다른 아카이브(" + join(summarizeNumberedNames(in.relatedArchives), ", ") + u8")도 각각 열어서 추출, 진단, 교체합니다", DIAG_STEP_NONE);
	}
	addGuide(guide, u8"게임 화면에서 확인합니다", DIAG_STEP_CHECK);

	int warnCount = 0;
	int unknownCount = 0;
	for (Json::ArrayIndex i = 0; i < items.size(); i++) {
		const string status = items[i]["status"].asString();
		if (status == "warn") {
			warnCount++;
		}
		else if (status == "unknown") {
			unknownCount++;
		}
	}

	out = Json::Value(Json::objectValue);
	out["type"] = "diagnosis";
	out["version"] = 1;
	Json::Value archive(Json::objectValue);
	archive["file"] = in.fileName;
	archive["kind"] = in.kind;
	archive["engine"] = in.engine;
	archive["build"] = in.build;
	if (in.codepageKnown) {
		archive["codepage"] = in.codepage;
	}
	if (in.localeKnown) {
		archive["locale"] = in.locale;
	}
	out["archive"] = archive;
	Json::Value summary(Json::objectValue);
	summary["warn"] = warnCount;
	summary["unknown"] = unknownCount;
	out["summary"] = summary;
	out["items"] = items;
	out["guide"] = guide;
}


vector<string> CTT::Diagnosis::diagnosisSummaryLines(const Json::Value& diagnosis)
{
	vector<string> lines;
	const Json::Value& items = diagnosis["items"];
	for (Json::ArrayIndex i = 0; i < items.size(); i++) {
		const string status = items[i]["status"].asString();
		const char* label = status == "ok" ? u8"[확인됨]" : status == "warn" ? u8"[확인 필요]" : status == "unknown" ? u8"[알 수 없음]" : u8"[정보]";
		lines.push_back(string(label) + " " + items[i]["title"].asString() + ": " + items[i]["detail"].asString());
	}
	const Json::Value& guide = diagnosis["guide"];
	for (Json::ArrayIndex i = 0; i < guide.size(); i++) {
		lines.push_back(to_string(i + 1) + ". " + guide[i]["text"].asString());
	}
	return lines;
}
