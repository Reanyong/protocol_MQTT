#if !defined(AFX_THREADSUB_H__0D927586_1ECD_4104_ACF7_E40BD0064638__INCLUDED_)
#define AFX_THREADSUB_H__0D927586_1ECD_4104_ACF7_E40BD0064638__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ThreadSub.h : header file
//

#include "ParserJSON.h"
#include "MqttMessageQueue.h"

// 전방 선언
class CMqttWorkerThread;

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
    CWnd* m_pOwner;

    char	m_szIP[256], m_szTopic[256];
    int		m_nPort, m_nKeepAlive;
    void	SetOwner(CWnd* pOwner) { m_pOwner = pOwner; }
    void	Stop() { m_bEndThread = TRUE; }

    // Operations
        // JSON 파서 인스턴스
    static CJsonParser s_jsonParser;

    // 파싱 통계 함수
    void UpdateStats(int parsedCount, int totalCount);

    int m_nParsedCount;   // 파싱 성공 파일 수
    int m_nTotalCount;    // 총 JSON 파일 수

    // 멀티스레드 관련 새 멤버 변수
    CMqttMessageQueue* m_pMessageQueue;
    std::vector<CMqttWorkerThread*> m_workerThreads;
    int m_workerThreadCount;

    // 새 메서드
    void CreateWorkerThreads();
    void DestroyWorkerThreads();
    void PrintThreadStatus();
    void SetWorkerThreadCount(int count) { m_workerThreadCount = count; }
    int GetTotalQueueSize() const;
    int GetTotalProcessedCount() const;

public:

    // Overrides
        // ClassWizard generated virtual function overrides
        //{{AFX_VIRTUAL(CThreadSub)
public:
    virtual BOOL InitInstance();
    virtual int ExitInstance();
    virtual int Run();
    //}}AFX_VIRTUAL

    // Device Type별 Run 함수
    int RunSubscribeMode();  // IFM 모드: MQTT Subscribe (수신)
    int RunPublishMode();    // Navifra 모드: MQTT Publish (송신)

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
