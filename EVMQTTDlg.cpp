#include "pch.h"
#include "framework.h"
#include "EVMQTT.h"
#include "EVMQTTDlg.h"
#include "ConfigDlg.h"
#include "afxdialogex.h"
#include "ConfigManager.h"
#include "LogManager.h"

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

	// 상태 정보 초기화
	m_bMqttConnected = false;
	m_strMqttHost = _T("");
	m_nMqttPort = 0;
	m_nActiveTagCount = 0;
	m_nTotalTagCount = 0;
	m_nMessagesPerSec = 0;
	m_nSuccessRate = 0;
	m_dwLastUpdateTime = GetTickCount();
	m_nLastProcessedCount = 0;

	// 최적화 변수 초기화
	m_lastActivityUpdate = 0;
	m_needActivityRefresh = false;
}

void CEVMQTTDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STC_MQTT_STATUS, m_staticMqttStatus);
	DDX_Control(pDX, IDC_STC_TAG_INFO, m_staticTagInfo);
	DDX_Control(pDX, IDC_STC_PERFORMANCE, m_staticPerformance);
	DDX_Control(pDX, IDC_STC_ACTIVITY_LABEL, m_staticActivityLabel);
	DDX_Control(pDX, IDC_LIST_ACTIVITY, m_listActivity);
}

BEGIN_MESSAGE_MAP(CEVMQTTDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_SUB, &CEVMQTTDlg::OnBnClickedBtnSub)
	ON_BN_CLICKED(IDOK, &CEVMQTTDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CEVMQTTDlg::OnBnClickedCancel)
	ON_MESSAGE(WM_USER + 100, OnUpdateStats)
	ON_MESSAGE(WM_USER + 102, OnUpdateActivityLog)
	ON_BN_CLICKED(IDC_BTN_CONFIG, &CEVMQTTDlg::OnBnClickedBtnConfig)
	ON_BN_CLICKED(IDC_BTN_VIEW_LOG, &CEVMQTTDlg::OnBnClickedBtnViewLog)
END_MESSAGE_MAP()

// CEVMQTTDlg 메시지 처리기

BOOL CEVMQTTDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 시스템 메뉴에 "정보..." 메뉴 항목을 추가합니다.
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

	// 이 대화 상자의 아이콘을 설정합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.

	// 새로운 UI 초기화
	InitStatusControls();
	InitActivityList();

	// 초기 상태 설정
	CConfigManager& configManager = CConfigManager::GetInstance();
	configManager.LoadConfig();

	UpdateMqttStatus(false, configManager.GetMqttIp(), configManager.GetMqttPort());
	UpdateTagInfo(0, configManager.GetAllTagMappings().size());
	UpdatePerformance(0, 0);

	AddActivityLog(_T("시스템"), _T("프로그램 시작"), ActivityLogItem::LOG_INFO, _T("준비"));

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

HCURSOR CEVMQTTDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
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
		// 스레드 생성
		m_pThreadSub = (CThreadSub*)AfxBeginThread(RUNTIME_CLASS(CThreadSub),
			THREAD_PRIORITY_HIGHEST, 0, CREATE_SUSPENDED);
		m_pThreadSub->m_pOwner = this;

		// 스레드 시작
		m_pThreadSub->ResumeThread();

		// 연결 상태 업데이트
		CConfigManager& configManager = CConfigManager::GetInstance();
		UpdateMqttStatus(true, configManager.GetMqttIp(), configManager.GetMqttPort());
		AddActivityLog(_T("MQTT"), _T("연결 시도"), ActivityLogItem::LOG_CONNECTION, _T("진행중"));
	}
}

void CEVMQTTDlg::StopThreadSub()
{
	if (m_pThreadSub != NULL)
	{
		try
		{
			PostThreadMessage(m_pThreadSub->m_nThreadID, WM_QUIT, 0, 0);
			m_pThreadSub->Stop();

			UpdateMqttStatus(false, m_strMqttHost, m_nMqttPort);
			AddActivityLog(_T("MQTT"), _T("연결 종료"), ActivityLogItem::LOG_CONNECTION, _T("종료"));
		}
		catch (...)
		{
			ASSERT(FALSE);
		}
	}
}

void CEVMQTTDlg::DeleteThreadSub()
{
	if (m_pThreadSub != NULL)
	{
		try
		{
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

				int sleepTime = (n < 10) ? 1 : (n < 50) ? 5 : 10;
				Sleep(sleepTime);
				n++;

				if (n > 200)
					break;
			}

			delete m_pThreadSub;
			m_pThreadSub = NULL;
		}
		catch (...)
		{
			ASSERT(FALSE);
		}
	}
}

// ===============================
// 새로운 UI 관련 함수들
// ===============================

