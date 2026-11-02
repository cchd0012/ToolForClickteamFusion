#include "Dx9Application.h"
#include "../ImGUI/Headers.h"
#include "../Chunk/Image/Headers.h"
#include "../Chunk/Sound/Headers.h"
#include "../Util/CommonUtil.h"

#include <shellapi.h>

using namespace CTT::App;
using namespace CTT::Archive;
using namespace CTT::Chunk;


// 필요한 UI 렌더링 함수
void Dx9Application::render()
{
	//ImGui::ShowDemoWindow();
	//return;

	IM_ASSERT(ImGui::GetCurrentContext() != NULL && "Missing Dear ImGui context. Refer to examples app!");
	IMGUI_CHECKVERSION();

	// x버튼 누를때 창 닫기, dirty 체크
	if (!states.isOpen) {
		if (states.isDirty) {
			renderDirtyCloseModal();
		}
		else {
			PostQuitMessage(0);
		}
	}

	// 시작 스위치(--open)로 받은 아카이브를 처음 한 번 연다
	if (states.startupArchivePath[0] != 0) {
		strcpy_s(states.openArchivePath, states.startupArchivePath);
		states.startupArchivePath[0] = 0;
		states.isArchiveOpened = openArchive();
		hangul.showWindow = states.isArchiveOpened && (states.startupShowAtlas || states.startupShowBlitter || states.startupShowModule);
		hangul.requestTab = states.startupShowAtlas ? 0 : (states.startupShowBlitter ? 1 : (states.startupShowModule ? 2 : -1));
	}

	// 메뉴 탭 렌더링
	renderMenuTab();

	// (아카이브를 열었을 때) 아카이브 정보, Chunk 리스트 윈도우 렌더링
	if (states.isArchiveOpened) {
		renderArchiveInfoWindow();
		renderArchiveChunkListWindow();
		renderArchiveEachChunkInfoWindow();

		// 한글화 도구 창 (수정 단계)
		if (hangul.showWindow) {
			renderHangulToolWindow();
		}
	}

	// 에러가 있다면 에러 메시지 박스 렌더링
	if (states.isError) {
		renderErrorModal();
	}
	else {
		// 아카이브 열기 모달 렌더링, dirty 체크
		if (states.isOpenModalShow) {
			if (states.isDirty) {
				renderDirtyArchiveOpenModal();
			}
			else {
				renderArchiveOpenModal();
			}
		}
		// 아카이브 저장 모달 렌더링
		if (states.isSaveModalShow) {
			renderArchiveSaveModal();
		}
		// 아카이브 전체 추출 모달 렌더링
		if (states.isExtractModalShow) {
			renderArchiveExtractModal();
		}
		// 아카이브 부분 추출 모달 렌더링
		if (states.isPartExtractModalShow) {
			renderArchivePartExtractModal();
		}
		// 아카이브 전체 교체 모달 렌더링
		if (states.isReplaceModalShow) {
			renderArchiveReplaceModal();
		}
		// 아카이브 부분 교체 모달 렌더링
		if (states.isPartReplaceModalShow) {
			renderArchivePartReplaceModal();
		}
	}

}




// ------------------- BEGIN [메뉴] ---------------------


