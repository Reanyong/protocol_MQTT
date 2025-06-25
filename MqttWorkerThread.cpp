// MqttWorkerThread.cpp
#include "pch.h"
#include "MqttWorkerThread.h"
#include "EVMQTTDlg.h"

IMPLEMENT_DYNCREATE(CMqttWorkerThread, CWinThread)

CMqttWorkerThread::CMqttWorkerThread()
{
	m_bAutoDelete = FALSE;
	m_pMessageQueue = nullptr;
	m_pOwner = nullptr;
	m_bEndThread = FALSE;
	m_workerID = 0;

	// Batch settings default values
	m_batchSize = 30;        // 30 messages or
	m_batchTimeoutMs = 100;  // 100ms timeout
	m_lastBatchTime = GetTickCount();

	// Initialize statistics
	m_processedCount = 0;
	m_successCount = 0;
	m_errorCount = 0;
	m_startTime = GetTickCount();
}

CMqttWorkerThread::~CMqttWorkerThread()
{
	TRACE("Worker thread %d destroyed\n", m_workerID);
}

BOOL CMqttWorkerThread::InitInstance()
{
	TRACE("Worker thread %d InitInstance called\n", m_workerID);
	return TRUE;
}

int CMqttWorkerThread::Run()
{
	TRACE("=== Worker Thread %d Started (TID: %d) ===\n",
		m_workerID, GetCurrentThreadId());

	// 초기화 상태 체크
	TRACE("Worker %d: Checking initialization...\n", m_workerID);
	TRACE("Worker %d: MessageQueue = %p\n", m_workerID, m_pMessageQueue);
	TRACE("Worker %d: Owner = %p\n", m_workerID, m_pOwner);
	TRACE("Worker %d: EndThread = %s\n", m_workerID, m_bEndThread ? "TRUE" : "FALSE");

	if (!m_pMessageQueue) {
		TRACE("ERROR: Worker %d - Message queue is NULL! Exiting...\n", m_workerID);
		return -1;
	}

	TRACE("Worker %d: Initialization OK, starting main loop...\n", m_workerID);
	m_startTime = GetTickCount();

	int loopCount = 0;
	while (!m_bEndThread)
	{
		loopCount++;

		// 처음 5번은 상세 로그
		if (loopCount <= 5) {
			TRACE("Worker %d: Loop iteration %d\n", m_workerID, loopCount);
		}

		MqttMessage msg;

		// Get message from queue (100ms timeout)
		bool popResult = false;
		if (m_pMessageQueue) {
			popResult = m_pMessageQueue->Pop(msg, 100);
		}

		if (popResult)
		{
			TRACE("Worker %d: Message received - Topic: %s, Batch size will be: %d\n",
				m_workerID, msg.topic.c_str(), m_batch.size() + 1);

			// Add to batch
			m_batch.push_back(msg);

			// Check batch processing conditions
			DWORD currentTime = GetTickCount();
			bool shouldProcess = (m_batch.size() >= m_batchSize) ||
				((currentTime - m_lastBatchTime) >= m_batchTimeoutMs);

			if (shouldProcess)
			{
				TRACE("Worker %d: Starting batch processing...\n", m_workerID);
				ProcessMessageBatch();
				m_batch.clear();
				m_lastBatchTime = currentTime;
			}
		}
		else
		{
			// 처음 3번은 Pop 실패도 로그
			if (loopCount <= 3) {
				TRACE("Worker %d: Pop failed/timeout - Queue size: %d\n",
					m_workerID, m_pMessageQueue ? (int)m_pMessageQueue->Size() : -1);
			}

			// Timeout occurred - process remaining batch
			if (!m_batch.empty())
			{
				TRACE("Worker %d: Timeout batch processing - Size: %d\n",
					m_workerID, m_batch.size());
				ProcessMessageBatch();
				m_batch.clear();
				m_lastBatchTime = GetTickCount();
			}
		}

		// 첫 5초 동안은 1초마다 생존 신호
		if (loopCount <= 50) { // 5초 (100ms * 50)
			if (loopCount % 10 == 0) {
				TRACE("Worker %d: Still alive, loop %d\n", m_workerID, loopCount);
			}
		}

		// Output statistics every 10 seconds
		static DWORD lastStatsTime = GetTickCount();
		DWORD currentTime = GetTickCount();
		if (currentTime - lastStatsTime > 10000) // 10 seconds
		{
			PrintWorkerStats();
			lastStatsTime = currentTime;
		}
	}

	// Process remaining batch on exit
	if (!m_batch.empty())
	{
		TRACE("Worker %d: Final batch processing on exit - Size: %d\n",
			m_workerID, m_batch.size());
		ProcessMessageBatch();
	}

	PrintWorkerStats(); // Final statistics
	TRACE("=== Worker Thread %d Terminated ===\n", m_workerID);
	return 0;
}

