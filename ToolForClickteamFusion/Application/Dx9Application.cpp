#include "Dx9Application.h"
#include "../Util/TextureUtil.h"

using namespace CTT::App;
using namespace CTT::Archive;
using namespace CTT::TextureUtil;


Dx9Application::Dx9Application(wstring appName) 
	: Application(appName)
{
	hwnd = NULL;
	wc = {};
	io = NULL;
	pD3d = NULL;
	pD3dDevice = NULL;
	isDeviceLost = false;
	d3dPp = {};
}


Dx9Application::~Dx9Application()
{
}


bool Dx9Application::init()
{
	HMODULE hModule = GetModuleHandleW(NULL);

	// 윈도우 화면 생성
	wc.cbSize = sizeof(wc);
	wc.style = CS_CLASSDC;
	wc.lpfnWndProc = getApp()->WndProc;
	wc.cbClsExtra = 0L;
	wc.cbWndExtra = 0L;
	wc.hInstance = hModule;
	wc.hIcon = NULL;
	wc.hCursor = NULL;
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = L"ImGui_Dx9_Application";
	wc.hIconSm = NULL;
	::RegisterClassExW(&wc);

	// app 상태 초기화
	initializeAppState();

	// 화면 크기 가져오기
	RECT desktop;
	GetWindowRect(GetDesktopWindow(), &desktop);
	int screenWidth = desktop.right;
	int screenHeight = desktop.bottom;

	// 윈도우 위치 계산
	int x = (screenWidth - states.windowWidth) / 2;
	int y = (screenHeight - states.windowHeight) / 2;

	// 중앙 위치로, 메뉴 없앰(모달 띄울때 위치 어긋남), 화면 크기 조절 불가하게
	hwnd = ::CreateWindowW(
		wc.lpszClassName,
		name.c_str(),
		WS_POPUP,
		x, y,
		states.windowWidth,
		states.windowHeight,
		NULL,
		NULL,
		wc.hInstance,
		NULL
	);

	// Direct3D 초기화
	if (!createDeviceD3D(hwnd))
	{
		cleanupDeviceD3D();
		::UnregisterClassW(wc.lpszClassName, wc.hInstance);
		wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": DirectX 초기화에 실패했습니다." << endl;
		return false;
	}

	// 윈도우 화면 띄우기
	::ShowWindow(hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(hwnd);

	// ImGui 컨텍스트 초기화
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	io = &(ImGui::GetIO());
	io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;	 // Enable Keyboard Controls
	io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;	  // Enable Gamepad Controls

	// 다크 스타일
	ImGui::StyleColorsDark();

	// 한국어 / 일본어 폰트 세팅, 나머지 언어는 알아서 추가하던가 하면 될듯
	// 실행 파일 옆의 GUI_Font.ttf 를 먼저 쓰고, 없으면 윈도우 기본 한글 폰트(맑은 고딕)로 대신한다
	float fontSize = 14.0f;
	{
		auto toUtf8 = [](const wstring& w) {
			if (w.empty()) { return string(); }
			int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
			string s(n, '\0');
			WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, NULL, NULL);
			return s;
		};
		wchar_t modulePath[MAX_PATH] = {};
		GetModuleFileNameW(NULL, modulePath, MAX_PATH);
		wstring exeDir = modulePath;
		const size_t slash = exeDir.find_last_of(L"\\/");
		exeDir = (slash == wstring::npos) ? L"." : exeDir.substr(0, slash);
		wchar_t winDir[MAX_PATH] = {};
		GetWindowsDirectoryW(winDir, MAX_PATH);

		const wstring candidates[] = {
			exeDir + L"\\GUI_Font.ttf",
			L"GUI_Font.ttf",
			wstring(winDir) + L"\\Fonts\\malgun.ttf",
		};
		ImFont* guiFont = NULL;
		for (const wstring& candidate : candidates) {
			if (GetFileAttributesW(candidate.c_str()) == INVALID_FILE_ATTRIBUTES) {
				continue;
			}
			guiFont = io->Fonts->AddFontFromFileTTF(toUtf8(candidate).c_str(), fontSize, NULL, io->Fonts->GetGlyphRangesKoreanAndJapanese());
			if (guiFont != NULL) {
				break;
			}
		}
		if (guiFont == NULL) {
			wcout << errPrefix << L"<<function>> " << __FUNCTIONW__ << L": 화면 글꼴(GUI_Font.ttf, malgun.ttf)을 찾지 못해 기본 글꼴을 씁니다. 한글이 ?로 보일 수 있습니다." << endl;
		}
	}

	// ini 파일 생성 방지
	io->IniFilename = NULL;

	// 플랫폼/렌더러 세팅
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX9_Init(pD3dDevice);

	return true;
}


