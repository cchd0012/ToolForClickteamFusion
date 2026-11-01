#include "BaseImage.h"
#include "../../Application/Application.h"
#include "../../Archive/Archive.h"
#include "../../Util/BitmapTranslationUtil.h"
#include "../../Util/PngUtil.h"
#include "../../Util/CommonUtil.h"
#include "../../Util/CommonStructure.h"

using namespace CTT::App;
using namespace CTT::Chunk;
using namespace CTT::Archive;
using namespace CTT::BitmapTranslationUtil;
using namespace CTT::PngUtil;
using namespace CTT::CommonUtil;


BaseImage::BaseImage()
{
	compressedData = NULL;
	compressedSize = 0;
	translatingData = NULL;
	translatingSize = 0;
	bitmapData = NULL;
	bitmapSize = 0;
}


BaseImage::~BaseImage()
{
	if (compressedData != NULL) {
		free(compressedData);
		compressedData = NULL;
	}
	if (translatingData != NULL) {
		free(translatingData);
		translatingData = NULL;
	}
	if (bitmapData != NULL) {
		free(bitmapData);
		bitmapData = NULL;
	}
}


bool BaseImage::exportData(wstring& dirPath, UINT32 index, bool isFullyExport)
{
	// 압축 데이터에서 변환 데이터 뽑아내기
	if (!deriveTranslatingData()) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 변환된 데이터를 얻지 못했습니다. (buffer)" << endl;
		return false;
	}

	// 변환 데이터에서 비트맵 데이터 뽑아내기
	if (!getBitmapDataFromTranslatingData()) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 비트맵 데이터를 얻지 못했습니다. (buffer)" << endl;
		return false;
	}

	UINT32 width = getWidth();
	UINT32 height = getHeight();
	UINT32 bitCount = 32;

	// 추출 파일 경로 생성, 투명모드 미추출도 경로에 반영
	wostringstream woss;
	woss << setw(6) << setfill(L'0') << index;

	wstring exportFilePath = dirPath + L"/" + woss.str() + L".png";
	if (getApp()->getStates()->usingImageAndExtractNoneTransparent) {
		exportFilePath = dirPath + L"/" + woss.str() + L"_none_transparent.png";
	}

	wcout << infoPrefix << exportFilePath << L" exporting..." << endl;
	if (getGraphicMode() == 4) {
		wcout << infoPrefix << L" - transparent_color: " << getTransparent().toString() << endl;
	}

	// 투명모드 미추출 체크 반영
	if (!writeToPng(bitmapData, exportFilePath, width, height, bitCount, getApp()->getStates()->usingImageAndExtractNoneTransparent)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(exportFilePath) << L": 이미지 추출을 실패했습니다." << endl;
		return false;
	}

	free(bitmapData);
	bitmapData = NULL;

	return true;
}


bool BaseImage::importData(wstring& dirPath, UINT32 index)
{
	// info에서 flag, graphicMode, transparent 값이 세팅되어 있을 것을 전제로 하고 진행한다
	// 그게 여의치 않으면 직접 flag, graphicMode, transparent를 세팅하는 수밖에 없음

	// import 파일 경로 생성
	wostringstream woss;
	woss << setw(6) << setfill(L'0') << index;
	wstring importFilePath = dirPath + L"/" + woss.str() + L".png";

	// 존재 확인, 없으면 그냥 돌아감
	if (isFileOrDir(importFilePath) != FSOType::FSO_FILE) {
		return true;
	}

	wcout << infoPrefix << L"importing " << importFilePath << L"..." << endl;

	// 이미지 읽어들여서 너비/높이 새로 세팅
	UINT32 width = 0, height = 0, bitCount = 0;

	if (bitmapData != NULL) {
		free(bitmapData);
		bitmapData = NULL;
	}
	bitmapSize = 0;

	if (!readFromPng(bitmapData, importFilePath, width, height, bitCount)) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(importFilePath) << L": 이미지를 읽어들이지 못했습니다." << endl;
		return false;
	}

	setWidth(width);
	setHeight(height);
	bitmapSize = width * height * (bitCount / 8);

	// 생각해보니까 NormalImage면 compressedData만 세팅되고 imageInfo가 세팅되지 않은 상태다
	// 플래그를 세팅하던가, 아니면 한 번 돌려서 미리 세팅하던가
	// 여기서는 교체가 목적이라 다른 정보들을 전부 갖추고 있어야 하므로, 느리더라도 한 번 돌려서 세팅하는 게 합리적이다
	if (!getArchive()->isTwoPointFiveImage()) {
		if (!deriveTranslatingData()) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(importFilePath) << L": 필요한 플래그 및 정보를 세팅하지 못했습니다." << endl;
			return false;
		}
	}

	// 비트맵 데이터에서 변환 데이터 뽑아내기
	if (!getTranslatingDataFromBitmapData()) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": translating 데이터를 얻지 못했습니다. (buffer)" << endl;
		return false;
	}

	// 변환 데이터에서 압축 데이터 뽑아내기
	if (!deriveCompressedData()) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 압축된 데이터를 얻지 못했습니다. (buffer)" << endl;
		return false;
	}

	return true;
}


