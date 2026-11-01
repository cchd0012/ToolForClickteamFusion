#pragma once

#include "../Chunk.h"
#include "../BaseData.h"
#include "../../Util/JsonUtil.h"
#include "FrameStructure.h"
#include "../AppInfo/ShaderStructure.h"

namespace CTT {

	namespace Chunk {

		using namespace CTT::JsonUtil;

		class FrameLayerEffect : public BaseData
		{
		public:
			FrameLayerEffect();
			~FrameLayerEffect();

			UINT32 getDataSize() { 
				// 데이터가 딱 붙어있지 않지만 합치면 크기는 똑같다
				return (sizeof(FrameEffectInfo) + (sizeof(UINT32) * 2)) + getValuesSize();
			}
			UINT32 getValuesSize() {
				UINT32 result = 0;
				for (UINT32 i = 0; i < inst.shaderArguments.size(); i++) {
					switch (inst.shaderArguments[i].info.type) {
						case ShaderParameterType::INT: {
							result += sizeof(UINT32);
							break;
						}
						case ShaderParameterType::FLOAT: {
							result += sizeof(float);
							break;
						}
						case ShaderParameterType::FLOAT4_RGBA: {
							result += sizeof(Color);
							break;
						}
						case ShaderParameterType::IMAGE_HANDLE: {
							result += sizeof(UINT32);
							break;
						}
						default: {
							break;
						}
					}
				}
				return result;
			}

			UINT32 getHandle() { return 0; }
			void setHandle(UINT32 handle) { ; }

			BYTE getFlags() { return 0; }
			void clearFlags() { ; }

			// 인자 정보를 읽어들이거나 기록할 수 있도록 하기 위함
			void setBaseBuffer(BYTE *baseBuffer) {
				this->baseBuffer = baseBuffer;
			}
			void setValuesBufferPnt(BYTE* valuesBufferPnt) {
				this->valuesBufferPnt = valuesBufferPnt;
			}

			bool exportData(wstring& dirPath, UINT32 index, bool isFullyExport) { return true; }
			bool importData(wstring& dirPath, UINT32 index) { return true; }
			bool exportJson(Json::Value& exportValue, vector<UINT32>& offsetList);
			bool importJson(Json::Value& importValue, vector<UINT32>& offsetList);

			bool loadFromBuffer(BYTE*& bufferPnt);
			bool saveIntoBuffer(BYTE*& bufferPnt);

			// 데이터 렌더링용 함수
			void renderData(UINT32 index);

		private:
			FrameEffectInstruction inst;
			BYTE* baseBuffer;
			BYTE* valuesBufferPnt;

		};

	}

}