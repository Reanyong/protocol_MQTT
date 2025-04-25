#include "pch.h"
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
}

void CEVMQTTDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STATIC_STATS, m_staticStats);
}

BEGIN_MESSAGE_MAP(CEVMQTTDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_SUB, &CEVMQTTDlg::OnBnClickedBtnSub)
	ON_BN_CLICKED(IDOK, &CEVMQTTDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CEVMQTTDlg::OnBnClickedCancel)
	ON_MESSAGE(WM_USER + 100, OnUpdateStats)
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
		m_pThreadSub = (CThreadSub*)AfxBeginThread(RUNTIME_CLASS(CThreadSub), THREAD_PRIORITY_HIGHEST, 0, CREATE_SUSPENDED);
		m_pThreadSub->m_pOwner = this;
		m_pThreadSub->ResumeThread();
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
			PostThreadMessage(m_pThreadSub->m_nThreadID, WM_QUIT, 0, 0);
			m_pThreadSub->Stop();
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

	CString statsText;
	statsText.Format(_T("json 파싱 결과: %d / %d"), m_nParsedCount, m_nTotalCount);
	m_staticStats.SetWindowText(statsText);
}

// 메시지 핸들러 구현
LRESULT CEVMQTTDlg::OnUpdateStats(WPARAM wParam, LPARAM lParam)
{
	int parsedCount = (int)wParam;
	int totalCount = (int)lParam;
	UpdateParsingStats(parsedCount, totalCount);
	return 0;
}