void CEVMQTTDlg::InitStatusControls()
{
	// 활동 라벨 설정
	m_staticActivityLabel.SetWindowText(_T("최근 활동"));

	// 폰트 설정 (선택사항)
	CFont* pFont = GetFont();
	if (pFont)
	{
		m_staticMqttStatus.SetFont(pFont);
		m_staticTagInfo.SetFont(pFont);
		m_staticPerformance.SetFont(pFont);
	}

	TRACE("상태 컨트롤 초기화 완료\n");
}

void CEVMQTTDlg::InitActivityList()
{
	if (!::IsWindow(m_listActivity.GetSafeHwnd())) {
		TRACE("활동 리스트 컨트롤이 유효하지 않습니다.\n");
		return;
	}

	// 기존 컬럼 삭제
	while (m_listActivity.DeleteColumn(0));

	// 확장 스타일 설정
	DWORD dwStyle = m_listActivity.GetExtendedStyle();
	m_listActivity.SetExtendedStyle(dwStyle | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

	// 컬럼 추가
	m_listActivity.InsertColumn(0, _T("시간"), LVCFMT_LEFT, 80);
	m_listActivity.InsertColumn(1, _T("태그명"), LVCFMT_LEFT, 120);
	m_listActivity.InsertColumn(2, _T("값"), LVCFMT_LEFT, 150);
	m_listActivity.InsertColumn(3, _T("상태"), LVCFMT_CENTER, 60);

	TRACE("활동 리스트 초기화 완료\n");
}

void CEVMQTTDlg::UpdateMqttStatus(bool connected, const CString& host, int port)
{
	m_bMqttConnected = connected;
	m_strMqttHost = host;
	m_nMqttPort = port;

	CString statusText;
	if (connected)
	{
		statusText.Format(_T("MQTT 브로커: 연결됨 (%s:%d)"), host, port);
	}
	else
	{
		statusText = _T("MQTT 브로커: 연결 안됨");
	}

	m_staticMqttStatus.SetWindowText(statusText);
	TRACE("MQTT 상태 업데이트: %s\n", (LPCTSTR)statusText);
}

void CEVMQTTDlg::UpdateTagInfo(int activeTagCount, int totalTagCount)
{
	m_nActiveTagCount = activeTagCount;
	m_nTotalTagCount = totalTagCount;

	CString statusText;
	statusText.Format(_T("태그 매핑: %d개 활성"), activeTagCount);
	if (totalTagCount > 0)
	{
		statusText.AppendFormat(_T(" / %d개 전체"), totalTagCount);
	}

	m_staticTagInfo.SetWindowText(statusText);
}

void CEVMQTTDlg::UpdatePerformance(int messagesPerSec, int successRate)
{
	m_nMessagesPerSec = messagesPerSec;
	m_nSuccessRate = successRate;

	CString statusText;
	statusText.Format(_T("처리 속도: %d msg / sec"), messagesPerSec);
	if (successRate >= 0)
	{
		statusText.AppendFormat(_T(" (성공률 %d%%)"), successRate);
	}

	m_staticPerformance.SetWindowText(statusText);
}

void CEVMQTTDlg::AddActivityLog(const CString& tagName, const CString& value,
	ActivityLogItem::LogType type, const CString& status)
{
	TRACE("=== AddActivityLog 파라미터 디버깅 ===\n");
	TRACE("tagName 주소: %p, 내용: [%s]\n", &tagName, (LPCTSTR)tagName);
	TRACE("value 주소: %p, 내용: [%s]\n", &value, (LPCTSTR)value);
	TRACE("status 주소: %p, 내용: [%s]\n", &status, (LPCTSTR)status);

	// 🔍 status 문자열의 각 바이트 확인
	TRACE("status 길이: %d\n", status.GetLength());
	for (int i = 0; i < min(status.GetLength(), 10); i++) {
		TCHAR ch = status.GetAt(i);
		TRACE("  status[%d]: '%c' (0x%02X)\n", i,
			(ch >= 32 && ch <= 126) ? ch : '?', (unsigned int)ch);
	}

	// 🔍 임시 객체 생성해서 비교
	CString tempStatus = status;  // 복사 생성
	TRACE("tempStatus 주소: %p, 내용: [%s]\n", &tempStatus, (LPCTSTR)tempStatus);

	CSingleLock lock(&m_activityMutex, TRUE);
	if (!lock.IsLocked()) return;

	ActivityLogItem logItem;

	// 명시적으로 각 필드 설정
	logItem.tagName = tagName;
	logItem.value = value;
	logItem.type = type;
	logItem.status = status;
	logItem.fullTime = CTime::GetCurrentTime();
	logItem.timestamp = logItem.fullTime.Format(_T("%H:%M:%S"));

	// 즉시 디버깅 출력
	TRACE("=== AddActivityLog ===\n");
	TRACE("Input - Tag=[%s], Value=[%s], Status=[%s], Type=%d\n",
		(LPCTSTR)tagName, (LPCTSTR)value, (LPCTSTR)status, (int)type);
	TRACE("LogItem - Tag=[%s], Value=[%s], Status=[%s], Type=%d\n",
		(LPCTSTR)logItem.tagName, (LPCTSTR)logItem.value, (LPCTSTR)logItem.status, (int)logItem.type);

	// 벡터에 추가하기 전에 한번 더 체크
	m_activityLogs.push_back(logItem);

	// 추가 후 바로 체크
	if (!m_activityLogs.empty()) {
		const auto& lastItem = m_activityLogs.back();
		TRACE("Vector LastItem - Tag=[%s], Value=[%s], Status=[%s]\n",
			(LPCTSTR)lastItem.tagName, (LPCTSTR)lastItem.value, (LPCTSTR)lastItem.status);
	}

	// 로그 개수 제한
	TrimActivityLogs();

	// UI 업데이트 (비동기)
	PostMessage(WM_USER + 102, 0, 0);
}

void CEVMQTTDlg::UpdateActivityList()
{
	if (!::IsWindow(m_listActivity.GetSafeHwnd())) return;

	CSingleLock lock(&m_activityMutex, TRUE);
	if (!lock.IsLocked()) return;

	m_listActivity.DeleteAllItems();

	// 최신 로그부터 표시 (역순)
	int itemCount = 0;
	for (int i = (int)m_activityLogs.size() - 1; i >= 0 && itemCount < 50; i--, itemCount++)
	{
		const ActivityLogItem& logItem = m_activityLogs[i];

		int nItem = m_listActivity.InsertItem(itemCount, logItem.timestamp);
		if (nItem >= 0)
		{
			m_listActivity.SetItemText(nItem, 1, logItem.tagName);
			m_listActivity.SetItemText(nItem, 2, logItem.value);
			m_listActivity.SetItemText(nItem, 3, logItem.status);  // status 필드 사용 (올바름)

			// 디버그 출력으로 확인
			TRACE("Activity Log[%d]: Time=%s, Tag=%s, Value=%s, Status=%s\n",
				nItem, (LPCTSTR)logItem.timestamp, (LPCTSTR)logItem.tagName,
				(LPCTSTR)logItem.value, (LPCTSTR)logItem.status);
		}
	}

	// 첫 번째 항목으로 스크롤
	if (m_listActivity.GetItemCount() > 0)
	{
		m_listActivity.EnsureVisible(0, FALSE);
	}
}

void CEVMQTTDlg::TrimActivityLogs()
{
	/*while (m_activityLogs.size() > MAX_ACTIVITY_LOGS)
	{
		m_activityLogs.erase(m_activityLogs.begin());
	}*/
}

COLORREF CEVMQTTDlg::GetStatusColor(bool isGood)
{
	return isGood ? RGB(0, 128, 0) : RGB(255, 0, 0);  // 녹색 또는 빨간색
}

// ===============================
// 메시지 핸들러
// ===============================

LRESULT CEVMQTTDlg::OnUpdateStats(WPARAM wParam, LPARAM lParam)
{
	int parsedCount = (int)wParam;
	int totalCount = (int)lParam;

	// 성능 계산
	DWORD currentTime = GetTickCount();
	DWORD elapsed = currentTime - m_dwLastUpdateTime;

	if (elapsed >= 1000)  // 1초마다 업데이트
	{
		int processedDiff = parsedCount - m_nLastProcessedCount;
		int messagesPerSec = elapsed > 0 ? (processedDiff * 1000 / elapsed) : 0;
		int successRate = totalCount > 0 ? (parsedCount * 100 / totalCount) : 0;

		UpdatePerformance(messagesPerSec, successRate);
		UpdateTagInfo(m_nActiveTagCount, m_nTotalTagCount);  // 주기적으로 갱신

		m_dwLastUpdateTime = currentTime;
		m_nLastProcessedCount = parsedCount;
	}

	return 0;
}

LRESULT CEVMQTTDlg::OnUpdateActivityLog(WPARAM wParam, LPARAM lParam)
{
	UpdateActivityList();
	return 0;
}

// ===============================
// ThreadSub와의 인터페이스
// ===============================

void CEVMQTTDlg::UpdateParsingStats(int parsedCount, int totalCount)
{
	// 기존 OnUpdateStats 메시지 전송
	PostMessage(WM_USER + 100, parsedCount, totalCount);
}

void CEVMQTTDlg::OnTagUpdated(const CString& tagName, const CString& value, bool success)
{
	// 성공한 경우 1/10 확률로만 로그 추가 (스팸 방지)
	if (success) {
		static int successCounter = 0;
		successCounter++;
		if (successCounter % 10 != 0) {
			return; // 10번에 1번만 로그
		}
	}

	CString status = success ? _T("성공") : _T("실패");
	ActivityLogItem::LogType logType = success ? ActivityLogItem::LOG_TAG_UPDATE : ActivityLogItem::LOG_ERROR;

	AddActivityLog(tagName, value, logType, status);
}

void CEVMQTTDlg::OnMqttConnectionChanged(bool connected)
{
	CConfigManager& configManager = CConfigManager::GetInstance();
	UpdateMqttStatus(connected, configManager.GetMqttIp(), configManager.GetMqttPort());

	CString status = connected ? _T("연결") : _T("끊김");
	AddActivityLog(_T("MQTT"), configManager.GetMqttIp(), ActivityLogItem::LOG_CONNECTION, status);
}

void CEVMQTTDlg::OnBnClickedBtnConfig()
{
	// 통신 중인 경우 경고 메시지
	if (m_pThreadSub != nullptr)
	{
		if (AfxMessageBox(_T("통신 중에는 설정을 변경할 수 없습니다.\n통신을 중지하고 설정을 열까요?"),
			MB_YESNO | MB_ICONQUESTION) == IDYES)
		{
			// 통신 중지
			StopThreadSub();
			DeleteThreadSub();

			// 버튼 텍스트 변경
			CWnd* pBtn = GetDlgItem(IDC_BTN_SUB);
			if (pBtn)
			{
				pBtn->SetWindowText(_T("통신 시작"));
			}
		}
		else
		{
			return; // 사용자가 취소하면 설정 다이얼로그를 열지 않음
		}
	}

	CConfigDlg configDlg(this);
	if (configDlg.DoModal() == IDOK) {
		// 설정이 변경된 경우 UI 업데이트
		CConfigManager& configManager = CConfigManager::GetInstance();
		configManager.LoadConfig();

		UpdateMqttStatus(false, configManager.GetMqttIp(), configManager.GetMqttPort());
		UpdateTagInfo(0, configManager.GetAllTagMappings().size());

		AddActivityLog(_T("시스템"), _T("설정 변경"), ActivityLogItem::LOG_INFO, _T("완료"));
	}
}


void CEVMQTTDlg::OnBnClickedBtnViewLog()
{
	// 로그 매니저에서 로그 파일 경로 가져오기
	CLogManager& logManager = CLogManager::GetInstance();
	CString logFilePath = logManager.GetLogFilePath();

	// 로그 파일이 존재하는지 확인
	if (!logManager.LogFileExists())
	{
		AfxMessageBox(_T("아직 생성된 오류 로그가 없습니다.\n로그는 오류 발생 시 자동으로 생성됩니다."),
			MB_OK | MB_ICONINFORMATION);
		return;
	}

	// 파일 크기 확인
	int fileSize = logManager.GetLogFileSize();
	if (fileSize > 5120) // 5MB 이상
	{
		CString sizeMsg;
		sizeMsg.Format(_T("로그 파일 크기가 %dKB입니다.\n큰 파일을 열면 시간이 걸릴 수 있습니다.\n계속하시겠습니까?"),
			fileSize);

		if (AfxMessageBox(sizeMsg, MB_YESNO | MB_ICONQUESTION) != IDYES)
		{
			return;
		}
	}

	// ShellExecute로 기본 텍스트 에디터에서 로그 파일 열기
	HINSTANCE result = ShellExecute(
		this->GetSafeHwnd(),
		_T("open"),
		logFilePath,
		NULL,
		NULL,
		SW_SHOWNORMAL
	);

	// 실행 결과 확인
	if ((INT_PTR)result <= 32)
	{
		// 실패한 경우 메모장으로 직접 열기 시도
		CString notepadCmd;
		notepadCmd.Format(_T("notepad.exe \"%s\""), logFilePath);

		HINSTANCE notepadResult = ShellExecute(
			this->GetSafeHwnd(),
			_T("open"),
			_T("notepad.exe"),
			logFilePath,
			NULL,
			SW_SHOWNORMAL
		);

		if ((INT_PTR)notepadResult <= 32)
		{
			// 메모장도 실패한 경우
			CString errorMsg;
			errorMsg.Format(_T("로그 파일을 열 수 없습니다.\n수동으로 다음 경로의 파일을 확인하세요:\n\n%s"),
				logFilePath);
			AfxMessageBox(errorMsg, MB_OK | MB_ICONERROR);
		}
	}
	else
	{
		// 성공적으로 열린 경우 활동 로그에 기록
		AddActivityLog(_T("시스템"), _T("로그 파일 열기"), ActivityLogItem::LOG_INFO, _T("완료"));
	}
}