void Dx9Application::run()
{
	bool done = false;

	// 윈도우 배경색은 지정된 대로 세팅
	ImVec4 bgColor = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];

	// 윈도우가 포커스를 잃었을 때도 타이틀 탭 색은 동일하게
	ImGui::GetStyle().Colors[ImGuiCol_TitleBg] = ImGui::GetStyle().Colors[ImGuiCol_TitleBgActive];

	while (!done)
	{
		// Poll and handle messages (inputs, window resize, etc.)
		// See the WndProc() function below for our to dispatch events to the Win32 backend.
		MSG msg;
		while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			if (msg.message == WM_QUIT) { done = true; }
		}
		if (done) { break; }

		// Handle lost D3D9 device
		if (isDeviceLost)
		{
			HRESULT hr = pD3dDevice->TestCooperativeLevel();
			if (hr == D3DERR_DEVICELOST)
			{
				::Sleep(10);
				continue;
			}
			if (hr == D3DERR_DEVICENOTRESET) { resetDevice(); }
			isDeviceLost = false;
		}

		// WM_SIZE 이벤트로 리사이즈하지 않고 여기서 직접 리사이즈 처리
		if (resizeWidth != 0 && resizeHeight != 0 && (d3dPp.BackBufferWidth != resizeWidth) && (d3dPp.BackBufferHeight != resizeHeight))
		{
			states.windowWidth = d3dPp.BackBufferWidth = resizeWidth;
			states.windowHeight = d3dPp.BackBufferHeight = resizeHeight;
			resizeWidth = resizeHeight = 0;
			resetDevice();
		}

		// ImGui 프레임 초기화
		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// ** 실제 기작들 렌더링 **
		render();

		// ImGui 렌더링
		ImGui::EndFrame();

		// DirectX 9 세팅
		pD3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
		pD3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		pD3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

		D3DCOLOR clear_col_dx = 
			D3DCOLOR_RGBA((int)(bgColor.x * bgColor.w * 255.0f), (int)(bgColor.y * bgColor.w * 255.0f), (int)(bgColor.z * bgColor.w * 255.0f), (int)(bgColor.w * 255.0f));
		pD3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);

		if (pD3dDevice->BeginScene() >= 0)
		{
			ImGui::Render();
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
			pD3dDevice->EndScene();
		}
		HRESULT result = pD3dDevice->Present(NULL, NULL, NULL, NULL);
		if (result == D3DERR_DEVICELOST) { isDeviceLost = true; }

	}
}


void Dx9Application::terminate()
{
	// 각종 프로세스 정리
	terminateProcess();

	// 상태 정리
	states.chunkTreeNodeContainer.release();

	// 컨테이너 정리는 알아서 소멸할테니 할 필요 없음

	// ImGUI 정리
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	// Direct3D 정리
	cleanupDeviceD3D();
	::DestroyWindow(hwnd);
	::UnregisterClassW(wc.lpszClassName, wc.hInstance);
}




bool Dx9Application::createDeviceD3D(HWND hWnd)
{
	if ((pD3d = Direct3DCreate9(D3D_SDK_VERSION)) == NULL) { return false; }

	// D3DDevice 생성
	memset(&d3dPp, 0, sizeof(D3DPRESENT_PARAMETERS));
	d3dPp.Windowed = TRUE;
	d3dPp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dPp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
	d3dPp.EnableAutoDepthStencil = TRUE;
	d3dPp.AutoDepthStencilFormat = D3DFMT_D16;
	d3dPp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;		   // Present with vsync
	//d3dPp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;   // Present without vsync, maximum unthrottled framerate
	if (pD3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dPp, &pD3dDevice) < 0) {
		return false;
	}

	return true;
}


void Dx9Application::cleanupDeviceD3D()
{
	if (pD3dDevice) { pD3dDevice->Release(); pD3dDevice = NULL; }
	if (pD3d) { pD3d->Release(); pD3d = NULL; }
}


void Dx9Application::resetDevice()
{
	ImGui_ImplDX9_InvalidateDeviceObjects();
	HRESULT hr = pD3dDevice->Reset(&d3dPp);
	if (hr == D3DERR_INVALIDCALL) { IM_ASSERT(0); }
	ImGui_ImplDX9_CreateDeviceObjects();
}