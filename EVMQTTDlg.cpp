﻿#include "pch.h"
#include "framework.h"
#include "EVMQTT.h"
#include "EVMQTTDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 응용 프로그램 정보에 사용되는 CAboutDlg 대화 상자입니다.

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

// 구현입니다.
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

CEVMQTTDlg::CEVMQTTDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_EVMQTT_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	m_pThreadSub = NULL;

	m_showInfoLogs = false;
	m_showSuccessLogs = false;
	m_showWarningLogs = true;
	m_showErrorLogs = true;
}

void CEVMQTTDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STATIC_STATS, m_staticStats);
	DDX_Control(pDX, IDC_LIST_DEBUG, m_listDebug);
}

BEGIN_MESSAGE_MAP(CEVMQTTDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_SUB, &CEVMQTTDlg::OnBnClickedBtnSub)
	ON_BN_CLICKED(IDOK, &CEVMQTTDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CEVMQTTDlg::OnBnClickedCancel)
	ON_MESSAGE(WM_USER + 100, OnUpdateStats)
	//ON_BN_CLICKED(IDC_BUTTON_CLEAR_LOG, &CEVMQTTDlg::OnBnClickedButtonClearLog)
	ON_MESSAGE(WM_USER + 101, OnUpdateDebugLog)
END_MESSAGE_MAP()


// CEVMQTTDlg 메시지 처리기

BOOL CEVMQTTDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 시스템 메뉴에 "정보..." 메뉴 항목을 추가합니다.

	// IDM_ABOUTBOX는 시스템 명령 범위에 있어야 합니다.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 이 대화 상자의 아이콘을 설정합니다.  응용 프로그램의 주 창이 대화 상자가 아닐 경우에는
	//  프레임워크가 이 작업을 자동으로 수행합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.

	m_nParsedCount = 0;
	m_nTotalCount = 0;
	UpdateParsingStats(0, 0);

	SetDebugLogFilter(false, false, true, true);

	InitDebugList();

	AddDebugLog(_T("프로그램 시작"), _T(""), DebugLogItem::LOG_INFO);

	// TODO: 여기에 추가 초기화 작업을 추가합니다.

	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

void CEVMQTTDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 대화 상자에 최소화 단추를 추가할 경우 아이콘을 그리려면
//  아래 코드가 필요합니다.  문서/뷰 모델을 사용하는 MFC 애플리케이션의 경우에는
//  프레임워크에서 이 작업을 자동으로 수행합니다.

void CEVMQTTDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 그리기를 위한 디바이스 컨텍스트입니다.

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 클라이언트 사각형에서 아이콘을 가운데에 맞춥니다.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 아이콘을 그립니다.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// 사용자가 최소화된 창을 끄는 동안에 커서가 표시되도록 시스템에서
//  이 함수를 호출합니다.
HCURSOR CEVMQTTDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void PeekMessages()
{
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		//    if(!IsDialogMessage(&msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

void CEVMQTTDlg::OnBnClickedBtnSub()
{
	CWnd* pBtn = GetDlgItem(IDC_BTN_SUB);
	if (m_pThreadSub == NULL)
	{
		// 스레드가 없는 상태 - 시작
		BeginThreadSub();
		// 버튼 텍스트를 '종료'로 변경
		pBtn->SetWindowText(_T("통신 종료"));
	}
	else
	{
		// 스레드가 실행 중인 상태 - 종료
		StopThreadSub();
		DeleteThreadSub();
		// 버튼 텍스트를 '시작'으로 변경
		pBtn->SetWindowText(_T("통신 시작"));
	}
}

void CEVMQTTDlg::OnBnClickedOk()
{
	StopThreadSub();
	DeleteThreadSub();

	CDialogEx::OnOK();
}

void CEVMQTTDlg::OnBnClickedCancel()
{
	StopThreadSub();
	DeleteThreadSub();

	CDialogEx::OnCancel();
}

void CEVMQTTDlg::BeginThreadSub()
{
	if (m_pThreadSub == NULL)
	{
		m_nParsedCount = 0;
		m_nTotalCount = 0;
		UpdateParsingStats(0, 0);

		// 스레드 생성
		m_pThreadSub = (CThreadSub*)AfxBeginThread(RUNTIME_CLASS(CThreadSub),
			THREAD_PRIORITY_HIGHEST, 0, CREATE_SUSPENDED);
		m_pThreadSub->m_pOwner = this;

		// 스레드 시작 전 초기화 작업 실행
		m_pThreadSub->InitializeFileProcessing();

		m_nTotalCount = m_pThreadSub->m_nTotalCount;
		UpdateParsingStats(m_nParsedCount, m_nTotalCount);

		// 스레드 시작
		m_pThreadSub->ResumeThread();

		// 로그 추가
		AddDebugLog(_T("통신 시작"), _T(""), DebugLogItem::LOG_INFO);
	}
}

void CEVMQTTDlg::StopThreadSub()
{
	if (m_pThreadSub != NULL)
	{
#ifndef _DEBUG
		try
		{
#endif
			// 종료 메시지 전송
			PostThreadMessage(m_pThreadSub->m_nThreadID, WM_QUIT, 0, 0);
			m_pThreadSub->Stop();

			// 로그 추가
			AddDebugLog(_T("통신 종료"), _T(""), DebugLogItem::LOG_INFO);
#ifndef _DEBUG
		}
		catch (...)
		{
			ASSERT(FALSE);
		}
#endif
	}
}

void CEVMQTTDlg::DeleteThreadSub()
{
	if (m_pThreadSub != NULL)
	{
#ifndef _DEBUG
		try
		{
#endif
			int n = 0;
			DWORD dwExitCode;
			m_pThreadSub->Stop();
			while (true)
			{
				if (GetExitCodeThread(m_pThreadSub->m_hThread, &dwExitCode))
				{
					if (dwExitCode != STILL_ACTIVE)
						break;
				}
				else break;
				Sleep(10);

				n++;

				if (n > 500)
					break;
			}

			delete m_pThreadSub;
			m_pThreadSub = NULL;
#ifndef _DEBUG
		}
		catch (...)
		{
			ASSERT(FALSE);
		}
#endif
	}
}

// 통계 업데이트 메서드 구현
void CEVMQTTDlg::UpdateParsingStats(int parsedCount, int totalCount)
{
	m_nParsedCount = parsedCount;
	m_nTotalCount = totalCount;

	TRACE("UpdateParsingStats: m_nParsedCount=%d, m_nTotalCount=%d\n", m_nParsedCount, m_nTotalCount);

	CString statsText;
	statsText.Format(_T("json 파싱 결과: %ld / %ld"), (long)m_nParsedCount, (long)m_nTotalCount);
	// 또는 다른 방식으로 시도:
	// CString statsText;
	// statsText.Format(_T("json 파싱 결과: %d / %d"), (int)m_nParsedCount, (int)m_nTotalCount);

	TRACE("statsText: %s\n", (LPCTSTR)statsText);

	// 컨트롤이 있는지 확인하고 텍스트 설정
	if (::IsWindow(m_staticStats.GetSafeHwnd()))
	{
		m_staticStats.SetWindowText(statsText);

		// 강제 갱신 시도
		m_staticStats.Invalidate();
		m_staticStats.UpdateWindow();
	}
	else
	{
		TRACE("m_staticStats is not a valid window!\n");
	}
}

// 메시지 핸들러 구현
LRESULT CEVMQTTDlg::OnUpdateStats(WPARAM wParam, LPARAM lParam)
{
	int parsedCount = (int)wParam;
	int totalCount = (int)lParam;

	TRACE("OnUpdateStats: parsedCount=%d, totalCount=%d\n", parsedCount, totalCount);

	if (m_nParsedCount != parsedCount || m_nTotalCount != totalCount) {
		UpdateParsingStats(parsedCount, totalCount);
	}
	return 0;
}

// 디버그 리스트 초기화
void CEVMQTTDlg::InitDebugList()
{
	if (!::IsWindow(m_listDebug.GetSafeHwnd())) {
		TRACE("리스트 컨트롤이 유효하지 않습니다.\n");
		return;
	}

	while (m_listDebug.DeleteColumn(0));

	DWORD dwStyle = m_listDebug.GetExtendedStyle();
	m_listDebug.SetExtendedStyle(dwStyle | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

	// 컬럼 추가
	m_listDebug.InsertColumn(0, _T("파일명"), LVCFMT_LEFT, 150);
	m_listDebug.InsertColumn(1, _T("상태"), LVCFMT_CENTER, 60);
	m_listDebug.InsertColumn(2, _T("시간"), LVCFMT_LEFT, 70);
	m_listDebug.InsertColumn(3, _T("메시지"), LVCFMT_LEFT, 300);

	OutputDebugString(_T("디버그 리스트 초기화 완료\n"));
}

// 디버그 로그 추가
void CEVMQTTDlg::AddDebugLog(const CString& message, const CString& filePath, DebugLogItem::LogType type)
{
	// 현재 필터 설정에 따라 표시 여부 결정
	bool shouldDisplay = false;
	switch (type) {
	case DebugLogItem::LOG_INFO:
		shouldDisplay = m_showInfoLogs;
		break;
	case DebugLogItem::LOG_SUCCESS:
		shouldDisplay = m_showSuccessLogs;
		break;
	case DebugLogItem::LOG_WARNING:
		shouldDisplay = m_showWarningLogs;
		break;
	case DebugLogItem::LOG_ERROR:
		shouldDisplay = m_showErrorLogs;
		break;
	}

	// 표시하지 않을 로그는 저장하지 않음
	if (!shouldDisplay) {
		return;
	}

	CSingleLock lock(&m_logMutex, TRUE);
	if (!lock.IsLocked()) {
		OutputDebugString(_T("로그 추가 중 뮤텍스 잠금 실패\n"));
		return;
	}

	DebugLogItem logItem;
	logItem.message = message;
	logItem.filePath = filePath;
	logItem.type = type;
	logItem.timestamp = CTime::GetCurrentTime();
	logItem.shouldDisplay = shouldDisplay;

	m_debugLogs.push_back(logItem);

	OutputDebugString(_T("로그 추가됨: "));
	OutputDebugString(message);
	OutputDebugString(_T("\n"));

	// UI 업데이트 메시지 전송
	PostMessage(WM_USER + 101, 0, 0);
}

// 디버그 리스트 업데이트
void CEVMQTTDlg::UpdateDebugList()
{
	if (!::IsWindow(m_listDebug.GetSafeHwnd())) {
		TRACE("리스트 컨트롤이 유효하지 않습니다.\n");
		return;
	}

	CSingleLock lock(&m_logMutex, TRUE);
	if (!lock.IsLocked()) {
		TRACE("뮤텍스 잠금 실패\n");
		return;
	}

	m_listDebug.DeleteAllItems();

	// 필터링된 로그만 표시
	int itemCount = 0;
	for (size_t i = 0; i < m_debugLogs.size(); i++) {
		const DebugLogItem& logItem = m_debugLogs[i];

		// 표시 설정된 항목만 추가
		if (!logItem.shouldDisplay) {
			continue;
		}

		// 파일명 추출 (경로에서)
		CString fileName = logItem.filePath;
		if (!fileName.IsEmpty()) {
			int pos = fileName.ReverseFind('\\');
			if (pos >= 0)
				fileName = fileName.Mid(pos + 1);
		}

		// 상태 텍스트 및 색상
		CString strStatus;
		COLORREF textColor = RGB(0, 0, 0);

		switch (logItem.type) {
		case DebugLogItem::LOG_INFO:
			strStatus = _T("정보");
			textColor = RGB(0, 0, 0); // 검은색
			break;
		case DebugLogItem::LOG_SUCCESS:
			strStatus = _T("성공");
			textColor = RGB(0, 128, 0); // 녹색
			break;
		case DebugLogItem::LOG_WARNING:
			strStatus = _T("경고");
			textColor = RGB(255, 128, 0); // 주황색
			break;
		case DebugLogItem::LOG_ERROR:
			strStatus = _T("오류");
			textColor = RGB(255, 0, 0); // 빨간색
			break;
		}

		// 시간 포맷팅
		CString strTime = logItem.timestamp.Format(_T("%H:%M:%S"));

		// 항목 추가
		int nItem = m_listDebug.InsertItem(itemCount++, fileName);
		if (nItem >= 0) {
			m_listDebug.SetItemText(nItem, 1, strStatus);
			m_listDebug.SetItemText(nItem, 2, strTime);
			m_listDebug.SetItemText(nItem, 3, logItem.message);

			// 맨 아래로 스크롤
			m_listDebug.EnsureVisible(nItem, FALSE);
		}
	}

	// 화면 갱신 요청
	m_listDebug.Invalidate();
	m_listDebug.UpdateWindow();
}

// 로그 지우기 버튼 핸들러
void CEVMQTTDlg::OnBnClickedButtonClearLog()
{
	//std::lock_guard<std::mutex> lock(m_logMutex);
	CSingleLock lock(&m_logMutex, TRUE);

	m_debugLogs.clear();
	m_listDebug.DeleteAllItems();
}

// 디버그 로그 업데이트 메시지 핸들러
LRESULT CEVMQTTDlg::OnUpdateDebugLog(WPARAM wParam, LPARAM lParam)
{
	static int callCount = 0;
	callCount++;

	CString dbgMsg;
	dbgMsg.Format(_T("OnUpdateDebugLog 호출 횟수: %d\n"), callCount);
	OutputDebugString(dbgMsg);

	UpdateDebugList();
	return 0;
}

// 디버그 로그 필터 설정 메소드
void CEVMQTTDlg::SetDebugLogFilter(bool showInfo, bool showSuccess, bool showWarning, bool showError)
{
	CSingleLock lock(&m_logMutex, TRUE);
	if (lock.IsLocked()) {
		m_showInfoLogs = showInfo;
		m_showSuccessLogs = showSuccess;
		m_showWarningLogs = showWarning;
		m_showErrorLogs = showError;

		// 필터 변경 시 리스트 업데이트
		PostMessage(WM_USER + 101, 0, 0);
	}
}
