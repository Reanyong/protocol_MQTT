#pragma once

#include "ThreadSub.h"

// 디버그 로그 항목 구조체
struct DebugLogItem
{
	enum LogType {
		LOG_INFO,    // 일반 정보
		LOG_SUCCESS, // 성공
		LOG_WARNING, // 경고
		LOG_ERROR    // 오류
	};

	CString message;      // 로그 메시지
	CString filePath;     // 관련 파일 경로
	LogType type;         // 로그 유형
	CTime timestamp;      // 로그 시간

	bool shouldDisplay;   // 표시 여부 플래그

	DebugLogItem() : type(LOG_INFO), shouldDisplay(true) {}
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

	void SetDebugLogFilter(bool showInfo, bool showSuccess, bool showWarning, bool showError);

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

	// 디버그 관련 함수
	afx_msg void OnBnClickedButtonClearLog();
	afx_msg LRESULT OnUpdateDebugLog(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnBnClickedBtnSub();
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();

	// 파싱 통계 관련
	CStatic m_staticStats;
	void UpdateParsingStats(int parsedCount, int totalCount);

	// 디버그 관련
	CListCtrl m_listDebug;
	void InitDebugList();
	void AddDebugLog(const CString& message, const CString& filePath = _T(""), DebugLogItem::LogType type = DebugLogItem::LOG_INFO);
	void UpdateDebugList();

private:
	int m_nParsedCount;       // 파싱 성공 파일 수
	int m_nTotalCount;        // 총 JSON 파일 수

	std::vector<DebugLogItem> m_debugLogs;
	//mutable std::mutex m_logMutex;
	CCriticalSection m_logMutex;

	bool m_showInfoLogs;      // 정보 로그 표시 여부
	bool m_showSuccessLogs;   // 성공 로그 표시 여부
	bool m_showWarningLogs;   // 경고 로그 표시 여부
	bool m_showErrorLogs;     // 오류 로그 표시 여부
};