bool BaseImage::getBitmapDataFromTranslatingData()
{
	UINT32 width = getWidth();
	UINT32 height = getHeight();
	UINT32 bitCount = 32;

	// flipRGB는 신경쓰지 않을거임
	// alpha || RGBA 면 imageData에 alpha 데이터가 끼어있음
	// 그렇지 않다면 transparent 색깔로 alpha를 지정한다
	// 위치를 전부 다 수행하거나, alpha이되 RGBA가 아니면 alpha 데이터가 따로 분리되어있다고 판단함

	// 압축풀고 얻은 데이터를 대상으로 추가 변환 작업을 수행한다
	// 이 과정에서 진짜 bitCount를 알 수 있음
	switch (getGraphicMode())
	{
		case 0:
		{
			if (!Normal32BitImageToRGBA(translatingData, translatingSize, bitmapData, bitmapSize,
				width, height, isAlphaSeparated(), getTransparent())) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Normal 32bit -> 32bit png 변환을 실패했습니다" << endl;
				return false;
			}
			break;
		}
		case 4:
		{
			if (!Normal24BitImageToRGBA(translatingData, translatingSize, bitmapData, bitmapSize,
				width, height, isAlphaSeparated(), getTransparent())) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Normal 24bit -> 32bit png 변환을 실패했습니다" << endl;
				return false;
			}
			break;
		}
		case 5:
		{
			Color transparentColor = Color(0x00, 0x00, 0x00, 0x00);
			setTransparent(transparentColor);
			if (!Normal24BitImageToRGBA(translatingData, translatingSize, bitmapData, bitmapSize,
				width, height, isAlphaSeparated(), getTransparent())) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Normal 24bit -> 32bit png 변환을 실패했습니다" << endl;
				return false;
			}
			break;
		}
		case 6:
		{
			if (!Normal15BitImageToRGBA(translatingData, translatingSize, bitmapData, bitmapSize,
				width, height, getTransparent())) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Normal 15bit -> 32bit png 변환을 실패했습니다" << endl;
				return false;
			}
			break;
		}
		case 7:
		{
			if (!Normal16BitImageToRGBA(translatingData, translatingSize, bitmapData, bitmapSize,
				width, height, getTransparent())) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": Normal 16bit -> 32bit png 변환을 실패했습니다" << endl;
				return false;
			}
			break;
		}
		case 8:
		{
			if (!twoFivePlusImageToRGBA(translatingData, translatingSize, bitmapData, bitmapSize,
				width, height, isAlphaSeparated(), isRGBAData(), getTransparent())) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 2.5+ image -> 32bit png 변환을 실패했습니다" << endl;
				return false;
			}
			break;
		}
		case 1:
		case 2:
		case 3:
		default:
		{
			// Android Mode 1, Android Mode 2, Android Mode 3, (Android Mode 4 = case 4, Android) 미지원
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": graphicMode: " << to_wstring(getGraphicMode()) << L": 해당 그래픽 모드는 지원하지 않습니다." << endl;
			return false;
		}
	};

	// 메모리 관리를 위해 변환 데이터 제거
	free(translatingData);
	translatingData = NULL;
	translatingSize = 0;

	return true;
}


bool BaseImage::getTranslatingDataFromBitmapData()
{
	UINT32 width = getWidth();
	UINT32 height = getHeight();

	// 초기화
	if (translatingData != NULL) {
		free(translatingData);
		translatingData = NULL;
	}
	translatingSize = 0;

	// 변환 수행
	switch (getGraphicMode())
	{
		case 0:
		{
			if (!RGBAToNormal32BitImage(bitmapData, bitmapSize, translatingData, translatingSize,
				width, height, isAlphaSeparated(), getTransparent())) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 32bit png -> 32bit 변환을 실패했습니다" << endl;
				return false;
			}
			break;
		}
		case 4:
		{
			if (!RGBAToNormal24BitImage(bitmapData, bitmapSize, translatingData, translatingSize,
				width, height, isAlphaSeparated(), getTransparent())) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 32bit png -> Normal 24bit 변환을 실패했습니다" << endl;
				return false;
			}
			break;
		}
		case 5:
		{
			Color transparentColor = Color(0x00, 0x00, 0x00, 0x00);
			setTransparent(transparentColor);
			if (!RGBAToNormal24BitImage(bitmapData, bitmapSize, translatingData, translatingSize,
				width, height, isAlphaSeparated(), getTransparent())) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 32bit png -> Normal 24bit 변환을 실패했습니다" << endl;
				return false;
			}
			break;
		}
		case 6:
		{
			if (!RGBAToNormal15BitImage(bitmapData, bitmapSize, translatingData, translatingSize,
				width, height, getTransparent())) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 32bit png -> Normal 15bit변환을 실패했습니다" << endl;
				return false;
			}
			break;
		}
		case 7:
		{
			if (!Normal16BitImageToRGBA(bitmapData, bitmapSize, translatingData, translatingSize,
				width, height, getTransparent())) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 32bit png -> Normal 16bit변환을 실패했습니다" << endl;
				return false;
			}
			break;
		}
		case 8:
		{
			if (!RGBAToTwoFivePlusImage(bitmapData, bitmapSize, translatingData, translatingSize,
				width, height, isAlphaSeparated(), isRGBAData(), getTransparent())) {
				wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 32bit png -> 2.5+ image변환을 실패했습니다" << endl;
				return false;
			}
			break;
		}
		case 1:
		case 2:
		case 3:
		default:
		{
			// Android Mode 1, Android Mode 2, Android Mode 3, (Android Mode 4 = case 4, Android) 미지원
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L"graphicMode: " << to_wstring(getGraphicMode()) << L": 해당 그래픽 모드는 지원하지 않습니다." << endl;
			return false;
		}
	};

	// 메모리 관리를 위해 비트맵 데이터 제거
	free(bitmapData);
	bitmapData = NULL;
	bitmapSize = 0;

	return true;
}