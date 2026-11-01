#include "PngUtil.h"
#include "CommonUtil.h"
#include <libpng-1.6.37/png.h>
#include <zlib-1.2.3/zlib.h>

using namespace CTT::PngUtil;
using namespace CTT::CommonUtil;

#pragma warning (disable:6031)
#pragma warning (disable:4996)

void pngErrorHandler(png_structp pngPtr, png_const_charp message);


bool CTT::PngUtil::writeToPng(BYTE*& imageBuffer, wstring pngPath, UINT32& width, UINT32& height, UINT32& bitCount, bool isNoneTransparentMode)
{
	// pngPtr 포인터 및 관련 변수 초기화
	FILE* pngFilePtr = _wfopen(pngPath.c_str(), L"wb");
	png_structp pngPtr = NULL;
	png_infop pngInfoPtr = NULL;
	png_bytepp pngRowArray = NULL;

	// pngPtr 기록용 구조체 생성
	pngPtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, pngErrorHandler, NULL);

	// pngPtr 정보 구조체 생성
	pngInfoPtr = png_create_info_struct(pngPtr);

	// 예외 핸들링
	if (setjmp(png_jmpbuf(pngPtr))) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(pngPath) << L": 오류 발생!" << endl;
		png_destroy_read_struct(&pngPtr, &pngInfoPtr, NULL);
		fclose(pngFilePtr);
		return false;
	}

	// 기록할 곳 세팅
	png_init_io(pngPtr, pngFilePtr);

	// pngPtr color type 세팅, 디폴트 -> 단색, 32비트 -> rgba, 24비트 -> rgb
	UINT32 pngColorType = PNG_COLOR_TYPE_GRAY;
	if (bitCount == 0x20) {
		pngColorType = PNG_COLOR_TYPE_RGBA;
	}
	else if (bitCount == 0x18) {
		pngColorType = PNG_COLOR_TYPE_RGB;
	}
	png_set_IHDR(pngPtr, pngInfoPtr, width, height, 8, pngColorType, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	// pngPtr 정보 구조체 기록
	png_write_info(pngPtr, pngInfoPtr);

	// 픽셀당 바이트 수와 데이터 내의 실제 너비 픽셀 수
	UINT32 bytesPerPixel = (bitCount / 8);
	UINT32 widthPixel = (bytesPerPixel * width);
	widthPixel += (4 - (widthPixel % 4)) % 4;

	// row pointer 배열 할당 후 데이터 옮기기
	pngRowArray = (png_bytep*)malloc(height * sizeof(png_bytep));
	for (UINT32 y = 0; y < height; y++) {
		pngRowArray[y] = (png_bytep)malloc(sizeof(png_byte) * bytesPerPixel * width);
		BYTE* _org_data_pnt = (imageBuffer + (widthPixel * y));
		for (UINT32 x = 0; x < width; x++) {
			pngRowArray[y][(x * bytesPerPixel) + 2] = _org_data_pnt[(x * bytesPerPixel) + 0];		//*b in org_data 
			pngRowArray[y][(x * bytesPerPixel) + 1] = _org_data_pnt[(x * bytesPerPixel) + 1];		//*g in org_data
			pngRowArray[y][(x * bytesPerPixel) + 0] = _org_data_pnt[(x * bytesPerPixel) + 2];		//*r in org_data
			// 알파값이 있는 경우, 또한 transparent를 무시하고 추출한다면 그냥 0xFF로 박아버린다
			if (bytesPerPixel > 3) {
				if (isNoneTransparentMode) {
					pngRowArray[y][(x * bytesPerPixel) + 3] = 0xFF;
				}
				else {
					pngRowArray[y][(x * bytesPerPixel) + 3] = _org_data_pnt[(x * bytesPerPixel) + 3];	//*a in org_data
				}
			}
		}
	}

	// 컬러 데이터 기록
	png_write_image(pngPtr, pngRowArray);
	png_write_end(pngPtr, NULL);

	// 정리
	png_free_data(pngPtr, pngInfoPtr, PNG_FREE_ALL, -1);
	png_destroy_write_struct(&pngPtr, (png_infopp)NULL);
	for (UINT32 y = 0; y < height; y++) {
		free(pngRowArray[y]);
	}
	free(pngRowArray);
	fclose(pngFilePtr);
	return true;
}


