// MqttWorkerThread.cpp
#include "pch.h"
#include "MqttWorkerThread.h"
#include "EVMQTTDlg.h"
#include "LogManager.h"

IMPLEMENT_DYNCREATE(CMqttWorkerThread, CWinThread)

CMqttWorkerThread::CMqttWorkerThread()
{
	m_bAutoDelete = FALSE;
	m_pMessageQueue = nullptr;
	m_pOwner = nullptr;
	m_bEndThread = FALSE;
	m_workerID = 0;

	// Batch settings default values - 최적화: 실시간성 향상
	m_batchSize = 50;
	m_batchTimeoutMs = 100;
	m_lastBatchTime = GetTickCount();

	// Initialize statistics
	m_processedCount = 0;
	m_successCount = 0;
	m_errorCount = 0;
	m_startTime = GetTickCount();
}

CMqttWorkerThread::~CMqttWorkerThread()
{
	TRACE("Worker thread %d destructor called\n", m_workerID);

	// 큐 참조 해제는 ThreadSub에서 처리하므로 여기서는 하지 않음
	m_pMessageQueue = nullptr; // 포인터만 NULL로 설정

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

	// 초기화 상태 체크 - 강화된 검증
	TRACE("Worker %d: Checking initialization...\n", m_workerID);
	TRACE("Worker %d: MessageQueue = %p\n", m_workerID, m_pMessageQueue);
	TRACE("Worker %d: Owner = %p\n", m_workerID, m_pOwner);
	TRACE("Worker %d: EndThread = %s\n", m_workerID, m_bEndThread ? "TRUE" : "FALSE");
	TRACE("Worker %d: WorkerID = %d\n", m_workerID, m_workerID);
	TRACE("Worker %d: BatchSize = %d\n", m_workerID, m_batchSize);
	TRACE("Worker %d: BatchTimeout = %d\n", m_workerID, m_batchTimeoutMs);

	// 필수 조건 검증
	if (!m_pMessageQueue) {
		TRACE("CRITICAL ERROR: Worker %d - Message queue is NULL! Exiting...\n", m_workerID);
		return -1;
	}

	if (!m_pOwner) {
		TRACE("WARNING: Worker %d - Owner is NULL! UI updates will not work\n", m_workerID);
	}

	if (m_workerID <= 0) {
		TRACE("ERROR: Worker %d - Invalid worker ID! Exiting...\n", m_workerID);
		return -2;
	}

	if (m_batchSize <= 0 || m_batchTimeoutMs <= 0) {
		TRACE("ERROR: Worker %d - Invalid batch settings! BatchSize=%d, Timeout=%d\n", 
			m_workerID, m_batchSize, m_batchTimeoutMs);
		return -3;
	}

	// 메시지 큐 상태 검증
	try {
		if (m_pMessageQueue->IsShutdown()) {
			TRACE("ERROR: Worker %d - Message queue is already shutdown! Exiting...\n", m_workerID);
			return -4;
		}
		
		size_t queueSize = m_pMessageQueue->Size();
		TRACE("Worker %d: Message queue initial size = %d\n", m_workerID, queueSize);
	}
	catch (const std::exception& e) {
		TRACE("ERROR: Worker %d - Exception during queue validation: %s\n", m_workerID, e.what());
		return -5;
	}
	catch (...) {
		TRACE("ERROR: Worker %d - Unknown exception during queue validation\n", m_workerID);
		return -6;
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

		// 큐 상태 확인 - NULL 체크와 shutdown 상태 체크
		if (!m_pMessageQueue || m_pMessageQueue->IsShutdown()) {
			TRACE("Worker %d: Queue is null or shutdown, exiting\n", m_workerID);
			break;
		}

		MqttMessage msg;

		// ===== 성능 최적화: Pop timeout 100ms→10ms =====
		// 이전: 100ms timeout → 메시지 대기 시 최대 100ms 지연
		// 현재: 10ms timeout → 메시지 대기 시 최대 10ms 지연, 10배 빠른 반응
		bool popResult = false;
		try {
			popResult = m_pMessageQueue->Pop(msg, 10);
		}
		catch (const std::exception& e) {
			TRACE("Worker %d: Exception during Pop: %s\n", m_workerID, e.what());
			break;
		}
		catch (...) {
			TRACE("Worker %d: Unknown exception during Pop\n", m_workerID);
			break;
		}

		if (popResult)
		{
			// ===== 성능 최적화: TRACE 출력 완전 제거 (운영 모드) =====
			// TRACE("Worker %d: Message received - Topic: %s, Batch size will be: %d\n",
			//	m_workerID, msg.topic.c_str(), m_batch.size() + 1);

			// Add to batch
			m_batch.push_back(msg);

			// Check batch processing conditions
			DWORD currentTime = GetTickCount();
			bool shouldProcess = (m_batch.size() >= m_batchSize) ||
				((currentTime - m_lastBatchTime) >= m_batchTimeoutMs);

			if (shouldProcess)
			{
				// ===== 성능 최적화: TRACE 출력 완전 제거 =====
				// TRACE("Worker %d: Starting batch processing...\n", m_workerID);
				ProcessMessageBatch();
				m_batch.clear();
				m_lastBatchTime = currentTime;
			}
		}
		else
		{
			// 처음 3번은 Pop 실패도 로그
			if (loopCount <= 3) {
				// 안전한 큐 크기 확인
				int queueSize = -1;
				if (m_pMessageQueue && !m_pMessageQueue->IsShutdown()) {
					try {
						queueSize = (int)m_pMessageQueue->Size();
					}
					catch (...) {
						queueSize = -2; // 예외 발생
					}
				}

				TRACE("Worker %d: Pop failed/timeout - Queue size: %d\n",
					m_workerID, queueSize);
			}

			// Timeout occurred - process remaining batch
			if (!m_batch.empty())
			{
				// ===== 성능 최적화: TRACE 출력 제거 =====
				// TRACE("Worker %d: Timeout batch processing - Size: %d\n",
				//	m_workerID, m_batch.size());
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

	// ===== 성능 최적화: 시간 측정도 선택적으로만 =====
	// DWORD startTime = GetTickCount();
	int batchSuccessCount = 0;

	// ===== 성능 최적화: TRACE 출력 완전 제거 =====
	// TRACE("Worker %d: Batch processing started - %d messages\n",
	//	m_workerID, m_batch.size());

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

	// ===== 성능 최적화: 시간 측정 및 TRACE 출력 제거 =====
	// DWORD elapsed = GetTickCount() - startTime;
	// TRACE("Worker %d: Batch processing completed - %d/%d success, Time: %dms\n",
	//	m_workerID, batchSuccessCount, m_batch.size(), elapsed);
	// if (elapsed > 500) {
	//	TRACE("Warning: Worker %d batch processing slow (%dms)\n", m_workerID, elapsed);
	// }
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
			// 파싱 실패는 주기적으로만 UI에 알림 (스팸 방지)
			static int parseErrorCount = 0;
			if (++parseErrorCount % 50 == 0) { // 50번에 1번만
				CString errorMsg;
				errorMsg.Format(_T("JSON파싱 실패 (x%d)"), parseErrorCount);
				CString topicInfo;
				topicInfo.Format(_T("토픽: %s"), msg.topic.c_str());
				SendTagUpdateToUI(topicInfo, errorMsg, false);

				// 실패 로그 기록
				CLogManager& logManager = CLogManager::GetInstance();
				CString payloadPreview;
				if (msg.payloadLength > 0) {
					int previewLen = min(150, msg.payloadLength);
					payloadPreview = CString(msg.payload.substr(0, previewLen).c_str());
					if (msg.payloadLength > 150) {
						payloadPreview += _T("...");
					}
				}
				logManager.WriteErrorLog(_T("메시지파싱실패"),
					CString(msg.topic.c_str()),
					CString(_T("JSON 파싱 실패 (누적: ")) + errorMsg + _T(")"),
					payloadPreview);
			}
			return false;
		}

		// Apply MQTT tag mapping
		CString mqttTopic(msg.topic.c_str());
		bool tagResult = m_jsonParser.ApplyMqttTagMapping(mqttTopic);

		if (tagResult)
		{
			// 성공한 경우 UI 알림은 SendTagUpdateToUI 내부에서 제한됨
			static int successCount = 0;
			successCount++;

			// 매우 제한적으로만 UI 업데이트
			if (successCount % 200 == 0) { // 200번에 1번만
				CString extractedValue = ExtractRepresentativeValue(msg.payload);
				CString topicInfo;
				topicInfo.Format(_T("토픽: %s"), mqttTopic);
				SendTagUpdateToUI(topicInfo, extractedValue, true);
			}

			return true;
		}
		else
		{
			// 실패한 경우: 유효한 태그가 없으면 아예 UI 업데이트 안함
			// (ParserJSON에서 이미 return false 했으므로 여기 오면 실제 매핑 실패)
			static int tagErrorCount = 0;
			if (++tagErrorCount % 50 == 0) { // 빈도 더 줄임 (50번에 1번)
				CString errorMsg;
				errorMsg.Format(_T("EasyView 연결 확인 필요 (x%d)"), tagErrorCount);
				CString topicInfo;
				topicInfo.Format(_T("토픽: %s"), mqttTopic);
				SendTagUpdateToUI(topicInfo, errorMsg, false);
			}
			return false;
		}
	}
	catch (const std::exception& e)
	{
		// 예외는 항상 UI에 알림 (중요하므로)
		CString errorMsg;
		errorMsg.Format(_T("예외: %hs"), e.what());
		CString topicInfo;
		topicInfo.Format(_T("토픽: %s"), msg.topic.c_str());
		SendTagUpdateToUI(topicInfo, errorMsg, false);
		return false;
	}
}

void CMqttWorkerThread::SendTagUpdateToUI(const CString& tagName, const CString& value, bool success)
{
	if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
	{
		// *** 훨씬 더 엄격한 UI 업데이트 빈도 제한 ***
		static DWORD lastUIUpdateTime = 0;
		static int successCounter = 0;
		static int errorCounter = 0;

		DWORD currentTime = GetTickCount();

		if (success)
		{
			successCounter++;

			// 성공한 경우: 100번에 1번만 UI 업데이트 (또는 5초마다)
			bool shouldUpdateSuccess = (successCounter % 100 == 0) ||
				(currentTime - lastUIUpdateTime > 5000);

			if (shouldUpdateSuccess)
			{
				CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;

				// 성공 케이스는 통계 정보로 표시
				CString displayValue;
				displayValue.Format(_T("처리완료 (총 %d건)"), successCounter);
				pDlg->OnTagUpdated(_T("MQTT처리"), displayValue, true);

				lastUIUpdateTime = currentTime;
			}
		}
		else
		{
			errorCounter++;

			// 실패한 경우: 10번에 1번만 UI 업데이트 (또는 2초마다)
			bool shouldUpdateError = (errorCounter % 10 == 0) ||
				(currentTime - lastUIUpdateTime > 2000);

			if (shouldUpdateError)
			{
				CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;

				CString displayValue;
				displayValue.Format(_T("오류 (총 %d건) - %s"), errorCounter, value);
				pDlg->OnTagUpdated(tagName, displayValue, false);

				lastUIUpdateTime = currentTime;
			}
		}
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
					CString result = CString(fieldValue.get<std::string>().c_str());
					return result.Left(15); // 최대 15자까지만 (UI 공간 절약)
				}
				else if (fieldValue.is_number()) {
					CString result;
					if (fieldValue.is_number_integer()) {
						result.Format(_T("%d"), fieldValue.get<int>());
					}
					else {
						result.Format(_T("%.1f"), fieldValue.get<double>()); // 소수점 1자리로 제한
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
				return result.Left(15); // 최대 15자까지만
			}
			else if (firstValue.is_number()) {
				CString result;
				if (firstValue.is_number_integer()) {
					result.Format(_T("%d"), firstValue.get<int>());
				}
				else {
					result.Format(_T("%.1f"), firstValue.get<double>());
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

	// *** 통계 정보를 UI에 주기적으로 전송 (10초마다) ***
	static DWORD lastStatsToUI = 0;
	DWORD currentTime = GetTickCount();

	if (currentTime - lastStatsToUI > 10000) // 10초마다
	{
		if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
		{
			CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;

			CString statsMsg;
			statsMsg.Format(_T("Worker Thread %d: %.1f msg/s"), m_workerID, msgPerSec);
			pDlg->OnTagUpdated(_T("성능통계"), statsMsg, true);
		}

		lastStatsToUI = currentTime;
	}
}