// 메뉴 탭을 렌더링하는 함수
void Dx9Application::renderMenuTab()
{
	ImGuiWindowFlags menuFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;

	// 메뉴는 최상단에 위치
	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, (float)states.menuHeight), ImGuiCond_Always);

	if (!ImGui::Begin("ToolForClickteamFusion_Gui_ver1.0.01", &states.isOpen, menuFlags)) {
		ImGui::End();
		return;
	}

    if (ImGui::BeginMenuBar())
    {

		// 현재 마우스 위치 감지 -> draggable 변수 체크용, 순간적으로 영역을 벗어날 경우는 draggable이 안되는 경우가 있어서 그건 제외함
		ImGuiIO& io = ImGui::GetIO();
		ImVec2 mousePos = io.MousePos; 
		states.isDraggable = (mousePos.y <= states.menuHeight);

		// 아카이브
        if (ImGui::BeginMenu(u8"아카이브"))
        {
			// 열기
			if (ImGui::MenuItem(u8"열기")) {
				states.isOpenModalShow = true;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(u8"ClickteamFusion 아카이브 파일을 엽니다");
			}
			// 저장 [아카이브를 열지 않거나 dirty 상태가 아니라면 비활성화]
			if (!states.isArchiveOpened || !states.isDirty) {
				ImGui::BeginDisabled(true);
			}
			if (ImGui::MenuItem(u8"저장")) {
				states.isSaveModalShow = true;
				states.totalSavingDataCount = 0;
				states.currentSavingDataIndex = 0;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(u8"'~_new' 가 붙은 새 아카이브 파일로 저장합니다");
			}
			// [아카이브를 열지 않거나 dirty 상태가 아니라면 비활성화]
			if (!states.isArchiveOpened || !states.isDirty) {
				ImGui::EndDisabled();
			}
			ImGui::EndMenu();
        }
		// 추출
		if (ImGui::BeginMenu(u8"1.추출"))
		{
			ImGui::TextDisabled(u8"1단계: 아카이브의 데이터를 파일(PNG, JSON 등)로 꺼냅니다");
			ImGui::Separator();
			// [아카이브를 열지 않았다면 비활성화]
			if (!states.isArchiveOpened) {
				ImGui::BeginDisabled(true);
			}
			// 전체 추출
			if (ImGui::MenuItem(u8"전체 추출")) {
				states.isExtractModalShow = true;
				states.totalDataCount = 0;
				states.currentDataIndex = 0;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(u8"추출 가능한 모든 Chunk 데이터를\n아카이브와 같은 이름을 가진 폴더에 추출합니다");
			}
			// 지정한 유형 Chunk만 추출
			if (ImGui::MenuItem(u8"지정한 유형 Chunk만 추출")) {
				states.isPartExtractModalShow = true;
				states.totalDataCount = 0;
				states.currentDataIndex = 0;
				// 기본적으로 전부 추출하는 것으로 체크해둠 (투명은 기본적으로 x)
				states.usingImage = true;
				states.usingImageAndExtractNoneTransparent = false;
				states.usingSound = true;
				states.usingFont = true;
				states.usingModule = true;
				states.usingShader = true;
				states.usingBinaryData = true;
				states.usingText = true;
				states.usingScript = true;
					states.usingFlow = true;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(u8"지정한 타입의 Chunk 데이터만\n아카이브와 같은 위치에 같은 이름을 가진\n폴더에 추출합니다");
			}
			// [아카이브를 열지 않았다면 비활성화]
			if (!states.isArchiveOpened) {
				ImGui::EndDisabled();
			}
			ImGui::EndMenu();
		}
			// 수정
			if (ImGui::BeginMenu(u8"2.수정"))
			{
				ImGui::TextDisabled(u8"2단계: 추출된 파일을 고칩니다. 다 고치면 '3.교체'를 하세요");
				ImGui::Separator();
				// [아카이브를 열지 않았다면 비활성화]
				if (!states.isArchiveOpened) {
					ImGui::BeginDisabled(true);
				}
				wstring baseDir;
				if (states.isArchiveOpened) {
					wstring archivePathForEdit = archive->getArchivePath();
					baseDir = CTT::CommonUtil::getExtentionRemovedPath(archivePathForEdit);
				}
				// 폴더 열기: 추출한 폴더가 없으면 비활성화한다
				auto folderItem = [&](const char* label, const wchar_t* sub, const char* tip) {
					wstring folder = baseDir + L"/" + sub;
					DWORD attr = baseDir.empty() ? INVALID_FILE_ATTRIBUTES : GetFileAttributesW(folder.c_str());
					bool exists = (attr != INVALID_FILE_ATTRIBUTES) && ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0);
					if (!exists) {
						ImGui::BeginDisabled(true);
					}
					if (ImGui::MenuItem(label)) {
						ShellExecuteW(NULL, L"open", folder.c_str(), NULL, NULL, SW_SHOWNORMAL);
					}
					if (!exists) {
						ImGui::EndDisabled();
					}
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
						ImGui::SetTooltip(exists ? tip : u8"추출한 폴더가 없습니다. 먼저 '1.추출'을 하세요");
					}
				};
				if (ImGui::BeginMenu(u8"텍스트")) {
					folderItem(u8"텍스트 폴더 열기", L"text", u8"frameEventText.json, objectText.json 의 translation 칸에 번역을 적습니다");
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu(u8"이미지")) {
					folderItem(u8"이미지 폴더 열기", L"image", u8"PNG 를 같은 이름으로 고치거나 appendImages.json 으로 새 이미지를 추가합니다");
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu(u8"폰트")) {
					folderItem(u8"폰트 폴더 열기", L"font", u8"TrueType/LogFont 폰트 파일을 바꿉니다");
					ImGui::Separator();
					ImGui::TextDisabled(u8"한글화 도구: 글자 세트를 만들어 오브젝트에 적용하고 모듈에 심습니다");
					if (ImGui::MenuItem(u8"가. 글자 세트 만들기...")) {
						hangul.showWindow = true;
						hangul.requestTab = 0;
					}
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
						ImGui::SetTooltip(u8"폰트 파일에서 한글 글자 그림(글자 세트)을 만듭니다");
					}
					if (ImGui::MenuItem(u8"나. 오브젝트에 적용...")) {
						hangul.showWindow = true;
						hangul.requestTab = 1;
					}
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
						ImGui::SetTooltip(u8"글자를 그리는 오브젝트마다 쓸 글자 세트와 줄 폭 계산 방식을 정하고,\n그 오브젝트에 나올 문구를 미리 봅니다");
					}
					if (ImGui::MenuItem(u8"다. 게임에 넣기...")) {
						hangul.showWindow = true;
						hangul.requestTab = 2;
					}
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
						ImGui::SetTooltip(u8"글자 세트를 txtblt.mfx 모듈에 심어 새 모듈을 만듭니다");
					}
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu(u8"스크립트")) {
					folderItem(u8"스크립트 폴더 열기", L"script", u8"frame/ 의 이벤트, object/ 의 오브젝트 JSON 을 고칩니다");
					ImGui::EndMenu();
				}
				// [아카이브를 열지 않았다면 비활성화]
				if (!states.isArchiveOpened) {
					ImGui::EndDisabled();
				}
				ImGui::EndMenu();
			}
		// 교체
		if (ImGui::BeginMenu(u8"3.교체 (재합성)"))
		{
			ImGui::TextDisabled(u8"3단계: 고친 파일을 아카이브에 합쳐 새 아카이브(~_new)로 저장합니다");
			ImGui::Separator();
			// [아카이브를 열지 않았다면 비활성화]
			if (!states.isArchiveOpened) {
				ImGui::BeginDisabled(true);
			}
			// 전체 교체
			if (ImGui::MenuItem(u8"전체 교체")) {
				states.isReplaceModalShow = true;
				states.totalDataCount = 0;
				states.currentDataIndex = 0;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(u8"아카이브와 같은 위치에 같은 이름을 가진 폴더에\n추출되어 있는 모든 Chunk 데이터를 교체하여\n'~_new' 가 붙은 새 아카이브 파일로 저장합니다\n(이 과정에서 변경된 아카이브 정보도 자동으로 저장됩니다)");
			}
			// 지정한 유형 Chunk만 교체
			if (ImGui::MenuItem(u8"지정한 유형 Chunk만 교체")) {
				states.isPartReplaceModalShow = true;
				states.totalDataCount = 0;
				states.currentDataIndex = 0;
				// 기본적으로 전부 교체하는 것으로 체크해둠 (투명은 기본적으로 x, usingShader는 예외)
				states.usingImage = true;
				states.usingImageAndExtractNoneTransparent = false;
				states.usingSound = true;
				states.usingFont = true;
				states.usingModule = true;
				states.usingShader = false;
				states.usingBinaryData = true;
				states.usingText = true;
				states.usingScript = true;
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(u8"아카이브와 같은 위치에 같은 이름을 가진 폴더에\n추출되어 있는 데이터 중 지정한 타입의 Chunk 데이터를 교체하여\n'~_new' 가 붙은 새 아카이브 파일로 저장합니다\n(이 과정에서 변경된 아카이브 정보도 자동으로 저장됩니다)");
			}
			// [아카이브를 열지 않았다면 비활성화]
			if (!states.isArchiveOpened) {
				ImGui::EndDisabled();
			}
			ImGui::EndMenu();
		}
		// 기타
		if (ImGui::BeginMenu(u8"기타"))
		{
			// 정보
			if (ImGui::MenuItem(u8"정보")) {
			}
			ImGui::EndMenu();
		}
        ImGui::EndMenuBar();
    }

	ImGui::End();
}


// ------------------- END [메뉴] ---------------------




// ------------------- BEGIN [모달] ---------------------


// 프로그램 닫기 모달 (저장이 덜 된 상태에서 호출)
void Dx9Application::renderDirtyCloseModal()
{
	ImGuiWindowFlags modalFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove;

	// 너비 설정. 높이는 0.0f로 하면 그냥 자동 설정된다
	ImGui::SetNextWindowSize(ImVec2(400.0f, 0.0f), ImGuiCond_Always);

	ImGui::OpenPopup(u8"종료");
	if (ImGui::BeginPopupModal(u8"종료", NULL, modalFlags)) {

		// 에러 텍스트 세팅
		ImGui::Text(u8"아직 저장되지 않은 수정 내용이 있습니다.\n이대로 종료하겠습니까?");

		float modalWidth = ImGui::GetContentRegionAvail().x;
		float buttonsWidth = ImGui::CalcTextSize(u8"종료").x + ImGui::CalcTextSize(u8"취소").x + ((ImGui::GetStyle().FramePadding.x * 2) + states.buttonGap);

		// 확인 버튼 (오른쪽 정렬)
		ImGui::SetCursorPosX(modalWidth - buttonsWidth);
		if (ImGui::Button(u8"종료"))
		{
			// 그냥 끝낸다고 신호를 보낸다
			ImGui::CloseCurrentPopup();
			PostQuitMessage(0);
		}
		// --- 같은 라인 ---
		ImGui::SameLine();
		// 취소 버튼 (오른쪽 정렬)
		if (ImGui::Button(u8"취소"))
		{
			// close되려던 상태를 되돌린다
			ImGui::CloseCurrentPopup();
			states.isOpen = true;
		}
		ImGui::EndPopup();

	}
}