bool CTT::PngUtil::readFromPng(BYTE*& imageBuffer, wstring pngPath, UINT32& width, UINT32& height, UINT32& bitCount)
{
	// 변수 준비
	FILE* pngFilePtr;
	png_structp pngPtr = NULL;
	png_infop pngInfoPtr = NULL;
	INT32 numberOfPasses = 0;
	png_bytep* pngRowArray = NULL;
	char frontHeader[8];		// pngPtr 파일인지 점검할 헤더

	// pngPtr 파일 읽어들일 준비 및 점검
	pngFilePtr = _wfopen(pngPath.c_str(), L"rb");
	fread(frontHeader, 1, 8, pngFilePtr);
	if (png_sig_cmp((png_const_bytep)frontHeader, 0, 8) != 0) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(pngPath) << L": png 파일로 인식되지 않습니다." << endl;
		fclose(pngFilePtr);
		imageBuffer = NULL;
		width = height = bitCount = 0;
		return false;
	}

	// 읽기 전용 구조체 생성
	pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, pngErrorHandler, NULL);

	// 정보 구조체 초기화
	pngInfoPtr = png_create_info_struct(pngPtr);

	// 예외 핸들링
	if (setjmp(png_jmpbuf(pngPtr))) {
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << getOnlyName(pngPath) << L": 오류 발생!" << endl;
		png_destroy_read_struct(&pngPtr, &pngInfoPtr, NULL);
		fclose(pngFilePtr);
		return false;
	}

	// i/o(?) 세팅
	png_init_io(pngPtr, pngFilePtr);

	// 시그널 바이트 세팅
	png_set_sig_bytes(pngPtr, 8);

	// 정보 읽어들이기
	png_read_info(pngPtr, pngInfoPtr);

	width = png_get_image_width(pngPtr, pngInfoPtr);
	height = png_get_image_height(pngPtr, pngInfoPtr);
	UINT32 pngColorType = png_get_color_type(pngPtr, pngInfoPtr);
	UINT32 bitDepth = png_get_bit_depth(pngPtr, pngInfoPtr);
	numberOfPasses = png_set_interlace_handling(pngPtr);

	// rgba / rgb / 디폴트(단색) 세팅
	if (pngColorType == PNG_COLOR_TYPE_RGBA) {
		bitCount = bitDepth * 4;
	}
	else if (pngColorType == PNG_COLOR_TYPE_RGB) {
		bitCount = bitDepth * 3;
	}
	else {
		bitCount = bitDepth * 3;
	}

	// 필요한 값들 읽어들이는 한편 필요한 정보를 업뎃해주기
	png_read_update_info(pngPtr, pngInfoPtr);

	// 데이터가 될 공간 할당 후 실제로 읽어들이기	
	pngRowArray = (png_bytep*)malloc(sizeof(png_bytep) * height);
	for (UINT32 y = 0; y < height; y++) {
		pngRowArray[y] = (png_byte*)malloc(png_get_rowbytes(pngPtr, pngInfoPtr));
	}
	png_read_image(pngPtr, pngRowArray);

	// 픽셀당 바이트 수와 데이터 내의 실제 너비 픽셀 수
	UINT32 bytesPerPixel = (bitCount / 8);
	UINT32 widthPixel = (bytesPerPixel * height);
	widthPixel += (4 - (widthPixel % 4)) % 4;

	// 공간 할당 후 옮기기
	imageBuffer = (BYTE*)malloc(width * height * (bitCount / 8));
	memset(imageBuffer, 0, (width * height * (bitCount / 8)));
	for (UINT32 y = 0; y < height; y++) {
		BYTE* imageDataPtr = (imageBuffer + ((width * (bitCount / 8)) * y));
		for (UINT32 x = 0; x < width; x++) {
			imageDataPtr[(x * bytesPerPixel) + 0] = pngRowArray[y][(x * bytesPerPixel) + 2];		//*b in org_data 
			imageDataPtr[(x * bytesPerPixel) + 1] = pngRowArray[y][(x * bytesPerPixel) + 1];		//*g in org_data
			imageDataPtr[(x * bytesPerPixel) + 2] = pngRowArray[y][(x * bytesPerPixel) + 0];		//*r in org_data
			// 알파값이 있는 경우
			if (bytesPerPixel > 3) {
				imageDataPtr[(x * bytesPerPixel) + 3] = pngRowArray[y][(x * bytesPerPixel) + 3];	//*a in org_data
			}
		}
	}

	// 정리
	fclose(pngFilePtr);
	png_free_data(pngPtr, pngInfoPtr, PNG_FREE_ALL, -1);
	png_destroy_read_struct(&pngPtr, &pngInfoPtr, (png_infopp)NULL);
	for (UINT32 y = 0; y < height; y++) {
		free(pngRowArray[y]);
	}
	free(pngRowArray);
	return true;
}




