#pragma once

#include "ChunkConstant.h"
#include "ChunkStructure.h"

namespace CTT {

	namespace Chunk {

		class Chunk 
		{
		public:
			Chunk();
			virtual ~Chunk() = 0;

			ChunkType getType() { return info.type; }

			bool isCompressed() { return (info.flag & 0x1) != 0; }
			bool isEncrypted() { return (info.flag & 0x2) != 0; }

			UINT32 getSize() { return info.size; }

			bool readFromArchive(HANDLE hArchive);
			bool readFromBufferAndChunkInfo(BYTE* &rawBuffer, ChunkInfo &info);

			bool writeIntoArchive(HANDLE hArchive);
			bool writeIntoBufferAndChunkInfo(BYTE*& rawBuffer, ChunkInfo& info);

			bool importRawData(BYTE* &rawBuffer);
			bool exportRawData(BYTE* &rawBuffer);

			bool hasRead() { return chunkRead; }

			// 데이터 렌더링용 함수
			virtual void renderData() = 0;

		protected:
			ChunkInfo info;
			BYTE* orgBuffer;
			UINT32 orgSize;
			bool chunkRead;		// chunk 데이터를 한 번 이상 읽어들였냐에 대한 여부

			virtual bool loadFromOrgBuffer() = 0;
			virtual bool saveIntoOrgBuffer() = 0;
		};

		// 분류가 불가능한 Chunk
		class UnknownChunk : public Chunk 
		{
		public:
			UnknownChunk() { 
				value = NULL; 
				valueSize = 0;
			}
			~UnknownChunk() { 
				if (value != NULL) {
					free(value);
					value = NULL;
				}
			}
			BYTE*& getValue() { return value; }
			UINT32& getValueSize() { return valueSize; }

			// 데이터 렌더링용 함수
			void renderData();

		protected:
			bool loadFromOrgBuffer()
			{
				if (value != NULL) {
					free(value);
					value = NULL;
				}
				value = (BYTE*)malloc(orgSize);
				memcpy(value, orgBuffer, orgSize);
				valueSize = orgSize;
				free(orgBuffer);
				orgBuffer = NULL;
				return true;
			}
			bool saveIntoOrgBuffer()
			{
				if (orgBuffer != NULL) {
					free(orgBuffer);
					orgBuffer = NULL;
				}
				orgSize = valueSize;
				orgBuffer = (BYTE*)malloc(orgSize);
				memcpy(orgBuffer, value, orgSize);
				return true;
			}

		private:
			BYTE* value;
			UINT32 valueSize;
		};

		// 마지막 Chunk
		class EndChunk : public Chunk {
		public:
			EndChunk() {
				value = NULL;
				valueSize = 0;
				info.type = ChunkType::CHUNK_END;
			}
			~EndChunk() {
			}

			BYTE* &getValue() { return value; }
			UINT32& getValueSize() { return valueSize; }

			// 데이터 렌더링용 함수
			void renderData();

		protected:
			bool loadFromOrgBuffer() { return true; }
			bool saveIntoOrgBuffer() { return true; }

		private:
			BYTE* value;
			UINT32 valueSize;
		};

		// ** chunkType에 따라 chunk를 할당해서 반환해 주는 함수 **
		Chunk* allocateChunk(ChunkType chunkType, bool &isAppInfo);

		// ** 해당 chunkType이 위치한 오프셋을 세팅해 주는 함수, 성공하면 true **
		bool moveArchiveHandleToChunk(HANDLE hArchive, ChunkType chunkType);

	}

}