// 아카이브 열기 모달 (저장이 덜 된 상태에서 호출)
void Dx9Application::renderDirtyArchiveOpenModal()
{
	ImGuiWindowFlags modalFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove;

	// 너비 설정. 높이는 0.0f로 하면 그냥 자동 설정된다
	ImGui::SetNextWindowSize(ImVec2(400.0f, 0.0f), ImGuiCond_Always);

	ImGui::OpenPopup(u8"열기");
	if (ImGui::BeginPopupModal(u8"열기", NULL, modalFlags)) {

		// 에러 텍스트 세팅
		ImGui::Text(u8"아직 저장되지 않은 수정 내용이 있습니다.\n이대로 아카이브를 열겠습니까?");

		float modalWidth = ImGui::GetContentRegionAvail().x;
		float buttonsWidth = ImGui::CalcTextSize(u8"열기").x + ImGui::CalcTextSize(u8"취소").x + ((ImGui::GetStyle().FramePadding.x * 2) + states.buttonGap);

		// 열기 버튼 (오른쪽 정렬)
		ImGui::SetCursorPosX(modalWidth - buttonsWidth);
		if (ImGui::Button(u8"열기"))
		{
			// 열겠다고 세팅한 후 dirty를 해제한다
			ImGui::CloseCurrentPopup();
			states.isDirty = false;
		}
		// --- 같은 라인 ---
		ImGui::SameLine();
		// 취소 버튼 (오른쪽 정렬)
		if (ImGui::Button(u8"취소"))
		{
			// 열려던 행위를 취소시킨다
			ImGui::CloseCurrentPopup();
			states.isOpenModalShow = false;
		}
		ImGui::EndPopup();

	}
}


// 에러 모달
void Dx9Application::renderErrorModal()
{
	ImGuiWindowFlags modalFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove;

	// 너비 설정. 높이는 0.0f로 하면 그냥 자동 설정된다
	ImGui::SetNextWindowSize(ImVec2(400.0f, 0.0f), ImGuiCond_Always);

	ImGui::OpenPopup(states.lastErrorTitle);
	if (ImGui::BeginPopupModal(states.lastErrorTitle, NULL, modalFlags)) {

		// 에러 텍스트 세팅
		ImGui::Text(states.lastErrorMsg);

		float modalWidth = ImGui::GetContentRegionAvail().x;
		float buttonsWidth = ImGui::CalcTextSize(u8"확인").x;

		// 확인 버튼 (오른쪽 정렬)
		ImGui::SetCursorPosX(modalWidth - buttonsWidth);
		if (ImGui::Button(u8"확인"))
		{
			ImGui::CloseCurrentPopup();
			states.isError = false;
		}
		ImGui::EndPopup();

	}
}


// 아카이브 열기 모달
void Dx9Application::renderArchiveOpenModal()
{
	ImGuiWindowFlags modalFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove;

	ImGui::OpenPopup(u8"아카이브 열기");
	if (ImGui::BeginPopupModal(u8"아카이브 열기", NULL, modalFlags)) {

		// 파일 선택 버튼
		if (ImGui::Button(u8"파일 선택")) {
			// 파일 선택 다이얼로그
			IGFD::FileDialogConfig config;
			config.path = ".";
			ImGui::SetNextWindowPos(ImVec2(20.0f, 40.0f), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x - 40.0f, ImGui::GetIO().DisplaySize.y - 80.0f), ImGuiCond_Always);
			ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", u8"ClickteamFusion 아카이브 파일을 선택하세요", ".exe,.dat,.ccn", config);
		}
		// --- 같은 라인 ---
		ImGui::SameLine(140.0f, 0.0f);
		// 선택한 파일 경로 (readonly)
		ImGui::SetNextItemWidth(560.0f);
		ImGui::InputText("##ArchivePath", states.openArchivePath, IM_ARRAYSIZE(states.openArchivePath), ImGuiInputTextFlags_ReadOnly);
		if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey", ImGuiWindowFlags_NoResize)) {
			if (ImGuiFileDialog::Instance()->IsOk()) {
				// ImGuiFileDialog::Instance()->GetFilePathName() : 파일 절대경로
				// ImGuiFileDialog::Instance()->GetFilePath() : 파일이 위치한 폴더 절대경로
				string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
				memset(states.openArchivePath, 0, MAX_UTF8_PATH);
				memcpy(states.openArchivePath, filePathName.c_str(), strlen(filePathName.c_str()));
			}
			// 닫기
			ImGuiFileDialog::Instance()->Close();
		}

		// 구분선
		ImGui::Separator();

		float modalWidth = ImGui::GetContentRegionAvail().x;
		float buttonsWidth = ImGui::CalcTextSize(u8"열기").x + ImGui::CalcTextSize(u8"닫기").x + ((ImGui::GetStyle().FramePadding.x * 2) + states.buttonGap);

		// 열기 (오른쪽 정렬)
		ImGui::SetCursorPosX(modalWidth - buttonsWidth);
		// [경로가 없으면 열기 버튼 비활성화]
		if (strlen(states.openArchivePath) == 0) {
			ImGui::BeginDisabled(true);
		}
		if (ImGui::Button(u8"열기")) {
			// 기존에 렌더링하던 노드도 선택을 해제해야 쓸데없이 렌더링하지 않는다
			states.chunkTreeNodeContainer.clear();
			states.isRenderingChunkAppInfo = false;
			states.renderingChunk = NULL;
			// ** 아카이브 파일 열기 **
			states.isArchiveOpened = openArchive();
			if (states.isArchiveOpened) {
				states.isOpenModalShow = false;
				ImGui::CloseCurrentPopup();
			}
		}
		// [경로가 없을 시 열기 버튼 비활성화]
		if (strlen(states.openArchivePath) == 0) {
			ImGui::EndDisabled();
		}
		// --- 같은 라인 ---
		ImGui::SameLine();
		// 닫기 (오른쪽 정렬)
		ImGui::SetCursorPosX(modalWidth - ImGui::CalcTextSize(u8"닫기").x);
		if (ImGui::Button(u8"닫기")) {
			states.isOpenModalShow = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();

	}
}


// 저장용 스레드
DWORD savingThreadFunction(PVOID pParam)
{
	// 단순 저장 작업 수행
	vector<wstring> list;
	getApp()->getArchive()->rewriteNewArchive(list);
	// 저장 종료 세팅
	getApp()->getStates()->savingThreadState = ThreadState::THREAD_FINISHED;
	return 0;
}

// 아카이브 저장 모달
void Dx9Application::renderArchiveSaveModal()
{
	ImGuiWindowFlags modalFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove;

	ImGui::OpenPopup(u8"아카이브 저장");
	if (ImGui::BeginPopupModal(u8"아카이브 저장", NULL, modalFlags)) {

		// 저장 스레드 실행 (처음 한번만)
		if (states.savingThreadState == ThreadState::THREAD_READY) {
			states.savingThreadState = ThreadState::THREAD_EXECUTING;
			CreateThread(NULL, 0, savingThreadFunction, NULL, 0, NULL);
		}

		// 프로그레스 바 세팅
		float savingProgress = 0.0f;
		if (states.totalSavingDataCount != 0) {
			savingProgress = (float)states.currentSavingDataIndex / (float)states.totalSavingDataCount;
		}
		ImGui::ProgressBar(savingProgress, ImVec2(0.0f, 0.0f));

		// 저장이 끝났으면 상태 초기화 후 이 모달이 꺼지도록 세팅하며, dirty 상태도 해제한다
		if (states.savingThreadState == ThreadState::THREAD_FINISHED) {
			states.isSaveModalShow = false;
			states.savingThreadState = ThreadState::THREAD_READY;
			states.isDirty = false;
			ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
			return;
		}

		ImGui::EndPopup();

	}
}


