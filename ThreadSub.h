#if !defined(AFX_THREADSUB_H__0D927586_1ECD_4104_ACF7_E40BD0064638__INCLUDED_)
#define AFX_THREADSUB_H__0D927586_1ECD_4104_ACF7_E40BD0064638__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ThreadSub.h : header file
//

#include "ParserJSON.h"

/////////////////////////////////////////////////////////////////////////////
// CThreadSub thread

class CThreadSub : public CWinThread
{
	DECLARE_DYNCREATE(CThreadSub)
public:
	CThreadSub();           // protected constructor used by dynamic creation
	virtual ~CThreadSub();

// Attributes
public:
	BOOL	m_bEndThread;

	LPVOID		pParam;
	CWnd	*m_pOwner;

	char	m_szIP[256], m_szTopic[256];
	int		m_nPort, m_nKeepAlive;
	void	SetOwner(CWnd *pOwner) { m_pOwner = pOwner; }
	void	Stop() { m_bEndThread = TRUE; }

// Operations
	// JSON 파서 인스턴스
	static CJsonParser s_jsonParser;
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CThreadSub)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	virtual int Run();
	//}}AFX_VIRTUAL

	// 파싱 통계 함수
	void UpdateStats(int parsedCount, int totalCount);

	void InitializeFileProcessing();

private:
	HANDLE m_hChangeNotification;    // 디렉토리 변경 알림 핸들
	OVERLAPPED m_overlapped;         // 비동기 I/O용 OVERLAPPED 구조체
	char m_buffer[8192];             // 변경 정보를 저장할 버퍼
	DWORD m_bytesReturned;           // 반환된 바이트 수
	HANDLE m_directoryHandle;        // 디렉토리 핸들
	bool m_bWatchDirectory;          // 디렉토리 감시 활성화 플래그

	// 파싱 통계 변수
	int m_nParsedCount;   // 파싱 성공 파일 수
	int m_nTotalCount;    // 총 JSON 파일 수

	bool IsValidJsonFile(const CString& filePath);

public:
	bool StartDirectoryWatch(const CString& folderPath);
	void StopDirectoryWatch();
	void ProcessDirectoryChanges();

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CThreadSub)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_THREADSUB_H__0D927586_1ECD_4104_ACF7_E40BD0064638__INCLUDED_)
