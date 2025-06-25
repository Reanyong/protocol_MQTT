// MqttWorkerThread.h
#pragma once

#include "MqttMessageQueue.h"
#include "ParserJSON.h"

// 전방 선언
class CEVMQTTDlg;
struct ActivityLogItem;

class CMqttWorkerThread : public CWinThread
{
	DECLARE_DYNCREATE(CMqttWorkerThread)

public:
	CMqttWorkerThread();
	virtual ~CMqttWorkerThread();

	// 설정 메서드
	void SetMessageQueue(CMqttMessageQueue* pQueue) { m_pMessageQueue = pQueue; }
	void SetOwner(CWnd* pOwner) { m_pOwner = pOwner; }
	void SetWorkerID(int id) { m_workerID = id; }
	void Stop() { m_bEndThread = TRUE; }

	// 통계 정보
	int GetProcessedCount() const { return m_processedCount; }
	int GetSuccessCount() const { return m_successCount; }
	int GetErrorCount() const { return m_errorCount; }

	// 배치 설정
	void SetBatchSize(size_t size) { m_batchSize = size; }
	void SetBatchTimeout(DWORD timeout) { m_batchTimeoutMs = timeout; }

	virtual BOOL InitInstance() override;
	virtual int Run() override;

private:
	// 기본 멤버 변수
	CMqttMessageQueue* m_pMessageQueue;
	CWnd* m_pOwner;
	BOOL m_bEndThread;
	int m_workerID;

	// JSON 파서 (각 워커가 독립적으로 보유)
	CJsonParser m_jsonParser;

	// 배치 처리용 변수들
	std::vector<MqttMessage> m_batch;
	size_t m_batchSize;
	DWORD m_batchTimeoutMs;
	DWORD m_lastBatchTime;

	// 통계 변수
	int m_processedCount;
	int m_successCount;
	int m_errorCount;
	DWORD m_startTime;

	// 내부 처리 메서드
	void ProcessMessageBatch();
	bool ProcessSingleMessage(const MqttMessage& msg);
	void PrintWorkerStats();

	// UI 연동 메서드
	void SendTagUpdateToUI(const CString& tagName, const CString& value, bool success);
	CString ExtractRepresentativeValue(const std::string& jsonPayload);
};