// 추출용 스레드
DWORD extractingThreadFunction(PVOID pParam)
{
	// 추출 가능한 리스트 확보
	vector<wstring> extractingTypes;
	if (getApp()->getStates()->usingImage) {
		extractingTypes.push_back(L"image");
	}
	if (getApp()->getStates()->usingSound) {
		extractingTypes.push_back(L"sound");
	}
	if (getApp()->getStates()->usingFont) {
		extractingTypes.push_back(L"font");
	}
	if ((getApp()->getArchive()->getArchiveType() == ArchiveType::ARCHIVE_EXE) && getApp()->getStates()->usingModule) {
		extractingTypes.push_back(L"module");
	}
	if (getApp()->getStates()->usingShader) {
		extractingTypes.push_back(L"appShader");
	}
	if (getApp()->getStates()->usingBinaryData) {
		extractingTypes.push_back(L"blobs");
	}
	if (getApp()->getStates()->usingText) {
		extractingTypes.push_back(L"text");
	}
	if (getApp()->getStates()->usingScript) {
		extractingTypes.push_back(L"script");
	}
	if (getApp()->getStates()->usingFlow) {
		extractingTypes.push_back(L"flow");
	}
	// 전체 개수 세팅
	getApp()->getArchive()->setDataCount(extractingTypes);
	// 추출 작업 수행
	getApp()->getArchive()->exportFromArchive(extractingTypes);
	// 추출 종료 세팅
	getApp()->getStates()->extractingThreadState = ThreadState::THREAD_FINISHED;
	return 0;
}

// 아카이브 추출 모달
void Dx9Application::renderArchiveExtractModal()
{
	ImGuiWindowFlags modalFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove;

	ImGui::OpenPopup(u8"데이터 추출");
	if (ImGui::BeginPopupModal(u8"데이터 추출", NULL, modalFlags)) {

		// 추출 스레드 실행 (처음 한번만, 투명은 기본적으로 x), 전체를 추출하므로 현재 가능한 모든 타입을 넣어준다
		if (states.extractingThreadState == ThreadState::THREAD_READY) {
			states.extractingThreadState = ThreadState::THREAD_EXECUTING;
			states.usingImage = true;
			states.usingImageAndExtractNoneTransparent = false;
			states.usingSound = true;
			states.usingFont = true;
			states.usingModule = true;
			states.usingShader = true;
			states.usingBinaryData = true;
			states.usingText = true;
			states.usingScript = true;
			states.usingFlow = true;
			CreateThread(NULL, 0, extractingThreadFunction, NULL, 0, NULL);
		}

		// 프로그레스 바 세팅
		float extractingProgress = 0.0f;
		if (states.totalDataCount != 0) {
			extractingProgress = (float)states.currentDataIndex / (float)states.totalDataCount;
		}
		ImGui::ProgressBar(extractingProgress, ImVec2(0.0f, 0.0f));

		// 추출이 끝났으면 상태 초기화 후 이 모달이 꺼지도록 세팅한다
		if (states.extractingThreadState == ThreadState::THREAD_FINISHED) {
			states.isExtractModalShow = false;
			states.extractingThreadState = ThreadState::THREAD_READY;
			ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
			return;
		}

		ImGui::EndPopup();

	}
}

// 아카이브 부분 추출 모달
void Dx9Application::renderArchivePartExtractModal()
{
	ImGuiWindowFlags modalFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove;

	ImGui::OpenPopup(u8"데이터 부분 추출");
	if (ImGui::BeginPopupModal(u8"데이터 부분 추출", NULL, modalFlags)) {

		// 추출 가능 목록 나열
		ImGui::SeparatorText(u8"추출 타입");

		// [추출 중이면 버튼 비활성화]
		if (states.extractingThreadState == ThreadState::THREAD_EXECUTING) {
			ImGui::BeginDisabled(true);
		}
		ImGui::Checkbox(u8"이미지", &states.usingImage);
		if (states.usingImage) {
			ImGui::Checkbox(u8" - 추출시 alpha값을 255로 고정", &states.usingImageAndExtractNoneTransparent);
		}
		ImGui::Checkbox(u8"사운드", &states.usingSound);
		ImGui::Checkbox(u8"폰트", &states.usingFont);
		// exe일 경우 모듈 추출 추가
		if (archive->getArchiveType() == ArchiveType::ARCHIVE_EXE) {
			ImGui::Checkbox(u8"모듈", &states.usingModule);
		}
		ImGui::Checkbox(u8"셰이더 (추출만 가능)", &states.usingShader);
		ImGui::Checkbox(u8"기타 Blob 데이터", &states.usingBinaryData);
		ImGui::Checkbox(u8"텍스트", &states.usingText);
		ImGui::Checkbox(u8"스크립트", &states.usingScript);
		ImGui::Checkbox(u8"텍스트 흐름 분석 (추출만 가능, 블리터가 받는 문자열)", &states.usingFlow);
		// [추출 중이면 버튼 비활성화]
		if (states.extractingThreadState == ThreadState::THREAD_EXECUTING) {
			ImGui::EndDisabled();
		}

		// 추출 진행도
		ImGui::SeparatorText(u8"추출 진행도");

		// 프로그레스 바 세팅
		float extractingProgress = 0.0f;
		if (states.totalDataCount != 0) {
			extractingProgress = (float)states.currentDataIndex / (float)states.totalDataCount;
		}
		ImGui::ProgressBar(extractingProgress, ImVec2(0.0f, 0.0f));

		// 추출이 끝났으면 상태 초기화만 해 주고, 추출 목록은 선택된 채로 놔둔다
		if (states.extractingThreadState == ThreadState::THREAD_FINISHED) {
			states.extractingThreadState = ThreadState::THREAD_READY;
		}

		// 구분선
		ImGui::Separator();

		float modalWidth = ImGui::GetContentRegionAvail().x;
		float buttonsWidth = ImGui::CalcTextSize(u8"추출").x + ImGui::CalcTextSize(u8"취소").x + ((ImGui::GetStyle().FramePadding.x * 2) + states.buttonGap);

		static bool threadStateChange = false;

		// 아무것도 체크되지 않았는지 여부
		bool isNoneSelected = !states.usingImage && !states.usingSound && !states.usingFont && !states.usingShader && !states.usingBinaryData && !states.usingText && !states.usingScript && !states.usingFlow;
		if (archive->getArchiveType() == ArchiveType::ARCHIVE_EXE) {
			isNoneSelected = isNoneSelected && !states.usingModule;
		}

		// [추출 중 / 아무것도 체크되지 않으면 버튼 비활성화]
		if ((states.extractingThreadState == ThreadState::THREAD_EXECUTING) || isNoneSelected) {
			ImGui::BeginDisabled(true);
		}
		// 추출 (오른쪽 정렬)
		ImGui::SetCursorPosX(modalWidth - buttonsWidth);
		if (ImGui::Button(u8"추출")) {
			// 추출 스레드 실행 (처음 한번만)
			if (states.extractingThreadState == ThreadState::THREAD_READY) {
				states.extractingThreadState = ThreadState::THREAD_EXECUTING;
				threadStateChange = true;
				CreateThread(NULL, 0, extractingThreadFunction, NULL, 0, NULL);
			}
		}
		// [추출 중 / 아무것도 체크되지 않으면 버튼 비활성화, 상태가 바뀔 때는 BeginDisabled 처리가 안돼있기 때문에 EndDisabled 처리하면 에러난다]
		if ((states.extractingThreadState == ThreadState::THREAD_EXECUTING) || isNoneSelected) {
			if (threadStateChange) {
				threadStateChange = false;
			}
			else {
				ImGui::EndDisabled();
			}
		}

		// --- 같은 라인 ---
		ImGui::SameLine();

		// [추출 중이면 버튼 비활성화]
		if (states.extractingThreadState == ThreadState::THREAD_EXECUTING) {
			ImGui::BeginDisabled(true);
		}
		// 취소 (오른쪽 정렬, 추출 중에 취소 불가능)
		ImGui::SetCursorPosX(modalWidth - ImGui::CalcTextSize(u8"취소").x);
		if (ImGui::Button(u8"취소")) {
			states.isPartExtractModalShow = false;
			ImGui::CloseCurrentPopup();
		}
		// [추출 중이면 버튼 비활성화]
		if (states.extractingThreadState == ThreadState::THREAD_EXECUTING) {
			ImGui::EndDisabled();
		}

		ImGui::EndPopup();

	}
}


