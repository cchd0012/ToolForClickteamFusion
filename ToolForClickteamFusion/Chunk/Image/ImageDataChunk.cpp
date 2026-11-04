#include "ImageDataChunk.h"
#include "../../Util/CommonUtil.h"
#include "../../Util/JsonUtil.h"

#include <set>

using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::CommonUtil;
using namespace CTT::JsonUtil;


// 애니메이션 프레임이 이미지 핸들을 부호 있는 16비트로 저장하므로, 추가할 수 있는 핸들의 최대값이 정해진다
static const UINT32 maxAppendableHandle = 32767;


bool ImageDataChunk::exportImageList(wstring& exportJsonPath)
{
	// 옛 형식(NormalImage)도 항목마다 핸들을 갖는다 (빌드 0x11C 이상은 저장값에서 1을 뺀 값). 확인 화면이 읽는 보기용 목록이다
	Json::Value root(Json::objectValue);
	root["type"] = "imageList";
	root["version"] = 1;

	UINT32 maxHandle = 0;
	Json::Value images(Json::arrayValue);
	for (size_t i = 0; i < value.size(); i++) {
		BaseImage* image = dynamic_cast<BaseImage*>(value[i]);
		if (image == NULL) {
			continue;
		}
		Json::Value item(Json::objectValue);
		item["index"] = (Json::UInt)i;
		item["handle"] = image->getHandle();
		item["width"] = image->getWidth();
		item["height"] = image->getHeight();
		item["hotspotX"] = (int)(INT16)image->getHotSpotX();
		item["hotspotY"] = (int)(INT16)image->getHotSpotY();
		item["graphicMode"] = image->getGraphicMode();
		item["flags"] = image->getFlags();
		wostringstream fileName;
		fileName << setw(6) << setfill(L'0') << i << L".png";
		item["file"] = getAnsiStr(fileName.str(), CodePage::CP_UTF_8);
		images.append(item);
		if (image->getHandle() > maxHandle) {
			maxHandle = image->getHandle();
		}
	}
	root["imageCount"] = (Json::UInt)value.size();
	root["maxHandle"] = maxHandle;
	root["nextHandle"] = maxHandle + 1;
	root["images"] = images;

	if (!writeIntoJson(root, exportJsonPath)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 이미지 목록 json 을 기록하지 못했습니다." << endl;
		return false;
	}
	return true;
}


bool ImageDataChunk::appendImages(wstring& manifestJsonPath, wstring& baseDirPath)
{
	if (!getArchive()->isTwoPointFiveImage()) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 이미지 추가는 2.5+ 이미지에서만 지원합니다." << endl;
		return false;
	}

	Json::Value root;
	if (!readFromJson(root, manifestJsonPath)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 이미지 추가 json 을 읽어들이지 못했습니다." << endl;
		return false;
	}
	if (!root.isObject() || (root["type"].asString() != "imageAppend") || !root["images"].isArray()) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 이미지 추가 json 형식이 아닙니다." << endl;
		return false;
	}

	// 헤더 값을 복제할 템플릿 이미지
	TwoFivePlusImage* templateImage = NULL;
	std::set<UINT32> usedHandles;
	for (size_t i = 0; i < value.size(); i++) {
		usedHandles.insert(value[i]->getHandle());
	}
	if (root.isMember("templateHandle")) {
		UINT32 templateHandle = root["templateHandle"].asUInt();
		for (size_t i = 0; i < value.size(); i++) {
			if (value[i]->getHandle() == templateHandle) {
				templateImage = dynamic_cast<TwoFivePlusImage*>(value[i]);
				break;
			}
		}
		if (templateImage == NULL) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 템플릿 이미지(핸들 " << dec << templateHandle << L")를 찾지 못했습니다." << endl;
			return false;
		}
	}

	const Json::Value& items = root["images"];
	for (Json::ArrayIndex i = 0; i < items.size(); i++) {
		if (!items[i]["handle"].isUInt() || !items[i]["file"].isString()) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 이미지 추가 json 의 " << dec << i << L" 번째 항목이 올바르지 않습니다." << endl;
			return false;
		}
		UINT32 handle = items[i]["handle"].asUInt();
		if (handle > maxAppendableHandle) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 핸들 " << dec << handle << L" 은 애니메이션 프레임이 가리킬 수 있는 최대값(" << maxAppendableHandle << L")을 넘습니다." << endl;
			return false;
		}
		if (usedHandles.find(handle) != usedHandles.end()) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 핸들 " << dec << handle << L" 은 이미 사용 중입니다." << endl;
			return false;
		}

		wstring imagePath = baseDirPath + L"/" + getUniStr(items[i]["file"].asString(), CodePage::CP_UTF_8);
		TwoFivePlusImage* image = new TwoFivePlusImage();
		image->setHandle(handle);
		if (templateImage != NULL) {
			image->copyHeaderFrom(*templateImage);
		}
		else {
			image->initializeNew();
			image->setHandle(handle);
		}
		if (!image->importFromFile(imagePath)) {
			delete image;
			return false;
		}
		value.push_back(image);
		usedHandles.insert(handle);
	}

	wcout << infoPrefix << L"이미지 " << dec << items.size() << L"개를 추가했습니다." << endl;
	return true;
}
