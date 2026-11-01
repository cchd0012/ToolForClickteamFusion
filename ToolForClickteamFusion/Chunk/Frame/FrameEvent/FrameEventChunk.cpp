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
		// the model is only trusted when it reproduces the original bytes exactly
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
		wcout << infoPrefix << L"Frame events parsed losslessly: " << dec << groupCount << L" groups, " << pointerCount << L" group pointers" << endl;
	}
	else {
		model = FrameEventChunkModel();
		rawFallback.swap(raw);
		wcout << errPrefix << L"Frame events kept as raw bytes: " << parseMessage.c_str() << endl;
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


void FrameEventChunk::renderData()
{
	if (parsed) {
		ImGui::Text(u8"Event script: %u groups, %u group pointers", (unsigned)groupCount, (unsigned)pointerCount);
		ImGui::TextDisabled(u8"Parsed losslessly (build %u).", (unsigned)model.build);
	}
	else {
		ImGui::Text(u8"Event script kept as raw bytes (%u bytes).", (unsigned)rawFallback.size());
		ImGui::TextDisabled(u8"%s", parseMessage.c_str());
	}
}