void CMqttWorkerThread::ProcessMessageBatch()
{
	if (m_batch.empty()) return;

	DWORD startTime = GetTickCount();
	int batchSuccessCount = 0;

	TRACE("Worker %d: Batch processing started - %d messages\n",
		m_workerID, m_batch.size());

	for (const auto& msg : m_batch)
	{
		if (ProcessSingleMessage(msg))
		{
			batchSuccessCount++;
			m_successCount++;
		}
		else
		{
			m_errorCount++;
		}
		m_processedCount++;
	}

	DWORD elapsed = GetTickCount() - startTime;

	TRACE("Worker %d: Batch processing completed - %d/%d success, Time: %dms\n",
		m_workerID, batchSuccessCount, m_batch.size(), elapsed);

	// Performance warning
	if (elapsed > 500) { // Warning if takes more than 500ms
		TRACE("Warning: Worker %d batch processing slow (%dms)\n", m_workerID, elapsed);
	}
}

bool CMqttWorkerThread::ProcessSingleMessage(const MqttMessage& msg)
{
	try
	{
		// JSON parsing
		bool parseResult = m_jsonParser.ParseMessage(
			msg.payload.c_str(),
			msg.payloadLength
		);

		if (!parseResult)
		{
			// UI에 파싱 실패 알림 (가끔씩만)
			static int parseErrorCount = 0;
			if (++parseErrorCount % 10 == 0) { // 10번에 1번만
				CString errorMsg;
				errorMsg.Format(_T("오류 %d회"), parseErrorCount);
				SendTagUpdateToUI(_T("파싱오류"), errorMsg, false);
			}
			return false;
		}

		// Apply MQTT tag mapping
		CString mqttTopic(msg.topic.c_str());
		bool tagResult = m_jsonParser.ApplyMqttTagMapping(mqttTopic);

		if (tagResult)
		{
			// 성공한 경우 실제 태그 정보 추출해서 UI에 알림
			// JSON에서 대표적인 값 하나 추출해서 표시
			CString extractedValue = ExtractRepresentativeValue(msg.payload);
			SendTagUpdateToUI(mqttTopic, extractedValue, true);

			return true;
		}
		else
		{
			// 실패 로그는 제한적으로만
			static int tagErrorCount = 0;
			if (++tagErrorCount % 20 == 0) { // 20번에 1번만
				SendTagUpdateToUI(mqttTopic, _T("매핑 실패"), false);
			}
			return false;
		}
	}
	catch (const std::exception& e)
	{
		CString errorMsg;
		errorMsg.Format(_T("예외: %hs"), e.what());
		SendTagUpdateToUI(CString(msg.topic.c_str()), errorMsg, false);
		return false;
	}
}

void CMqttWorkerThread::SendTagUpdateToUI(const CString& tagName, const CString& value, bool success)
{
	if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
	{
		CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;
		pDlg->OnTagUpdated(tagName, value, success);
	}
}

CString CMqttWorkerThread::ExtractRepresentativeValue(const std::string& jsonPayload)
{
	try
	{
		// JSON에서 첫 번째 값이나 의미있는 값 추출
		nlohmann::json jsonData = nlohmann::json::parse(jsonPayload);

		// 일반적인 필드명들 우선 검색
		std::vector<std::string> commonFields = {
			"value", "data", "payload", "result", "temperature", "pressure",
			"status", "state", "level", "count", "rate", "speed"
		};

		for (const auto& field : commonFields)
		{
			if (jsonData.contains(field))
			{
				auto& fieldValue = jsonData[field];
				if (fieldValue.is_string()) {
					return CString(fieldValue.get<std::string>().c_str());
				}
				else if (fieldValue.is_number()) {
					CString result;
					if (fieldValue.is_number_integer()) {
						result.Format(_T("%d"), fieldValue.get<int>());
					}
					else {
						result.Format(_T("%.2f"), fieldValue.get<double>());
					}
					return result;
				}
				else if (fieldValue.is_boolean()) {
					return fieldValue.get<bool>() ? _T("true") : _T("false");
				}
			}
		}

		// 일반적인 필드가 없으면 첫 번째 값 사용
		if (jsonData.is_object() && !jsonData.empty())
		{
			auto firstItem = jsonData.begin();
			auto& firstValue = firstItem.value();

			if (firstValue.is_string()) {
				CString result = CString(firstValue.get<std::string>().c_str());
				return result.Left(20); // 최대 20자까지만
			}
			else if (firstValue.is_number()) {
				CString result;
				if (firstValue.is_number_integer()) {
					result.Format(_T("%d"), firstValue.get<int>());
				}
				else {
					result.Format(_T("%.2f"), firstValue.get<double>());
				}
				return result;
			}
			else if (firstValue.is_boolean()) {
				return firstValue.get<bool>() ? _T("true") : _T("false");
			}
		}

		return _T("JSON Object"); // 기본값
	}
	catch (...)
	{
		return _T("Parse Error");
	}
}

void CMqttWorkerThread::PrintWorkerStats()
{
	DWORD elapsed = GetTickCount() - m_startTime;
	double elapsedSec = elapsed / 1000.0;
	double msgPerSec = elapsedSec > 0 ? (m_processedCount / elapsedSec) : 0;

	TRACE("=== Worker %d Statistics ===\n", m_workerID);
	TRACE("Processed messages: %d\n", m_processedCount);
	TRACE("Success: %d, Failed: %d\n", m_successCount, m_errorCount);
	TRACE("Success rate: %.1f%%\n", m_processedCount > 0 ? (m_successCount * 100.0 / m_processedCount) : 0);
	TRACE("Processing speed: %.1f msg/sec\n", msgPerSec);
	TRACE("Runtime: %.1f seconds\n", elapsedSec);
	TRACE("=============================\n");
}
