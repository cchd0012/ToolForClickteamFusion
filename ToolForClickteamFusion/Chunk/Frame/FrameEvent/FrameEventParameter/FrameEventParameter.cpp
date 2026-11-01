#include "Headers.h"

using namespace CTT::Chunk;


CTT::Chunk::FrameEventBaseParameter* CTT::Chunk::getParameter(FrameEventParameterType frameEventParameterType)
{
	switch (frameEventParameterType)
	{
		// --------------------
		case FrameEventParameterType::GROUP:
		{
			return new FrameEventGroupParameter();
		}
		// --------------------
		case FrameEventParameterType::DATA_POINTER:
		{
			return new FrameEventDataPointerParameter();
		}
		// --------------------
		default:
		{
			return NULL;
		}
	}
}