// 메모리상의 PNG 데이터를 읽어서 텍스처를 만드는 함수
HRESULT CTT::PngUtil::createTextureFromPngMemory(BYTE* pngData, size_t pngSize, LPDIRECT3DDEVICE9 device, LPDIRECT3DTEXTURE9* ppTexture)
{
	// pngPtr 포인터 및 관련 변수 초기화
	png_structp pngPtr = NULL;
	png_infop pngInfoPtr = NULL;
	png_bytepp pngRowArray = NULL;

	// png 기록용 구조체 생성
	pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, pngErrorHandler, NULL);

	// png 정보 구조체 생성
	pngInfoPtr = png_create_info_struct(pngPtr);

	if (setjmp(png_jmpbuf(pngPtr))) {
		png_destroy_read_struct(&pngPtr, &pngInfoPtr, NULL);
		return -1;
	}

	// PNG 데이터를 메모리에서 읽기 위한 설정
	png_set_read_fn(pngPtr, &pngData, [](png_structp png, png_bytep data, png_size_t length) {
		BYTE** pngData = (BYTE**)png_get_io_ptr(png);
		memcpy(data, *pngData, length);
		*pngData += length;
	});

	png_set_compression_level(pngPtr, Z_BEST_COMPRESSION);

	// PNG 파일 읽기
	png_set_sig_bytes(pngPtr, 0);
	png_read_info(pngPtr, pngInfoPtr);

	int width = png_get_image_width(pngPtr, pngInfoPtr);
	int height = png_get_image_height(pngPtr, pngInfoPtr);

	// RGBA 포맷으로 설정
	png_set_expand(pngPtr);
	png_set_palette_to_rgb(pngPtr);
	png_set_tRNS_to_alpha(pngPtr);
	png_set_filler(pngPtr, 0, PNG_FILLER_AFTER);
	png_read_update_info(pngPtr, pngInfoPtr);

	// 텍스처 생성
	HRESULT hr = D3DXCreateTexture(device, width, height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, ppTexture);
	if (FAILED(hr)) {
		png_destroy_read_struct(&pngPtr, &pngInfoPtr, NULL);
		return hr;
	}

	// 비트맵 데이터 할당
	BYTE* data = (BYTE*)malloc(width * height * 4); // RGBA 데이터
	pngRowArray = (png_bytep*)malloc(sizeof(png_bytep) * height);
	for (UINT32 y = 0; y < height; y++) {
		pngRowArray[y] = data + y * width * 4; // RGBA로 변환
	}

	png_read_image(pngPtr, pngRowArray);

	// 텍스처에 데이터 복사, 너비가 좁으면 pitch가 0x80으로 고정되는 말도안되는 현상이 있어서 어쩔 수 없이 지정함
	D3DLOCKED_RECT lockedRect;
	hr = (*ppTexture)->LockRect(0, &lockedRect, NULL, 0);
	if (SUCCEEDED(hr)) {
		for (int y = 0; y < height; y++) {
			memcpy((BYTE*)lockedRect.pBits + y * lockedRect.Pitch, data + y * width * 4, width * 4);
		}
		(*ppTexture)->UnlockRect(0);
	}

	// 메모리 해제
	free(data);
	png_destroy_read_struct(&pngPtr, &pngInfoPtr, NULL);

	return hr;
}




void pngErrorHandler(png_structp pngPtr, png_const_charp message) {
	wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": " << L"PNG 오류: " << message << endl;
	longjmp(png_jmpbuf(pngPtr), 1);
}