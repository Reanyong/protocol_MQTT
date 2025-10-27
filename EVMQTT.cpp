
#include "pch.h"
#include "framework.h"
#include "EVMQTT.h"
#include "EVMQTTDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CEVMQTTApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()

// CEVMQTTApp 생성

CEVMQTTApp::CEVMQTTApp()
{
	// 다시 시작 관리자 지원
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// TODO: 여기에 생성 코드를 추가합니다.
	// InitInstance에 모든 중요한 초기화 작업을 배치합니다.
	m_hMutex = NULL;
}


// 유일한 CEVMQTTApp 개체입니다.

CEVMQTTApp theApp;

// ===== EasyView 프로젝트 이름 전역 변수 =====
CString g_szProjectName;


// CEVMQTTApp 초기화

BOOL CEVMQTTApp::InitInstance()
{
	// ===== EasyView 엔진에서 전달된 프로젝트 이름 파싱 =====
	// 예: EVMQTT_D.exe T_MQTT:engine
	CString cmdLine = m_lpCmdLine;
	cmdLine.TrimLeft();
	cmdLine.TrimRight();

	if (!cmdLine.IsEmpty())
	{
		int colonPos = cmdLine.Find(_T(':'));
		if (colonPos > 0)
		{
			// "T_MQTT:engine" → "T_MQTT"
			g_szProjectName = cmdLine.Left(colonPos);
		}
		else
		{
			// 콜론이 없으면 전체를 프로젝트 이름으로
			g_szProjectName = cmdLine;
		}
		g_szProjectName.MakeUpper();
		TRACE("Command line project name: %s\n", (LPCTSTR)g_szProjectName);
	}
	else
	{
		// 커맨드 라인이 없으면 INI 파일에서 읽기
		char szBuff[256] = { 0 };
		char szProjectName[256] = { 0 };
		EV_GetConfigFile(szBuff);
		::GetPrivateProfileString(
			"EasyView", "Project", "", szProjectName,
			sizeof(szProjectName), szBuff
		);
		g_szProjectName = CString(szProjectName);
		g_szProjectName.MakeUpper();
		TRACE("INI file project name: %s\n", (LPCTSTR)g_szProjectName);
	}

	// 중복 실행 방지 (실행 파일명 기반)
	// EVMQTT1.exe, EVMQTT2.exe, EVMQTT3.exe, EVMQTT4.exe는 각각 독립 실행 가능
	// 하지만 같은 이름끼리는 중복 실행 불가
	TCHAR szModulePath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, szModulePath, MAX_PATH);
	
	CString strModulePath(szModulePath);
	int nPos = strModulePath.ReverseFind(_T('\\'));
	CString exeName = _T("EVMQTT");  // 기본값
	
	if (nPos > 0) {
		exeName = strModulePath.Mid(nPos + 1);
		int dotPos = exeName.ReverseFind(_T('.'));
		if (dotPos > 0) {
			exeName = exeName.Left(dotPos);  // 확장자 제거
		}
	}
	
	// 실행 파일명 기반 Mutex 이름 생성
	// 예: "EVMQTT1_SINGLE_INSTANCE_MUTEX", "EVMQTT2_SINGLE_INSTANCE_MUTEX"
	CString mutexName;
	mutexName.Format(_T("%s_SINGLE_INSTANCE_MUTEX"), exeName);
	
	m_hMutex = CreateMutex(NULL, TRUE, mutexName);
	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		if (m_hMutex)
		{
			CloseHandle(m_hMutex);
			m_hMutex = NULL;
		}
		
		CString errorMsg;
		errorMsg.Format(_T("%s 프로그램이 이미 실행되어 있습니다."), exeName);
		AfxMessageBox(errorMsg, MB_OK | MB_ICONWARNING);
		return FALSE;
	}

	// 애플리케이션 매니페스트가 ComCtl32.dll 버전 6 이상을 사용하여 비주얼 스타일을
	// 사용하도록 지정하는 경우, Windows XP 상에서 반드시 InitCommonControlsEx()가 필요합니다.
	// InitCommonControlsEx()를 사용하지 않으면 창을 만들 수 없습니다.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// 응용 프로그램에서 사용할 모든 공용 컨트롤 클래스를 포함하도록
	// 이 항목을 설정하십시오.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();


	AfxEnableControlContainer();

	// 대화 상자에 셸 트리 뷰 또는
	// 셸 목록 뷰 컨트롤이 포함되어 있는 경우 셸 관리자를 만듭니다.
	CShellManager *pShellManager = new CShellManager;

	// MFC 컨트롤의 테마를 사용하기 위해 "Windows 원형" 비주얼 관리자 활성화
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	// 표준 초기화
	// 이들 기능을 사용하지 않고 최종 실행 파일의 크기를 줄이려면
	// 아래에서 필요 없는 특정 초기화
	// 루틴을 제거해야 합니다.
	// 해당 설정이 저장된 레지스트리 키를 변경하십시오.
	// TODO: 이 문자열을 회사 또는 조직의 이름과 같은
	// 적절한 내용으로 수정해야 합니다.
	SetRegistryKey(_T("로컬 애플리케이션 마법사에서 생성된 애플리케이션"));

	char szCurPath[512];
	GetCurrentDirectory(sizeof(szCurPath), szCurPath);

	CEVMQTTDlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: 여기에 [확인]을 클릭하여 대화 상자가 없어질 때 처리할
		//  코드를 배치합니다.
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: 여기에 [취소]를 클릭하여 대화 상자가 없어질 때 처리할
		//  코드를 배치합니다.
	}
	else if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "경고: 대화 상자를 만들지 못했으므로 애플리케이션이 예기치 않게 종료됩니다.\n");
		TRACE(traceAppMsg, 0, "경고: 대화 상자에서 MFC 컨트롤을 사용하는 경우 #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS를 수행할 수 없습니다.\n");
	}

	// 위에서 만든 셸 관리자를 삭제합니다.
	if (pShellManager != nullptr)
	{
		delete pShellManager;
	}

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
	ControlBarCleanUp();
#endif

	// 대화 상자가 닫혔으므로 응용 프로그램의 메시지 펌프를 시작하지 않고 응용 프로그램을 끝낼 수 있도록 FALSE를
	// 반환합니다.
	return FALSE;
}

int CEVMQTTApp::ExitInstance()
{
	// Mutex 해제
	if (m_hMutex)
	{
		CloseHandle(m_hMutex);
		m_hMutex = NULL;
	}

	return CWinApp::ExitInstance();
}
