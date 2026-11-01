#include "FrameEventChunk.h"
#include "../../../Archive/Archive.h"
#include "../../../ImGUI/Headers.h"

using namespace CTT::Chunk;
using namespace CTT::Archive;


FrameEventChunk::FrameEventChunk()
{
	parsed = false;
	groupCount = 0;
	pointerCount = 0;
}


FrameEventChunk::~FrameEventChunk()
{
}


bool FrameEventChunk::loadFromOrgBuffer()
{
	ByteBuffer raw(orgBuffer, orgBuffer + orgSize);
	parsed = false;
	parseMessage.clear();
	rawFallback.clear();

	string errorMessage;
	if (!parseFrameEventChunk(raw, getArchive()->getBuildVersion(), model, errorMessage)) {
		parseMessage = "parse failed: " + errorMessage;
	}
	else {
		// 모델이 원본 바이트를 정확히 재현할 때에만 모델을 신뢰한다
		ByteBuffer rewritten;
		if (!writeFrameEventChunk(model, rewritten, errorMessage)) {
			parseMessage = "write failed: " + errorMessage;
		}
		else if (rewritten != raw) {
			parseMessage = "model does not reproduce the original bytes";
		}
		else {
			parsed = true;
		}
	}

	if (parsed) {
		groupCount = model.getGroupCount();
		pointerCount = 0;
		for (size_t b = 0; b < model.blocks.size(); b++) {
			const vector<FrameEventDataGroup>& groups = model.blocks[b].groups;
			for (size_t g = 0; g < groups.size(); g++) {
				for (size_t c = 0; c < groups[g].conditions.size(); c++) {
					const vector<FrameEventParameter>& params = groups[g].conditions[c].params;
					for (size_t p = 0; p < params.size(); p++) {
						if (params[p].code == frameEventGroupPointerParameterCode) {
							pointerCount++;
						}
					}
				}
				for (size_t a = 0; a < groups[g].actions.size(); a++) {
					const vector<FrameEventParameter>& params = groups[g].actions[a].params;
					for (size_t p = 0; p < params.size(); p++) {
						if (params[p].code == frameEventGroupPointerParameterCode) {
							pointerCount++;
						}
					}
				}
			}
		}
		wcout << infoPrefix << L"프레임 이벤트를 무손실로 파싱했습니다: " << dec << groupCount << L"개 그룹, " << pointerCount << L"개 그룹 포인터" << endl;
	}
	else {
		model = FrameEventChunkModel();
		rawFallback.swap(raw);
		wcout << errPrefix << L"프레임 이벤트를 파싱하지 못해 원본 바이트로 보존합니다: " << parseMessage.c_str() << endl;
	}

	free(orgBuffer);
	orgBuffer = NULL;
	return true;
}


bool FrameEventChunk::saveIntoOrgBuffer()
{
	ByteBuffer out;
	if (parsed) {
		string errorMessage;
		if (!writeFrameEventChunk(model, out, errorMessage)) {
			parseMessage = "write failed: " + errorMessage;
			return false;
		}
	}
	else {
		out = rawFallback;
	}

	if (orgBuffer != NULL) {
		free(orgBuffer);
		orgBuffer = NULL;
	}
	orgSize = (UINT32)out.size();
	orgBuffer = (BYTE*)malloc(orgSize > 0 ? orgSize : 1);
	if (orgSize > 0) {
		memcpy(orgBuffer, &out[0], orgSize);
	}
	return true;
}


bool FrameEventChunk::extractText(UINT32 frameIndex, FrameEventTextTable& table)
{
	if (!parsed) {
		// 한 프레임을 해석하지 못하더라도 전체 추출은 계속한다
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 이벤트를 파싱하지 못해 텍스트를 추출하지 않습니다. (프레임 " << dec << frameIndex << L")" << endl;
		return true;
	}
	table.addFrameEventText(model, frameIndex);
	return true;
}


bool FrameEventChunk::replaceText(const FrameEventTextTable& table, UINT32& replacedCount)
{
	replacedCount = 0;
	if (!parsed) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 이벤트를 파싱하지 못해 텍스트를 교체하지 않습니다." << endl;
		return true;
	}
	string errorMessage;
	if (!replaceFrameEventText(model, table, replacedCount, errorMessage)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 텍스트 교체에 실패했습니다: " << wstring(errorMessage.begin(), errorMessage.end()) << endl;
		return false;
	}
	return true;
}


bool FrameEventChunk::decompileScript(const FrameEventScriptContext& context, Json::Value& value)
{
	if (!parsed) {
		return false;
	}
	return decompileFrameEvents(model, context, value);
}


bool FrameEventChunk::recompileScript(const Json::Value& value, string& errorMessage)
{
	if (!parsed) {
		errorMessage = "the frame events could not be parsed, so they are kept as raw bytes";
		return false;
	}
	return recompileFrameEvents(model, value, errorMessage);
}


void FrameEventChunk::detectParserSkeleton(vector<ParserSkeletonTarget>& targets)
{
	if (!parsed) {
		return;
	}
	detectParserSkeletonTargets(model, targets);
}


bool FrameEventChunk::patchParserSkeleton(const ParserSkeletonConfig& config, ParserSkeletonStats& stats, string& errorMessage)
{
	if (!parsed) {
		return true;
	}
	return applyParserSkeletonPatch(model, config, stats, errorMessage);
}


void FrameEventChunk::renderData()
{
	if (parsed) {
		ImGui::Text(u8"이벤트 스크립트: 그룹 %u개, 그룹 포인터 %u개", (unsigned)groupCount, (unsigned)pointerCount);
		ImGui::TextDisabled(u8"무손실로 파싱됨 (build %u).", (unsigned)model.build);
	}
	else {
		ImGui::Text(u8"이벤트 스크립트를 원본 바이트로 보존 중 (%u 바이트).", (unsigned)rawFallback.size());
		ImGui::TextDisabled(u8"%s", parseMessage.c_str());
	}
}