// 교체용 스레드
DWORD replacingThreadFunction(PVOID pParam)
{
	// 교체 가능한 리스트 확보, appShader는 제외
	vector<wstring> replacingTypes;
	if (getApp()->getStates()->usingImage) {
		replacingTypes.push_back(L"image");
	}
	if (getApp()->getStates()->usingSound) {
		replacingTypes.push_back(L"sound");
	}
	if (getApp()->getStates()->usingFont) {
		replacingTypes.push_back(L"font");
	}
	if ((getApp()->getArchive()->getArchiveType() == ArchiveType::ARCHIVE_EXE) && getApp()->getStates()->usingModule) {
		replacingTypes.push_back(L"module");
	}
	if (getApp()->getStates()->usingBinaryData) {
		replacingTypes.push_back(L"blobs");
	}
	if (getApp()->getStates()->usingText) {
		replacingTypes.push_back(L"text");
	}
	if (getApp()->getStates()->usingScript) {
		replacingTypes.push_back(L"script");
	}
	// 전체 교체 개수 세팅
	getApp()->getArchive()->setDataCount(replacingTypes);
	// 교체 작업 수행
	getApp()->getArchive()->rewriteNewArchive(replacingTypes);
	// 교체 종료 세팅
	getApp()->getStates()->replacingThreadState = ThreadState::THREAD_FINISHED;
	return 0;
}

// 아카이브 교체 모달
void Dx9Application::renderArchiveReplaceModal()
{
	ImGuiWindowFlags modalFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove;

	ImGui::OpenPopup(u8"데이터 교체");
	if (ImGui::BeginPopupModal(u8"데이터 교체", NULL, modalFlags)) {

		// 교체 스레드 실행 (처음 한번만), 전체를 교체하므로 현재 가능한 모든 타입을 넣어준다, shader 제외
		if (states.replacingThreadState == ThreadState::THREAD_READY) {
			states.replacingThreadState = ThreadState::THREAD_EXECUTING;
			states.usingImage = true;
			states.usingImageAndExtractNoneTransparent = false;
			states.usingSound = true;
			states.usingFont = true;
			states.usingModule = true;
			states.usingBinaryData = true;
			states.usingText = true;
			states.usingScript = true;
			CreateThread(NULL, 0, replacingThreadFunction, NULL, 0, NULL);
		}

		// 프로그레스 바 세팅
		float replacingProgress = 0.0f;
		if (states.totalDataCount != 0) {
			replacingProgress = (float)states.currentDataIndex / (float)states.totalDataCount;
		}
		ImGui::ProgressBar(replacingProgress, ImVec2(0.0f, 0.0f));

		// 교체가 끝났으면 상태 초기화 후 이 모달이 꺼지도록 세팅하며, dirty 상태도 해제한다
		if (states.replacingThreadState == ThreadState::THREAD_FINISHED) {
			states.isReplaceModalShow = false;
			states.replacingThreadState = ThreadState::THREAD_READY;
			states.isDirty = false;
			ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
			return;
		}

		ImGui::EndPopup();

	}
}

// 아카이브 부분 교체 모달
void Dx9Application::renderArchivePartReplaceModal()
{
	ImGuiWindowFlags modalFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove;

	ImGui::OpenPopup(u8"데이터 부분 교체");
	if (ImGui::BeginPopupModal(u8"데이터 부분 교체", NULL, modalFlags)) {

		// 교체 가능 목록 나열, shader는 제외
		ImGui::SeparatorText(u8"교체 타입");

		// [교체 중이면 버튼 비활성화]
		if (states.replacingThreadState == ThreadState::THREAD_EXECUTING) {
			ImGui::BeginDisabled(true);
		}
		ImGui::Checkbox(u8"이미지", &states.usingImage);
		ImGui::Checkbox(u8"사운드", &states.usingSound);
		ImGui::Checkbox(u8"폰트", &states.usingFont);
		// exe일 경우 모듈 교체 추가
		if (archive->getArchiveType() == ArchiveType::ARCHIVE_EXE) {
			ImGui::Checkbox(u8"모듈", &states.usingModule);
		}
		ImGui::Checkbox(u8"기타 Blob 데이터", &states.usingBinaryData);
		ImGui::Checkbox(u8"텍스트", &states.usingText);
		ImGui::Checkbox(u8"스크립트", &states.usingScript);
		// [교체 중이면 버튼 비활성화]
		if (states.replacingThreadState == ThreadState::THREAD_EXECUTING) {
			ImGui::EndDisabled();
		}

		// 추출 진행도
		ImGui::SeparatorText(u8"교체 진행도");

		// 프로그레스 바 세팅
		float replacingProgress = 0.0f;
		if (states.totalDataCount != 0) {
			replacingProgress = (float)states.currentDataIndex / (float)states.totalDataCount;
		}
		ImGui::ProgressBar(replacingProgress, ImVec2(0.0f, 0.0f));

		// 교체가 끝났으면 상태 초기화만 해 주고, 추출 목록은 선택된 채로 놔두고, dirty 상태도 해제한다
		if (states.replacingThreadState == ThreadState::THREAD_FINISHED) {
			states.replacingThreadState = ThreadState::THREAD_READY;
			states.isDirty = false;
		}

		// 구분선
		ImGui::Separator();

		float modalWidth = ImGui::GetContentRegionAvail().x;
		float buttonsWidth = ImGui::CalcTextSize(u8"교체").x + ImGui::CalcTextSize(u8"취소").x + ((ImGui::GetStyle().FramePadding.x * 2) + states.buttonGap);

		static bool threadStateChange = false;

		// 아무것도 체크되지 않았는지 여부
		bool isNoneSelected = !states.usingImage && !states.usingSound && !states.usingFont && !states.usingBinaryData && !states.usingText && !states.usingScript;
		if (archive->getArchiveType() == ArchiveType::ARCHIVE_EXE) {
			isNoneSelected = isNoneSelected && !states.usingModule;
		}

		// [교체 중 / 아무것도 체크되지 않으면 버튼 비활성화]
		if ((states.replacingThreadState == ThreadState::THREAD_EXECUTING) || isNoneSelected) {
			ImGui::BeginDisabled(true);
		}
		// 교체 (오른쪽 정렬)
		ImGui::SetCursorPosX(modalWidth - buttonsWidth);
		if (ImGui::Button(u8"교체")) {
			// 교체 스레드 실행 (처음 한번만)
			if (states.replacingThreadState == ThreadState::THREAD_READY) {
				states.replacingThreadState = ThreadState::THREAD_EXECUTING;
				threadStateChange = true;
				CreateThread(NULL, 0, replacingThreadFunction, NULL, 0, NULL);
			}
		}
		// [교체 중 / 아무것도 체크되지 않으면 버튼 비활성화, 상태가 바뀔 때는 BeginDisabled 처리가 안돼있기 때문에 EndDisabled 처리하면 에러난다]
		if ((states.replacingThreadState == ThreadState::THREAD_EXECUTING) || isNoneSelected) {
			if (threadStateChange) {
				threadStateChange = false;
			}
			else {
				ImGui::EndDisabled();
			}
		}

		// --- 같은 라인 ---
		ImGui::SameLine();

		// [교체 중이면 버튼 비활성화]
		if (states.replacingThreadState == ThreadState::THREAD_EXECUTING) {
			ImGui::BeginDisabled(true);
		}
		// 취소 (오른쪽 정렬, 추출 중에 취소 불가능)
		ImGui::SetCursorPosX(modalWidth - ImGui::CalcTextSize(u8"취소").x);
		if (ImGui::Button(u8"취소")) {
			states.isPartReplaceModalShow = false;
			ImGui::CloseCurrentPopup();
		}
		// [교체 중이면 버튼 비활성화]
		if (states.replacingThreadState == ThreadState::THREAD_EXECUTING) {
			ImGui::EndDisabled();
		}

		ImGui::EndPopup();

	}
}


