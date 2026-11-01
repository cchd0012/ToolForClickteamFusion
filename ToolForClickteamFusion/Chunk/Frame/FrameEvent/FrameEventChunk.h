#pragma once

#include "../../Chunk.h"
#include "FrameEventModel.h"

namespace CTT {

	namespace Chunk {

		// Frame event chunk (0x333D) backed by the lossless FrameEventChunkModel.
		// If the data cannot be parsed, or the model does not reproduce the original bytes exactly,
		// the chunk silently keeps the raw bytes, so saving can never corrupt the events.
		class FrameEventChunk : public Chunk
		{
		public:
			FrameEventChunk();
			~FrameEventChunk();

			// ImGui rendering
			void renderData();

			bool isParsed() const { return parsed; }
			FrameEventChunkModel& getModel() { return model; }
			const string& getParseMessage() const { return parseMessage; }

		protected:
			bool loadFromOrgBuffer();
			bool saveIntoOrgBuffer();

		private:
			bool parsed;
			FrameEventChunkModel model;
			ByteBuffer rawFallback;		// only filled when the model could not be used
			string parseMessage;

			// statistics for the viewer
			size_t groupCount;
			size_t pointerCount;
		};

	}

}
