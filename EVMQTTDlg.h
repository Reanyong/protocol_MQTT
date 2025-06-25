#pragma once

#include "ThreadSub.h"

// 또는 기존 MqttMessageQueue.h 대신 새로운 헤더 사용
// #include "SimpleMqttQueue.h"  // 새로운 헤더 파일

// 전방 선언
class CConfigDlg;

// 활동 로그 항목 구조체
struct ActivityLogItem
{
	enum LogType {
		LOG_TAG_UPDATE,    // 태그 업데이트
		LOG_CONNECTION,    // 연결 상태 변경
		LOG_ERROR,         // 오류
		LOG_INFO           // 일반 정보
	};

	CString timestamp;     // 시간 (HH:MM:SS 형식)
	CString tagName;       // 태그명
	CString value;         // 값
	CString status;        // 상태 ("성공", "실패", "연결", "끊김" 등)
	LogType type;          // 로그 타입
	CTime fullTime;        // 정렬용 전체 시간

	ActivityLogItem() : type(LOG_INFO), fullTime(CTime::GetCurrentTime())
	{
		timestamp = fullTime.Format(_T("%H:%M:%S"));
		// 명시적 초기화 추가
		tagName = _T("");
		value = _T("");
		status = _T("INIT");  // 초기값 설정
	}

	// 복사 생성자 명시적 정의
	ActivityLogItem(const ActivityLogItem& other)
		: timestamp(other.timestamp), tagName(other.tagName),
		value(other.value), status(other.status),
		type(other.type), fullTime(other.fullTime)
	{
	}

	// 대입 연산자 명시적 정의
	ActivityLogItem& operator=(const ActivityLogItem& other)
	{
		if (this != &other) {
			timestamp = other.timestamp;
			tagName = other.tagName;
			value = other.value;
			status = other.status;
			type = other.type;
			fullTime = other.fullTime;
		}
		return *this;
	}
};

class CEVMQTTDlg : public CDialogEx
{
	// 생성입니다.
public:
	CEVMQTTDlg(CWnd* pParent = nullptr);	// 표준 생성자입니다.

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_EVMQTT_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.

	CThreadSub* m_pThreadSub;

	void		BeginThreadSub();
	void		StopThreadSub();
	void		DeleteThreadSub();

	// 구현입니다.
protected:
	HICON m_hIcon;

	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();

	// 파싱 통계 관련 함수
	afx_msg LRESULT OnUpdateStats(WPARAM wParam, LPARAM lParam);

	// 활동 로그 관련 함수
	afx_msg LRESULT OnUpdateActivityLog(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnBnClickedBtnSub();
	afx_msg void OnBnClickedBtnConfig();
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();

	// 새로운 UI 컨트롤들
	CStatic m_staticMqttStatus;      // MQTT 브로커 상태
	CStatic m_staticTagInfo;         // 태그 매핑 정보
	CStatic m_staticPerformance;     // 처리 속도 정보
	CStatic m_staticActivityLabel;   // 활동 라벨
	CListCtrl m_listActivity;        // 최근 활동 로그

	// UI 초기화 및 업데이트 함수들
	void InitStatusControls();
	void InitActivityList();
	void UpdateMqttStatus(bool connected, const CString& host, int port);
	void UpdateTagInfo(int activeTagCount, int totalTagCount);
	void UpdatePerformance(int messagesPerSec, int successRate);
	void AddActivityLog(const CString& tagName, const CString& value,
		ActivityLogItem::LogType type = ActivityLogItem::LOG_TAG_UPDATE,
		const CString& status = _T("성공"));

private:
	// 상태 정보 변수들
	bool m_bMqttConnected;           // MQTT 연결 상태
	CString m_strMqttHost;           // MQTT 호스트
	int m_nMqttPort;                 // MQTT 포트
	int m_nActiveTagCount;           // 활성 태그 수
	int m_nTotalTagCount;            // 전체 태그 수
	int m_nMessagesPerSec;           // 초당 메시지 처리량
	int m_nSuccessRate;              // 성공률 (%)

	// 활동 로그 관리
	std::vector<ActivityLogItem> m_activityLogs;
	CCriticalSection m_activityMutex;
	static const int MAX_ACTIVITY_LOGS = 100;  // 최대 로그 개수

	// 성능 측정용
	DWORD m_dwLastUpdateTime;        // 마지막 업데이트 시간
	int m_nLastProcessedCount;       // 마지막 처리된 메시지 수

	// 내부 헬퍼 함수들
	void UpdateActivityList();
	void TrimActivityLogs();         // 오래된 로그 제거
	COLORREF GetStatusColor(bool isGood);

public:
	// ThreadSub와의 인터페이스
	void UpdateParsingStats(int parsedCount, int totalCount);
	void OnTagUpdated(const CString& tagName, const CString& value, bool success);
	void OnMqttConnectionChanged(bool connected);
};