// ------------------- END [모달] ---------------------




// ------------------- BEGIN [윈도우] ---------------------


// 아카이브 정보 윈도우
void Dx9Application::renderArchiveInfoWindow()
{
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;

	// 메뉴 바로 밑에 지정
	float xPos = 0.0f;
	float yPos = (float)states.menuHeight;
	ImVec2 windowSize;
	windowSize.x = ImGui::GetIO().DisplaySize.x;
	windowSize.y = (float)states.archiveOpenWindowHeight;
	ImGui::SetNextWindowPos(ImVec2(xPos, yPos), ImGuiCond_Always);
	ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

	// 타이틀에 dirty 반영
	string archiveInfoTitle = !states.isDirty ? u8"아카이브 정보" : u8"아카이브 정보(*)";
	if (!ImGui::Begin(archiveInfoTitle.c_str(), NULL, windowFlags)) {
		ImGui::End();
		return;
	}

	// 게임명 라인 (편집 전환 기능 구현, 이 사이에 UTF-16 <> UTF-8 변환 필요)
	ImGui::Text(u8"게임명:");
	// --- 같은 라인 ---
	ImGui::SameLine(140.0f, 0.0f);
	if (!states.isArchiveGameNameEditing) {
		// 디폴트 모드, 편집 버튼은 y축 오프셋 조정
		string appNameAnsi = getAnsiStr(archive->getAppName(), CodePage::CP_UTF_8);
		// 텍스트 파트
		ImGui::Text(appNameAnsi.c_str());
		// --- 같은 라인 ---
		ImGui::SameLine(140.0f + ImGui::CalcTextSize(appNameAnsi.c_str()).x + states.buttonGap, 0.0f);
		// 버튼 파트
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);
		if (ImGui::Button(u8"편집")) {
			// 이름 복사 후 편집 모드로 전환
			memset(states.archiveGameName, 0, MAX_GAME_NAME_LENGTH);
			memcpy(states.archiveGameName, appNameAnsi.c_str(), min((MAX_GAME_NAME_LENGTH - 1), strlen(appNameAnsi.c_str())));
			states.isArchiveGameNameEditing = true;
		}
	}
	else {
		// 편집 모드, 전체적으로 y축 오프셋 조정
		string appNameAnsi = getAnsiStr(archive->getAppName(), CodePage::CP_UTF_8);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);
		// 입력 파트
		ImGui::PushItemWidth(400.0f);
		ImGui::InputText("##ArchiveGameName", states.archiveGameName, IM_ARRAYSIZE(states.archiveGameName));
		ImGui::PopItemWidth();
		// --- 같은 라인 ---
		ImGui::SameLine(140.0f + 400.0f + states.buttonGap, 0.0f);
		// 버튼 파트
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);
		// 확인 버튼 혹은 엔터 키를 누를때 전환한다
		// [게임 이름 길이가 없다면 비활성화]
		if (strlen(states.archiveGameName) == 0) {
			ImGui::BeginDisabled(true);
		}
		if (ImGui::Button(u8"확인") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
			// archive의 appName에 반영 후 디폴트 모드로 전환
			// 이렇게 되면 아직 저장이 안된 상태이므로 isDirty 를 true로 세팅한다
			wstring newAppName = getUniStr(states.archiveGameName, CodePage::CP_UTF_8);
			archive->setAppNameFromEditor(newAppName);
			states.isArchiveGameNameEditing = false;
			states.isDirty = true;
		}
		// [게임 이름 길이가 없다면 비활성화]
		if (strlen(states.archiveGameName) == 0) {
			ImGui::EndDisabled();
		}
	}

	// 클릭팀 퓨전 버전 정보 라인
	ImGui::Text(u8"버전:");
	// --- 같은 라인 ---
	ImGui::SameLine(140.0f, 0.0f);
	switch (archive->getEngineType()) 
	{
		case EngineType::ENGINE_NORMAL: 
		{
			ImGui::Text(u8"ClickteamFusion ver 2.0");
			break;
		}
		case EngineType::ENGINE_MMF_1_5:
		{
			ImGui::Text(u8"MuitimediaFusion ver 1.5");
			break;
		}
		case EngineType::ENGINE_MMF_2:
		{
			ImGui::Text(u8"ClickteamFusion ver 2.0 (none unicode)");
			break;
		}
		case EngineType::ENGINE_MMF_2_5_PLUS:
		{
			ImGui::Text(u8"ClickteamFusion ver 2.5+");
			break;
		}
	}

	// 로케일 정보 라인 (존재할 때만 구현. 한국어 로케일로 맞추는 기능 구현. y축 오프셋 보정)
	if (archive->isAppLocaleIdRead()) {
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3.0f);
		ImGui::Text(u8"로케일:");
		// --- 같은 라인 ---
		ImGui::SameLine(140.0f, 0.0f);
		string localeStr = to_string(archive->getAppLocaleId()).c_str();
		wchar_t localeStrBuffer[256];
		if (GetLocaleInfoW(LCID(archive->getAppLocaleId()), LOCALE_SLANGUAGE, localeStrBuffer, sizeof(localeStrBuffer))) {
			ostringstream oss;
			oss << left << setw(6) << setfill(' ') << archive->getAppLocaleId();
			oss << "(" + getAnsiStr(localeStrBuffer, CodePage::CP_UTF_8) + ")";
			localeStr = oss.str();
			oss.clear();
		}
		ImGui::Text(localeStr.c_str());
		// --- 같은 라인 ---
		ImGui::SameLine(140.0f + ImGui::CalcTextSize(localeStr.c_str()).x + states.buttonGap, 0.0f);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);
		if (ImGui::Button(u8"한국어 로케일로 변경")) {
			// 한국어 로케일로 변경 후 dirty 세팅
			archive->setAppLocaleIdFromEditor(1042);
			states.isDirty = true;
		}
	}

	// 코드페이지 정보 라인 (존재할 때만 구현. 한국어 코드페이지로 맞추는 기능 구현. y축 오프셋 보정)
	if (archive->isAppCodepageRead()) {
		ImGui::Text(u8"코드페이지:");
		// --- 같은 라인 ---
		ImGui::SameLine(140.0f, 0.0f);
		CPINFOEXW cpInfoEx;
		string codePageStr = to_string(archive->getAppCodepage()).c_str();
		if (GetCPInfoExW(archive->getAppCodepage(), 0, &cpInfoEx)) {
			codePageStr = getAnsiStr(cpInfoEx.CodePageName, CodePage::CP_UTF_8);
		}
		ImGui::Text(codePageStr.c_str());
		// --- 같은 라인 ---
		ImGui::SameLine(140.0f + ImGui::CalcTextSize(codePageStr.c_str()).x + states.buttonGap, 0.0f);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 3.0f);
		if (ImGui::Button(u8"한국어 코드페이지로 변경")) {
			// 한국어 코드페이지로 변경 후 dirty 세팅
			archive->setAppCodepageFromEditor(949);
			states.isDirty = true;
		}
	}

	ImGui::End();

}


void Dx9Application::renderTreeNodes(ChunkTreeNode *node, UINT32 depth, UINT32 index, bool isRoot)
{
	// --- 다음 줄, 다음 컬럼 ---
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_OpenOnArrow;
	ImGuiTreeNodeFlags leafNodeFlags = treeNodeFlags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen;

	// 루트 노드는 그냥 다 열린 상태로 세팅
	if (isRoot) {
		treeNodeFlags |= ImGuiTreeNodeFlags_DefaultOpen;
		leafNodeFlags |= ImGuiTreeNodeFlags_DefaultOpen;
	}

	// 선택여부에 따른 세팅
	if (node->selected()) {
		treeNodeFlags |= ImGuiTreeNodeFlags_Selected;
		leafNodeFlags |= ImGuiTreeNodeFlags_Selected;
	}

	string idStr = "##chunk-" + to_string(depth) + "-" + to_string(index);
	string nameStr = isRoot ? "__root__" : "chunk " + to_string(index);
	node->setIdStr(idStr);

	// 일반 노드 및 자식 노드 리스트 렌더
	if (node->getChildren().size() != 0) 
	{
		// 이름
		bool open = ImGui::TreeNodeEx(idStr.c_str(), treeNodeFlags, nameStr.c_str());
		// 클릭시 자식노드들도 부모의 선택결과에 맞춰서 세팅
		if (ImGui::IsItemClicked()) {
			// 선택된 노드가 없거나 클릭한 노드가 현재 선택된 노드가 아닐 때만 동작하도록 세팅한다
			if ((states.chunkTreeNodeContainer.getSelectedNode() == NULL)
				|| (node->getIdStr() != states.chunkTreeNodeContainer.getSelectedNode()->getIdStr())) {
				// 렌더링 중이던 노드가 있다면 해제시킨다
				// *** 부모 노드에는 어떻게 세팅해야 한다...
				if (states.renderingChunk != NULL && !states.isRenderingChunkAppInfo) {
					delete states.renderingChunk;
					states.renderingChunk = NULL;
				}
				// 선택 전환
				bool toggleSelect = !node->selected();
				// 하나만 선택되도록 세팅
				states.chunkTreeNodeContainer.getRootNodePointer()->setAllNodeSelected(false);
				node->setSelected(toggleSelect);
				if (toggleSelect) {
					states.chunkTreeNodeContainer.setSelectedNode(node);
					states.renderingChunkLoaded = false;
				}
				// 자식노드들도 똑같이 세팅
				for (size_t i = 0; i < node->getChildren().size(); i++) {
					node->getChildren()[i]->setSelected(toggleSelect);
				}
			}
		}
		// --- 다음 컬럼 ---
		ImGui::TableNextColumn();
		// 크기
		if (isRoot) {
			ImGui::TextDisabled("--");
		}
		else {
			ImGui::Text("%d", node->getValue().size);
		}
		// --- 다음 컬럼 ---
		ImGui::TableNextColumn();
		// 타입
		if (isRoot) {
			ImGui::TextDisabled("--");
		}
		else {
			ImGui::TextUnformatted(getNameAnsi(node->getValue().type));
		}
		// 트리를 연 상태면 루프로 자식노드 렌더
		if (open)
		{
			for (UINT32 i = 0; i < (UINT32)node->getChildren().size(); i++) {
				renderTreeNodes(node->getChildren()[i], (depth + 1), i, false);
			}
			ImGui::TreePop();
		}
	}
	// 리프 노드 렌더
	else 
	{
		// 이름
		ImGui::TreeNodeEx(idStr.c_str(), leafNodeFlags, nameStr.c_str());
		// 클릭시 자식노드만 선택전환
		if (ImGui::IsItemClicked()) {
			// 선택된 노드가 없거나 클릭한 노드가 현재 선택된 노드가 아닐 때만 동작하도록 세팅한다
			if ((states.chunkTreeNodeContainer.getSelectedNode() == NULL)
				|| (node->getIdStr() != states.chunkTreeNodeContainer.getSelectedNode()->getIdStr())) {
				// 렌더링 중이던 노드가 있다면 해제시킨다
				// *** 부모 노드에는 어떻게 세팅해야 한다...
				if (states.renderingChunk != NULL && !states.isRenderingChunkAppInfo) {
					delete states.renderingChunk;
					states.renderingChunk = NULL;
				}
				// 선택 전환
				bool toggleSelect = !node->selected();
				// 초기화 후 하나만 선택되도록 세팅
				states.chunkTreeNodeContainer.getRootNodePointer()->setAllNodeSelected(false);
				states.chunkTreeNodeContainer.setSelectedNode(NULL);
				node->setSelected(toggleSelect);
				if (toggleSelect) {
					states.chunkTreeNodeContainer.setSelectedNode(node);
					states.renderingChunkLoaded = false;
				}
			}
		}
		// --- 다음 컬럼 ---
		ImGui::TableNextColumn();
		// 크기
		if (isRoot) {
			ImGui::TextDisabled("--");
		}
		else {
			ImGui::Text("%d", node->getValue().size);
		}
		// --- 다음 컬럼 ---
		ImGui::TableNextColumn();
		// 타입
		if (isRoot) {
			ImGui::TextDisabled("--");
		}
		else {
			ImGui::TextUnformatted(getNameAnsi(node->getValue().type));
		}
	}
}


// 아카이브 Chunk 리스트 윈도우
void Dx9Application::renderArchiveChunkListWindow()
{
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;

	// 아카이브 정보 윈도우 바로 밑에 지정
	float xPos = 0.0f;
	float yPos = (float)states.menuHeight + (float)states.archiveOpenWindowHeight;
	ImVec2 windowSize;
	windowSize.x = (float)states.chunkListWindowWidth;
	windowSize.y = ImGui::GetIO().DisplaySize.y - yPos;
	ImGui::SetNextWindowPos(ImVec2(xPos, yPos), ImGuiCond_Always);
	ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

	if (!ImGui::Begin(u8"Chunk 리스트", NULL, windowFlags)) {
		ImGui::End();
		return;
	}

	ImGuiWindowFlags tableFlags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_ScrollY;
	ImVec2 tableSize = windowSize;
	float tabHeight = ImGui::GetTextLineHeight() + (ImGui::GetStyle().ItemSpacing.y * 2.0f);
	tableSize.x -= (ImGui::GetStyle().FramePadding.x * 2 + 8.0f);
	tableSize.y -= ((ImGui::GetStyle().FramePadding.y * 2) + tabHeight + 8.0f);
	
	if (ImGui::BeginTable("3ways", 3, tableFlags, tableSize))
	{
		// name / size / type 탭 세팅
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
		ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("0").x * 8.0f);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_None);
		ImGui::TableHeadersRow();
		// Node 리스트 렌더
		renderTreeNodes(states.chunkTreeNodeContainer.getRootNodePointer(), 0, 0, true);
		ImGui::EndTable();
	}

	ImGui::End();

}


// 아카이브의 각 Chunk 정보 윈도우
void Dx9Application::renderArchiveEachChunkInfoWindow()
{
	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;

	// 아카이브 정보 윈도우 바로 밑, chunk 리스트 윈도우 오른쪽에 지정
	float xPos = (float)states.chunkListWindowWidth;
	float yPos = (float)states.menuHeight + (float)states.archiveOpenWindowHeight;
	ImVec2 windowSize;
	windowSize.x = ImGui::GetIO().DisplaySize.x - xPos;
	windowSize.y = ImGui::GetIO().DisplaySize.y - yPos;
	ImGui::SetNextWindowPos(ImVec2(xPos, yPos), ImGuiCond_Always);
	ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
	
	if (!ImGui::Begin(u8"각 Chunk 정보", NULL, windowFlags)) {
		ImGui::End();
		return;
	}

	// 이쪽은 하나만 선택됐을때 그 정보만 렌더링한다
	ChunkTreeNode* selectedNode = states.chunkTreeNodeContainer.getSelectedNode();
	if (selectedNode != NULL)
	{
		// 필요 정보 확보
		ChunkInfo chunkInfo = selectedNode->getValue();
		UINT32 chunkOffset = selectedNode->getOffset();

		std::stringstream oss;

		// chunk 타입 라인
		ImGui::Text(u8"타입:");
		// --- 같은 라인 ---
		ImGui::SameLine(120.0f, 0.0f);
		ImGui::Text(getNameAnsi(chunkInfo.type));

		// chunk 크기 라인
		ImGui::Text(u8"크기:");
		// --- 같은 라인 ---
		ImGui::SameLine(120.0f, 0.0f);
		oss << dec << chunkInfo.size << " (0x" << hex << uppercase << chunkInfo.size << ")";
		ImGui::Text(oss.str().c_str());
		oss.str("");

		// chunk 오프셋 라인
		ImGui::Text(u8"오프셋:");
		// --- 같은 라인 ---
		ImGui::SameLine(120.0f, 0.0f);
		oss << dec << chunkOffset << " (0x" << hex << uppercase << chunkOffset << ")";
		ImGui::Text(oss.str().c_str());
		oss.str("");

		// chunk 압축/암호화 여부 라인
		ImGui::Text(u8"압축/암호화:");
		// --- 같은 라인 ---
		ImGui::SameLine(120.0f, 0.0f);
		if ((chunkInfo.flag & 0x1) && (chunkInfo.flag & 0x2)) {
			ImGui::Text(u8"[압축][암호화]");
		}
		else if (chunkInfo.flag & 0x1) {
			ImGui::Text(u8"[압축]");
		}
		else if (chunkInfo.flag & 0x2) {
			ImGui::Text(u8"[암호화]");
		}
		else {
			ImGui::TextDisabled(u8"--");
		}

		// 값 읽어들이기, DataChunk는 가진 데이터 수만 보여줘도 될거같음
		// 이미 읽은 값들은 또 읽어들일 필요가 없다
		if (!states.renderingChunkLoaded) {
			states.renderingChunk = allocateChunk(chunkInfo.type, states.isRenderingChunkAppInfo);

			// appData로 이미 읽어들였다면, 정보 동기화를 위해서 새로 읽지 않는다
			if (states.isRenderingChunkAppInfo) {

				// 셰이더 리스트는 DataChunk라서 따로 처리해 줘야 한다
				if (dynamic_cast<DataChunk*>(states.renderingChunk)) {
					DataChunk* dcPtr = reinterpret_cast<DataChunk*>(states.renderingChunk);
					states.chunkDataCount = (UINT32)dcPtr->getValue().size();
				}
				states.renderingChunk->renderData();

			}
			else {

				// 새로 읽어들이기 위해 핸들 오프셋 지정
				HANDLE hArchive = getArchive()->getArchiveHandle();
				SetFilePointer(hArchive, chunkOffset, NULL, FILE_BEGIN);

				// FrameLayerEffectDataChunk 라면 FrameLayerDataChunk를 선행하여 읽어들여야 처리가 가능하다
				// 이를 수행하려면 부모 chunk인 FrameChunk를 찾은 후, 거기서 FrameLayerDataChunk를 찾는 방식으로 가야 한다
				
				// 부모 노드(FrameChunk) 의 오프셋
				// states.chunkTreeNodeContainer.getSelectedNode()->getParent()->getOffset();
				// 실제 옮겨야 할 위치는 sizeof(ChunkInfo)를 더하여 세팅한다

				// 핸들을 이 위치로 옮기고, 내부에서 데이터를 옮겨 가며 chunkType이
				// ChunkType::CHUNK_FRAME_LAYERS 인 위치를 찾은 후 FrameLayerDataChunk를 읽어들여야만 한다
				// 위치를 찾는 건 Chunk 쪽에 선언해두는게 좋겠다.
				// 이후 여기서 개수만 얻고 FrameLayerDataChunk는 해제하고 땡친다

				// DataChunk 상속 여부에 따라 다르게 처리
				if (dynamic_cast<DataChunk*>(states.renderingChunk)) {

					// FrameLayerEffectDataChunk일 때는 별개로 FrameLayerDataChunk를 읽어서 개수를 지정하는 처리가 필요하다
					if (dynamic_cast<FrameLayerEffectDataChunk*>(states.renderingChunk)) {

						UINT32 parentChunkOffset = states.chunkTreeNodeContainer.getSelectedNode()->getParent()->getOffset();
						SetFilePointer(hArchive, parentChunkOffset + sizeof(ChunkInfo), NULL, FILE_BEGIN);
						moveArchiveHandleToChunk(hArchive, ChunkType::CHUNK_FRAME_LAYERS);
						
						FrameLayerDataChunk* tmpFrameLayerDataChunk = new FrameLayerDataChunk();
						tmpFrameLayerDataChunk->readFromArchive(hArchive);
						FrameLayerEffectDataChunk* fledcPtr = reinterpret_cast<FrameLayerEffectDataChunk*>(states.renderingChunk);
						fledcPtr->setFrameLayerCount((UINT32)tmpFrameLayerDataChunk->getValue().size());
						delete tmpFrameLayerDataChunk;

					}

					// ImageDataChunk, SoundDataChunk를 제외하면 크기가 크지 않으므로 그냥 읽어들인다
					if (dynamic_cast<ImageDataChunk*>(states.renderingChunk) || dynamic_cast<SoundDataChunk*>(states.renderingChunk)) {
						// DataChunk로 포인트 변환 후 읽기
						DataChunk* dcPtr = reinterpret_cast<DataChunk*>(states.renderingChunk);
						dcPtr->readOnlyDataCount(hArchive);
					}
					else {
						states.renderingChunk->readFromArchive(hArchive);
					}

				}
				else {
					states.renderingChunk->readFromArchive(hArchive);
				}

			}
			states.renderingChunkLoaded = true;
		}

		// 각 chunk의 value 라인
		ImGui::Text(u8"데이터:");
		ImGui::PushTextWrapPos(states.windowWidth - states.chunkListWindowWidth - ImGui::GetStyle().FramePadding.x);
		if (states.renderingChunk != NULL) {
			states.renderingChunk->renderData();
		}
		ImGui::PopTextWrapPos();
	}

	ImGui::End();

}


// ------------------- END [윈도우] ---------------------