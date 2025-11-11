// CThreadSub.cpp : implementation file
//

#include "pch.h"
#include "EVMQTT.h"
#include "EVMQTTDlg.h"
#include "ThreadSub.h"
#include "ConfigManager.h"
#include "JsonResultManager.h"
#include "MqttWorkerThread.h"
#include "LogManager.h"
#include "TagInfoCache.h"
#include "JsonPathTokenCache.h"
#include "XlsxConfigManager.h"  // XLSX 기반 태그 매핑

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//#pragma execution_character_set("utf-8")

CJsonParser CThreadSub::s_jsonParser;

/////////////////////////////////////////////////////////////////////////////
// CThreadSub

IMPLEMENT_DYNCREATE(CThreadSub, CWinThread)

CThreadSub::CThreadSub()
{
	m_bAutoDelete = FALSE;
	m_pOwner = NULL;
	m_bEndThread = FALSE;
	pParam = NULL;

	/*
	Configuration handled by ConfigManager
	sprintf_s(m_szIP, sizeof(m_szIP), "127.0.0.1");
	sprintf_s(m_szTopic, sizeof(m_szTopic), "my_topic");
	m_nPort = 1883;
	m_nKeepAlive = 60;
	*/

	// Initialize parsing statistics
	m_nParsedCount = 0;
	m_nTotalCount = 0;

	// Initialize multithreading
	m_pMessageQueue = nullptr;

	// 단일 워커 스레드 모드 (캐시 최적화로 충분한 성능 확보)
	m_workerThreadCount = 1;
	TRACE("Worker Thread Mode: Single Thread\n");

	// 출력 제어 관련 초기화
	m_hCtrlQueue = NULL;
	m_hCtrlSync = NULL;
	m_pCtrlQueue = NULL;
	m_psd = NULL;
}

CThreadSub::~CThreadSub()
{
	TRACE("~CThreadSub() destructor called\n");

	// 워커 스레드 정리 (Subscribe 모드에서만 사용)
	DestroyWorkerThreads();

	// 메시지 큐 정리 (Subscribe 모드에서만 사용)
	if (m_pMessageQueue) {
		TRACE("Deleting message queue in destructor\n");
		delete m_pMessageQueue;
		m_pMessageQueue = nullptr;
	}

	// 출력 제어 정리 (Publish 모드에서만 사용)
	CloseOutputMap();

	TRACE("~CThreadSub() destructor completed\n");
}

BOOL CThreadSub::InitInstance()
{
	// Connect to EasyView engine
	char szBuff[256] = { 0, };
	char szProjectName[256] = { 0, };

	EV_GetConfigFile(szBuff);
	::GetPrivateProfileString(
		"EasyView",        // Section name
		"Project",         // Key name
		"",                // Default value (empty string if not found)
		szProjectName,     // Result buffer
		sizeof(szProjectName),
		szBuff             // INI file path
	);

	int nResult = EV_OpenMem(szProjectName);
	if (nResult > 0) {
		TRACE(_T("EasyView Engine Connected: %s\n"), szProjectName);
	}
	else {
		TRACE("EasyView Engine Connection Failed: %d\n", nResult);
	}

	return TRUE;
}

int CThreadSub::ExitInstance()
{
	return CWinThread::ExitInstance();
}

BEGIN_MESSAGE_MAP(CThreadSub, CWinThread)
	//{{AFX_MSG_MAP(CThreadSub)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CThreadSub message handlers

#include <mosquitto.h>
#include <thread>  // std::thread::hardware_concurrency()
#include <set>     // std::set for unique topics
#pragma comment(lib, ".\\Lib\\mosquitto.lib")

#define strdup _strdup

void connect_callback(struct mosquitto* mosq, void* obj, int result)
{
	CThreadSub* pThreadSub = static_cast<CThreadSub*>(obj);

	if (result == 0) {
		TRACE("MQTT Connection Success\n");

		// UI에 연결 성공 알림
		if (pThreadSub && pThreadSub->m_pOwner && ::IsWindow(pThreadSub->m_pOwner->GetSafeHwnd()))
		{
			CEVMQTTDlg* pDlg = (CEVMQTTDlg*)pThreadSub->m_pOwner;
			pDlg->OnMqttConnectionChanged(true);
		}
	}
	else {
		TRACE("MQTT Connection Failed: Error code %d\n", result);

		// UI에 연결 실패 알림
		if (pThreadSub && pThreadSub->m_pOwner && ::IsWindow(pThreadSub->m_pOwner->GetSafeHwnd()))
		{
			CEVMQTTDlg* pDlg = (CEVMQTTDlg*)pThreadSub->m_pOwner;
			pDlg->OnMqttConnectionChanged(false);
		}

		else {
			TRACE("=== MQTT Connection Failed! Error code: %d ===\n", result);

			// 실패 로그 기록
			CLogManager& logManager = CLogManager::GetInstance();
			CString errorMsg;
			CString detailMsg;

			switch (result) {
			case 1:
				errorMsg = _T("프로토콜 버전 오류");
				detailMsg = _T("Connection refused: bad protocol version");
				break;
			case 2:
				errorMsg = _T("클라이언트 ID 거부");
				detailMsg = _T("Connection refused: client ID rejected");
				break;
			case 3:
				errorMsg = _T("서버 사용 불가");
				detailMsg = _T("Connection refused: server unavailable");
				break;
			case 4:
				errorMsg = _T("인증 실패");
				detailMsg = _T("Connection refused: bad username/password");
				break;
			case 5:
				errorMsg = _T("권한 없음");
				detailMsg = _T("Connection refused: not authorized");
				break;
			default:
				errorMsg = _T("알 수 없는 오류");
				detailMsg.Format(_T("Connection refused: unknown error (code: %d)"), result);
				break;
			}

			logManager.WriteErrorLog(_T("MQTT연결실패"), _T("브로커연결"), errorMsg, detailMsg);
		}
	}
}

void subscribe_callback(struct mosquitto* mosq, void* obj, int mid, int qos_count, const int* granted_qos)
{
	TRACE("=== MQTT Subscription Success! MID: %d ===\n", mid);
	for (int i = 0; i < qos_count; i++) {
		TRACE("Subscription QoS[%d]: %d\n", i, granted_qos[i]);
	}
}

void disconnect_callback(struct mosquitto* mosq, void* obj, int rc)
{
	CThreadSub* pThreadSub = static_cast<CThreadSub*>(obj);

	TRACE("=== MQTT Disconnected! Reason code: %d ===\n", rc);

	// UI에 연결 끊김 알림
	if (pThreadSub && pThreadSub->m_pOwner && ::IsWindow(pThreadSub->m_pOwner->GetSafeHwnd()))
	{
		CEVMQTTDlg* pDlg = (CEVMQTTDlg*)pThreadSub->m_pOwner;
		pDlg->OnMqttConnectionChanged(false);

		if (rc != 0) {  // 비정상 종료인 경우만
			pDlg->AddActivityLog(_T("MQTT"), _T("연결 끊김"), ActivityLogItem::LOG_ERROR, _T("재연결 시도 중"));
		}
	}

	// rc == 0: 정상 종료 (mosquitto_disconnect 호출)
	// rc != 0: 비정상 종료 (네트워크 문제 등)
	if (rc != 0) {
		TRACE("Unexpected disconnect, will attempt to reconnect...\n");
		// mosquitto_loop_start()가 자동으로 재연결 시도함
		mosquitto_reconnect_async(mosq);
	}
}

void message_callback(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg)
{
	// Basic validation
	if (!msg || !obj) {
		TRACE("message_callback: null pointer received\n");
		return;
	}

	CThreadSub* pThreadSub = static_cast<CThreadSub*>(obj);
	if (!pThreadSub) {
		TRACE("message_callback: ThreadSub object is null\n");
		return;
	}

	TRACE("=== MQTT Message Received ===\n");
	TRACE("Topic: '%s'\n", msg->topic ? msg->topic : "null");
	TRACE("Size: %d bytes\n", msg->payloadlen);

	// Empty message check
	if (msg->payloadlen == 0 || !msg->payload) {
		TRACE("Empty message - skipping\n");
		return;
	}

	// JSON format check
	const char* payload = (const char*)msg->payload;
	if (payload[0] != '{' && payload[0] != '[') {
		TRACE("Non-JSON message: %.*s\n",
			(msg->payloadlen > 50 ? 50 : msg->payloadlen), payload);
		return;
	}

	// Multithreaded approach: Add to message queue
	if (pThreadSub->m_pMessageQueue) {
		MqttMessage mqttMsg(msg->topic, payload, msg->payloadlen);

		if (pThreadSub->m_pMessageQueue->Push(mqttMsg)) {
			pThreadSub->m_nTotalCount++;

			// Log every 100 messages
			static int msgCounter = 0;
			if (++msgCounter % 100 == 0) {
				TRACE("Message added to queue (Total %d) - Queue size: %d\n",
					msgCounter, pThreadSub->m_pMessageQueue->Size());
			}
		}
		else {
			TRACE("Failed to add message to queue\n");
		}
	}
	else {
		TRACE("Message queue is null - message ignored\n");
	}
}

void CThreadSub::UpdateStats(int parsedCount, int totalCount)
{
	if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
	{
		TRACE("UpdateStats: parsedCount=%d, totalCount=%d\n", parsedCount, totalCount);

		MSG msg;
		while (::PeekMessage(&msg, m_pOwner->GetSafeHwnd(), WM_USER + 100, WM_USER + 100, PM_REMOVE))
		{
			TRACE("Previous statistics message removed from queue\n");
		}

		::PostMessage(m_pOwner->GetSafeHwnd(), WM_USER + 100, parsedCount, totalCount);
	}
}

int CThreadSub::Run()
{
	TRACE("ThreadSub Started\n");

	// EasyView 엔진 연결
	char szBuff[256] = { 0, };
	char szProjectName[256] = { 0, };

	EV_GetConfigFile(szBuff);
	::GetPrivateProfileString(
		"EasyView", "Project", "", szProjectName,
		sizeof(szProjectName), szBuff
	);

	int nResult = EV_OpenMem(szProjectName);
	if (nResult > 0) {
		TRACE("EasyView Engine connected successfully: %s\n", szProjectName);
	}
	else {
		TRACE("EasyView Engine connection failed: %d\n", nResult);
	}

	// Device Type 체크
	CConfigManager& configManager = CConfigManager::GetInstance();
	configManager.LoadConfig();

	CString deviceType = configManager.GetDeviceType();
	TRACE("Device Type: %s\n", (LPCTSTR)deviceType);

	// Device Type에 따라 분기 (대소문자 구분 없음)
	if (deviceType.CompareNoCase(_T("IFM")) == 0) {
		TRACE("IFM Mode: Subscribe\n");
		return RunSubscribeMode();
	}
	else if (deviceType.CompareNoCase(_T("Navifra")) == 0) {
		TRACE("Navifra Mode: Publish\n");
		return RunPublishMode();
	}
	else {
		TRACE("ERROR: Unknown Device Type: %s\n", (LPCTSTR)deviceType);
		if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
		{
			CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;
			pDlg->AddActivityLog(_T("System"), _T("Unknown Device Type"), ActivityLogItem::LOG_ERROR, deviceType);
		}
		return -1;
	}
}

int CThreadSub::RunSubscribeMode()
{
	TRACE("RunSubscribeMode Started\n");

	CConfigManager& configManager = CConfigManager::GetInstance();

	// 메시지 큐 생성
	TRACE("Creating message queue...\n");
	m_pMessageQueue = new CMqttMessageQueue(5000);
	TRACE("Message queue creation completed\n");

	// ===== XLSX 설정 파일 로드 =====
	TRACE("=== XLSX 설정 파일 로드 시작 ===\n");
	DWORD xlsxLoadStartTime = GetTickCount();

	// 실행 파일 경로에서 XLSX 파일 경로 생성
	TCHAR szXlsxModulePath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, szXlsxModulePath, MAX_PATH);
	CString xlsxPath = szXlsxModulePath;
	int lastSlash = xlsxPath.ReverseFind(_T('\\'));
	if (lastSlash >= 0) {
		xlsxPath = xlsxPath.Left(lastSlash + 1);
	}
	xlsxPath += _T("EVMQTT_Tags.xlsx");

	TRACE("XLSX 파일 경로: %s\n", (LPCTSTR)xlsxPath);

	// XLSX 파일 로드
	if (!g_xlsxConfig.LoadFromXlsx(xlsxPath)) {
		TRACE("ERROR: XLSX 설정 파일 로드 실패!\n");
		TRACE("파일을 확인하세요: %s\n", (LPCTSTR)xlsxPath);

		// 로드 실패해도 계속 진행 (기존 INI 방식으로 fallback 가능)
		// return -1;  // 필요시 주석 해제
	}
	else {
		DWORD xlsxLoadTime = GetTickCount() - xlsxLoadStartTime;
		TRACE("=== XLSX 설정 파일 로드 완료 (%d ms) ===\n", xlsxLoadTime);
		TRACE("Subscribe 설정: %d개\n", g_xlsxConfig.GetSubConfigCount());
		TRACE("Publish 설정: %d개\n", g_xlsxConfig.GetPubConfigCount());
	}

	// 워커 스레드들 생성
	try {
		CreateWorkerThreads();
		TRACE("CreateWorkerThreads() call completed\n");
	}
	catch (const std::exception& e) {
		TRACE("Exception in CreateWorkerThreads(): %s\n", e.what());
	}
	catch (...) {
		TRACE("Unknown exception in CreateWorkerThreads()\n");
	}

	// MQTT 초기화
	DWORD dwCur = GetTickCount();
	DWORD dwOld = dwCur;
	int nErrorCode = 1;
	int nNetworkLoop;

	TRACE("=== TagInfoCache PreloadAllTags Starting ===\n");
	DWORD cacheLoadStartTime = GetTickCount();

	std::map<CString, CString> tagMappings = configManager.GetAllTagMappings();
	g_tagCache.PreloadAllTags(tagMappings);

	DWORD cacheLoadTime = GetTickCount() - cacheLoadStartTime;
	TRACE("=== TagInfoCache PreloadAllTags Completed in %d ms ===\n", cacheLoadTime);

	TRACE("=== JsonPathTokenCache PreloadAllJsonPaths Starting ===\n");
	DWORD jsonPathCacheStartTime = GetTickCount();

	g_jsonPathCache.PreloadAllJsonPaths(tagMappings);

	DWORD jsonPathCacheLoadTime = GetTickCount() - jsonPathCacheStartTime;
	TRACE("=== JsonPathTokenCache PreloadAllJsonPaths Completed in %d ms ===\n", jsonPathCacheLoadTime);

	// UI에 캐시 로딩 완료 알림
	if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
	{
		CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;
		CString cacheMsg;
		cacheMsg.Format(_T("캐시 로딩 완료: 태그 %d개 (%dms), JSONPath (%dms)"),
			tagMappings.size(), cacheLoadTime, jsonPathCacheLoadTime);
		pDlg->AddActivityLog(_T("시스템"), cacheMsg, ActivityLogItem::LOG_INFO, _T("준비"));
	}

	CString mqttIp = configManager.GetMqttIp();
	int mqttPort = configManager.GetMqttPort();
	int mqttKeepAlive = configManager.GetMqttKeepAlive();
	CString subscribeTopic = configManager.GetSubscribeTopic();

	// UI에 초기 연결 시도 알림
	if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
	{
		CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;
		pDlg->AddActivityLog(_T("MQTT"), mqttIp, ActivityLogItem::LOG_CONNECTION, _T("연결중"));
	}

	CT2A hostA(mqttIp);
	CT2A topicA(subscribeTopic);
	char* mqtt_host = strdup(hostA);
	char* mqtt_topic = strdup(topicA);
	int mqtt_port = mqttPort;
	int mqtt_keepalive = mqttKeepAlive;

	bool clean_session = true;
	struct mosquitto* mosq = NULL;

	// MQTT 클라이언트 ID 생성
	// EVMQTT1.exe → "EVMQTT1_Client"
	// EVMQTT2.exe → "EVMQTT2_Client"
	// 목적: 여러 인스턴스 실행 시 충돌 방지
	TCHAR szModulePath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, szModulePath, MAX_PATH);
	
	CString strModulePath(szModulePath);
	int nPos = strModulePath.ReverseFind(_T('\\'));
	CString clientId = _T("EVMQTT_Client");  // 기본값
	
	if (nPos > 0) {
		CString exeName = strModulePath.Mid(nPos + 1);
		int dotPos = exeName.ReverseFind(_T('.'));
		if (dotPos > 0) {
			exeName = exeName.Left(dotPos);
		}
		clientId = exeName + _T("_Client");
	}
	
	CT2A clientIdA(clientId);
	char* mqtt_client_id = strdup(clientIdA);
	TRACE("MQTT Client ID: %s\n", mqtt_client_id);

	// Mosquitto 초기화 (고유 클라이언트 ID 사용)
	mosquitto_lib_init();
	mosq = mosquitto_new(mqtt_client_id, clean_session, NULL);

	if (!mosq) {
		TRACE("mosquitto structure creation failed\n");
		nErrorCode = -1;

		// UI에 오류 알림
		if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
		{
			CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;
			pDlg->AddActivityLog(_T("MQTT"), _T("초기화 실패"), ActivityLogItem::LOG_ERROR, _T("실패"));
		}
	}
	else {
		TRACE("mosquitto structure creation successful\n");

		// 콜백 함수 등록 (obj에 this 포인터 전달)
		mosquitto_connect_callback_set(mosq, connect_callback);
		mosquitto_disconnect_callback_set(mosq, disconnect_callback);
		mosquitto_message_callback_set(mosq, message_callback);
		mosquitto_subscribe_callback_set(mosq, subscribe_callback);

		// 사용자 데이터 설정 (this 포인터)
		mosquitto_user_data_set(mosq, this);
	}

	TRACE("MQTT broker connection attempt: %s:%d\n", mqtt_host, mqtt_port);

	if (mosquitto_connect(mosq, mqtt_host, mqtt_port, mqtt_keepalive)) {
		TRACE("MQTT broker connection failed\n");
		nErrorCode = -2;

		// UI에 연결 실패 알림
		if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
		{
			CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;
			pDlg->OnMqttConnectionChanged(false);
			pDlg->AddActivityLog(_T("MQTT"), _T("연결 실패"), ActivityLogItem::LOG_ERROR, _T("실패"));
		}
	}
	else {
		TRACE("MQTT broker connection request sent\n");
	}

	// ===== INI 파일의 [TAGMAPPING]에 설정된 토픽만 개별 구독 =====
	// 목적: EVMQTT_N.exe로 분산 실행 시 설정된 토픽만 구독 (UI 업데이트 실패 방지)
	TRACE("=== MQTT Individual Topic Subscription Started ===\n");
	TRACE("Total tag mappings: %d\n", tagMappings.size());

	int successCount = 0;
	int failCount = 0;
	std::set<CString> uniqueTopics;  // 중복 토픽 제거용

	// tagMappings 형식: "태그명" → "토픽, JSONPath"
	// 예: "AI_TAG_01" → "test1, $.data.payload..."
	for (const auto& mapping : tagMappings) {
		const CString& tagName = mapping.first;
		const CString& tagMapping = mapping.second;

		// 토픽과 JSONPath 분리
		int commaPos = tagMapping.Find(_T(","));
		if (commaPos > 0) {
			CString topic = tagMapping.Left(commaPos);
			topic.Trim();

			// 와일드카드 토픽("+")은 구독하지 않음
			if (topic != _T("+") && !topic.IsEmpty()) {
				uniqueTopics.insert(topic);
			}
		}
	}

	TRACE("Unique topics to subscribe: %d\n", uniqueTopics.size());

	// 중복 제거된 토픽들 구독
	for (const auto& topic : uniqueTopics) {
		CT2A topicA(topic);

		int result = mosquitto_subscribe(mosq, NULL, topicA, 0);
		if (result == MOSQ_ERR_SUCCESS) {
			successCount++;
			TRACE("  [%d/%d] Subscribe OK: %s\n", successCount, uniqueTopics.size(), (const char*)topicA);
		}
		else {
			failCount++;
			TRACE("  [FAIL %d] Subscribe failed: %s (error: %d)\n", failCount, (const char*)topicA, result);
		}
	}

	TRACE("MQTT Subscription Completed: Success=%d, Failed=%d\n", successCount, failCount);

	// UI에 구독 완료 알림
	if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
	{
		CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;
		CString subMsg;
		subMsg.Format(_T("토픽 구독: %d개 성공"), successCount);
		pDlg->AddActivityLog(_T("MQTT"), subMsg, ActivityLogItem::LOG_INFO, _T("구독완료"));
	}

	// mosquitto_loop_start 사용 (백그라운드 스레드에서 네트워크 I/O 처리)
	TRACE("Starting Mosquitto background thread\n");
	int loopStartResult = mosquitto_loop_start(mosq);
	if (loopStartResult != MOSQ_ERR_SUCCESS) {
		TRACE("ERROR: mosquitto_loop_start failed with code: %d\n", loopStartResult);

		// UI에 오류 알림
		if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
		{
			CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;
			pDlg->AddActivityLog(_T("MQTT"), _T("백그라운드 스레드 시작 실패"), ActivityLogItem::LOG_ERROR, _T("실패"));
		}

		nErrorCode = -3;
	}
	else {
		TRACE("Mosquitto background thread started successfully\n");
	}

	// 메인 루프 (상태 모니터링 및 통계)
	TRACE("Main loop started\n");
	while (!m_bEndThread)
	{
		dwCur = GetTickCount();

		// 주기적 상태 체크 (5초마다)
		if (dwCur - dwOld > 5000) {
			dwOld = dwCur;

			// 큐 상태 출력
			if (m_pMessageQueue) {
				size_t queueSize = m_pMessageQueue->Size();
				TRACE("Queue status check - Size: %d, Total processed: %d\n",
					queueSize, GetTotalProcessedCount());

				// 큐가 너무 커지면 경고
				if (queueSize > 3000) {
					TRACE("Warning: Message queue very large! (%d)\n", queueSize);

					// UI에 경고 알림
					if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
					{
						CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;
						CString queueSizeMsg;
						queueSizeMsg.Format(_T("큐 크기: %d"), queueSize);
						pDlg->AddActivityLog(_T("시스템"), queueSizeMsg, ActivityLogItem::LOG_ERROR, _T("경고"));
					}
				}
			}

			// 통계 업데이트
			UpdateStats(GetTotalProcessedCount(), m_nTotalCount);

			// 워커 스레드 상태 출력
			PrintThreadStatus();

			// TagInfoCache 통계
			CTagInfoCache::CacheStats cacheStats;
			g_tagCache.GetCacheStats(cacheStats);
			TRACE("TagInfoCache Stats: Hit Rate %.2f%%, Size %d\n",
				cacheStats.hitRate, cacheStats.totalSize);

			// JsonPathTokenCache 통계
			CJsonPathTokenCache::CacheStats pathCacheStats;
			g_jsonPathCache.GetCacheStats(pathCacheStats);
			TRACE("JsonPathTokenCache Stats: Hit Rate %.2f%%, Size %d\n",
				pathCacheStats.hitRate, pathCacheStats.totalSize);
		}

		Sleep(100);
	}

	// 정리 작업
	TRACE("Main loop terminated, starting cleanup\n");

	// mosquitto_loop_stop 호출
	if (mosq) {
		TRACE("Stopping Mosquitto background thread...\n");
		int stopResult = mosquitto_loop_stop(mosq, true);  // force=true
		if (stopResult != MOSQ_ERR_SUCCESS) {
			TRACE("WARNING: mosquitto_loop_stop failed with code: %d\n", stopResult);
		}
		else {
			TRACE("Mosquitto background thread stopped successfully\n");
		}

		mosquitto_disconnect(mosq);
		mosquitto_destroy(mosq);
		TRACE("MQTT connection closed\n");
	}
	mosquitto_lib_cleanup();

	if (mqtt_host) free(mqtt_host);
	if (mqtt_topic) free(mqtt_topic);
	if (mqtt_client_id) free(mqtt_client_id);

	TRACE("RunSubscribeMode Terminated\n");
	return 0;
}

void CThreadSub::CreateWorkerThreads()
{
	TRACE("Worker Thread Creation Started\n");
	TRACE("m_workerThreadCount = %d\n", m_workerThreadCount);
	TRACE("m_pMessageQueue = %p\n", m_pMessageQueue);
	TRACE("m_pOwner = %p\n", m_pOwner);

	m_workerThreads.clear();

	for (int i = 0; i < m_workerThreadCount; i++)
	{
		TRACE("Creating worker thread %d...\n", i + 1);

		try {
			CMqttWorkerThread* pWorker = (CMqttWorkerThread*)AfxBeginThread(
				RUNTIME_CLASS(CMqttWorkerThread),
				THREAD_PRIORITY_NORMAL,
				0,
				CREATE_SUSPENDED  // 중단된 상태로 생성
			);

			if (pWorker) {
				TRACE("Worker thread %d object created, setting properties...\n", i + 1);

				// 스레드 시작 전에 모든 속성 설정
				pWorker->SetMessageQueue(m_pMessageQueue);
				pWorker->SetOwner(m_pOwner);
				pWorker->SetWorkerID(i + 1);
				pWorker->SetBatchSize(20);   // 최적화: 30→20
				pWorker->SetBatchTimeout(20); // 최적화: 100→20ms

				// AddRef() 제거 - 일반 포인터이므로 불필요
				// m_pMessageQueue->AddRef();  // 이 줄 제거

				TRACE("Worker thread %d properties set, resuming thread...\n", i + 1);

				// 스레드 시작 - 에러 처리 강화
				DWORD resumeResult = pWorker->ResumeThread();
				TRACE("Worker thread %d ResumeThread result: %d\n", i + 1, resumeResult);

				if (resumeResult == 0xFFFFFFFF) {
					TRACE("CRITICAL ERROR: ResumeThread failed for worker %d (Error: %d)\n", i + 1, GetLastError());
					
					// 실패한 스레드 정리
					delete pWorker;
					continue; // 다음 스레드 생성 시도
				}

				// 스레드가 실제로 시작될 시간을 더 충분히 제공
				Sleep(20);

				// 스레드 상태 확인
				DWORD exitCode;
				if (GetExitCodeThread(pWorker->m_hThread, &exitCode)) {
					if (exitCode == STILL_ACTIVE) {
						TRACE("SUCCESS: Worker thread %d is running (STILL_ACTIVE)\n", i + 1);
						
						// 추가 검증: 스레드가 실제로 동작하는지 확인
						if (pWorker->m_hThread && pWorker->m_nThreadID > 0) {
							TRACE("Worker thread %d validation passed (TID: %d)\n", i + 1, pWorker->m_nThreadID);
						}
						else {
							TRACE("WARNING: Worker thread %d has invalid handle or ID\n", i + 1);
						}
					}
					else {
						TRACE("CRITICAL ERROR: Worker thread %d already exited with code %d\n", i + 1, exitCode);
						
						// 실패한 스레드 정리 - 재시도 로직 추가
						delete pWorker;
						
						// 한 번 더 시도
						TRACE("Retrying worker thread %d creation...\n", i + 1);
						i--; // 인덱스 되돌려서 재시도
						continue;
					}
				}
				else {
					TRACE("CRITICAL ERROR: Cannot get thread status for worker %d (Error: %d)\n", i + 1, GetLastError());
					
					// 상태 확인 실패한 스레드도 정리
					delete pWorker;
					continue;
				}

				m_workerThreads.push_back(pWorker);

				TRACE("Worker thread %d created successfully (TID: %d)\n",
					i + 1, pWorker->m_nThreadID);
			}
			else {
				TRACE("ERROR: AfxBeginThread returned NULL for worker %d\n", i + 1);
			}
		}
		catch (const std::exception& e) {
			TRACE("Exception during worker thread %d creation: %s\n", i + 1, e.what());
		}
		catch (...) {
			TRACE("Unknown exception during worker thread %d creation\n", i + 1);
		}
	}

	TRACE("Worker thread creation completed - Total: %d (Target: %d)\n", m_workerThreads.size(), m_workerThreadCount);

	// 최소한의 워커 스레드가 생성되었는지 검증
	if (m_workerThreads.size() == 0) {
		TRACE("CRITICAL ERROR: No worker threads were created successfully!\n");
		// UI에 오류 알림
		if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
		{
			CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;
			pDlg->AddActivityLog(_T("시스템"), _T("Worker Thread 생성 실패"), ActivityLogItem::LOG_ERROR, _T("실패"));
		}
	}
	else if (m_workerThreads.size() < m_workerThreadCount / 2) {
		TRACE("WARNING: Only %d out of %d worker threads created successfully!\n", 
			m_workerThreads.size(), m_workerThreadCount);
		
		// UI에 경고 알림
		if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
		{
			CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;
			CString warningMsg;
			warningMsg.Format(_T("Worker Thread %d/%d개만 생성됨"), m_workerThreads.size(), m_workerThreadCount);
			pDlg->AddActivityLog(_T("시스템"), warningMsg, ActivityLogItem::LOG_ERROR, _T("경고"));
		}
	}
	else {
		TRACE("SUCCESS: Worker threads created successfully (%d/%d)\n", 
			m_workerThreads.size(), m_workerThreadCount);
			
		// UI에 성공 알림
		if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
		{
			CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;
			CString successMsg;
			successMsg.Format(_T("Worker Thread % d개 생성 완료"), m_workerThreads.size());
			pDlg->AddActivityLog(_T("시스템"), successMsg, ActivityLogItem::LOG_INFO, _T("성공"));
		}
	}

	// 모든 워커가 실제로 시작되었는지 한번 더 확인
	Sleep(10);  // 더 충분한 시간 제공
	TRACE("Final check - verifying all workers are still running...\n");

	for (size_t i = 0; i < m_workerThreads.size(); i++) {
		CMqttWorkerThread* pWorker = m_workerThreads[i];
		if (pWorker) {
			DWORD exitCode;
			if (GetExitCodeThread(pWorker->m_hThread, &exitCode)) {
				if (exitCode == STILL_ACTIVE) {
					TRACE("Worker %d: Still running\n", i + 1);
				}
				else {
					TRACE("WARNING: Worker %d has exited with code %d\n", i + 1, exitCode);
				}
			}
		}
	}
}

void CThreadSub::DestroyWorkerThreads()
{
	// 워커 스레드가 없으면 아무것도 하지 않음
	if (m_workerThreads.empty()) {
		TRACE("No worker threads to destroy\n");
		return;
	}

	TRACE("Worker Thread Termination Started (%d threads)\n", m_workerThreads.size());

	// Send shutdown signal to message queue
	if (m_pMessageQueue) {
		m_pMessageQueue->Shutdown();
		TRACE("Message queue shutdown signal sent\n");
	}

	// Send termination signal to all worker threads
	for (auto* pWorker : m_workerThreads)
	{
		if (pWorker) {
			pWorker->Stop();
		}
	}

	// Wait for worker threads to terminate
	for (int i = 0; i < (int)m_workerThreads.size(); i++)
	{
		CMqttWorkerThread* pWorker = m_workerThreads[i];
		if (!pWorker) continue;

		TRACE("Waiting for worker thread %d to terminate...\n", i + 1);

		DWORD dwExitCode;
		int waitCount = 0;
		const int MAX_WAIT_COUNT = 100; // Wait 10 seconds (100 * 100ms)

		while (waitCount < MAX_WAIT_COUNT)
		{
			if (GetExitCodeThread(pWorker->m_hThread, &dwExitCode))
			{
				if (dwExitCode != STILL_ACTIVE) {
					TRACE("Worker thread %d terminated normally\n", i + 1);
					break;
				}
			}
			else {
				TRACE("Worker thread %d status check failed\n", i + 1);
				break;
			}

			Sleep(10);
			waitCount++;
		}

		if (waitCount >= MAX_WAIT_COUNT) {
			TRACE("Warning: Worker thread %d termination timeout\n", i + 1);
		}

		// Release() 제거 - 일반 포인터이므로 불필요
		// if (pWorker->m_pMessageQueue) {
		//     pWorker->m_pMessageQueue->Release();  // 이 줄 제거
		// }

		// Memory cleanup
		delete pWorker;
	}

	m_workerThreads.clear();
	TRACE("Worker thread termination completed\n");
}

void CThreadSub::PrintThreadStatus()
{
	if (m_workerThreads.empty()) return;

	TRACE("=== Thread Status ===\n");
	TRACE("Message queue size: %d\n", GetTotalQueueSize());
	TRACE("Total processed messages: %d\n", GetTotalProcessedCount());

	for (int i = 0; i < (int)m_workerThreads.size(); i++)
	{
		CMqttWorkerThread* pWorker = m_workerThreads[i];
		if (pWorker) {
			TRACE("Worker %d: Processed=%d, Success=%d, Failed=%d\n",
				i + 1,
				pWorker->GetProcessedCount(),
				pWorker->GetSuccessCount(),
				pWorker->GetErrorCount());
		}
	}
	TRACE("=====================\n");
}

int CThreadSub::GetTotalQueueSize() const
{
	return m_pMessageQueue ? (int)m_pMessageQueue->Size() : 0;
}

int CThreadSub::GetTotalProcessedCount() const
{
	int totalProcessed = 0;
	for (const auto* pWorker : m_workerThreads)
	{
		if (pWorker) {
			totalProcessed += pWorker->GetProcessedCount();
		}
	}
	return totalProcessed;
}

// Navifra 모드: MQTT Publish
int CThreadSub::RunPublishMode()
{
	TRACE("RunPublishMode Started\n");

	CConfigManager& configManager = CConfigManager::GetInstance();

	// MQTT 설정 로드
	CString mqttIp = configManager.GetMqttIp();
	int mqttPort = configManager.GetMqttPort();
	int mqttKeepAlive = configManager.GetMqttKeepAlive();

	TRACE("MQTT Broker: %s:%d\n", (LPCTSTR)mqttIp, mqttPort);

	// UI에 초기 연결 시도 알림
	if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
	{
		CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;
		pDlg->AddActivityLog(_T("MQTT"), mqttIp, ActivityLogItem::LOG_CONNECTION, _T("연결중"));
	}

	// MQTT 클라이언트 ID 생성
	TCHAR szModulePath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, szModulePath, MAX_PATH);

	CString strModulePath(szModulePath);
	int nPos = strModulePath.ReverseFind(_T('\\'));
	CString clientId = _T("EVMQTT_Client");

	if (nPos > 0) {
		CString exeName = strModulePath.Mid(nPos + 1);
		int dotPos = exeName.ReverseFind(_T('.'));
		if (dotPos > 0) {
			exeName = exeName.Left(dotPos);
		}
		clientId = exeName + _T("_Client");
	}

	CT2A clientIdA(clientId);
	CT2A hostA(mqttIp);

	TRACE("MQTT Client ID: %s\n", (const char*)clientIdA);

	// Mosquitto 초기화
	mosquitto_lib_init();
	struct mosquitto* mosq = mosquitto_new(clientIdA, true, NULL);

	if (!mosq) {
		TRACE("mosquitto structure creation failed\n");

		if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
		{
			CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;
			pDlg->AddActivityLog(_T("MQTT"), _T("초기화 실패"), ActivityLogItem::LOG_ERROR, _T("실패"));
		}

		mosquitto_lib_cleanup();
		return -1;
	}

	// 콜백 설정
	mosquitto_connect_callback_set(mosq, connect_callback);
	mosquitto_disconnect_callback_set(mosq, disconnect_callback);
	mosquitto_user_data_set(mosq, this);

	// MQTT 브로커 연결
	TRACE("Connecting to MQTT broker: %s:%d\n", (const char*)hostA, mqttPort);
	if (mosquitto_connect(mosq, hostA, mqttPort, mqttKeepAlive)) {
		TRACE("MQTT broker connection failed\n");

		if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
		{
			CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;
			pDlg->OnMqttConnectionChanged(false);
			pDlg->AddActivityLog(_T("MQTT"), _T("연결 실패"), ActivityLogItem::LOG_ERROR, _T("실패"));
		}

		mosquitto_destroy(mosq);
		mosquitto_lib_cleanup();
		return -2;
	}

	// mosquitto_loop_start() 시작
	TRACE("Starting Mosquitto background thread...\n");
	int loopStartResult = mosquitto_loop_start(mosq);
	if (loopStartResult != MOSQ_ERR_SUCCESS) {
		TRACE("ERROR: mosquitto_loop_start failed with code: %d\n", loopStartResult);

		if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
		{
			CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;
			pDlg->AddActivityLog(_T("MQTT"), _T("백그라운드 스레드 시작 실패"), ActivityLogItem::LOG_ERROR, _T("실패"));
		}

		mosquitto_disconnect(mosq);
		mosquitto_destroy(mosq);
		mosquitto_lib_cleanup();
		return -3;
	}

	TRACE("Mosquitto background thread started successfully\n");

	// PubTagMapping 로드
	std::map<CString, CString> pubTagMappings = configManager.GetAllPubTagMappings();
	TRACE("Loaded %d PubTagMapping entries\n", pubTagMappings.size());

	if (pubTagMappings.empty()) {
		TRACE("WARNING: No PubTagMapping found\n");
		if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
		{
			CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;
			pDlg->AddActivityLog(_T("시스템"), _T("PubTagMapping이 비어있습니다"), ActivityLogItem::LOG_ERROR, _T("경고"));
		}
	}
	else {
		for (const auto& mapping : pubTagMappings) {
			TRACE("  %s = %s\n", (LPCTSTR)mapping.first, (LPCTSTR)mapping.second);
		}
	}

	// EasyView 출력 제어 Shared Memory 생성
	TRACE("Creating Output Control Map\n");
	int outputMapResult = CreateOutputMap();
	if (outputMapResult > 0) {
		TRACE("Output control map created successfully\n");
	if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
	{
		CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;
			pDlg->AddActivityLog(_T("시스템"), _T("출력 제어 준비 완료"), ActivityLogItem::LOG_INFO, _T("성공"));
		}
	}
	else {
		TRACE("WARNING: Output control map creation failed: %d\n", outputMapResult);
		if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
		{
			CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;
			pDlg->AddActivityLog(_T("시스템"), _T("출력 제어 생성 실패"), ActivityLogItem::LOG_ERROR, _T("실패"));
		}
	}

	// 메인 출력 처리 루프
	TRACE("Output Processing Loop Started\n");
	int outputCount = 0;

	while (!m_bEndThread)
	{
		// ===== EasyView 출력 명령 처리 =====
		if (m_pCtrlQueue != NULL) {
			ST_CONTROL2 stCtrl;
			int getResult = GetControl(&stCtrl);
			if (getResult > 0) {
				ProcessOutput(&stCtrl, mosq, pubTagMappings);
				outputCount++;
			}
		}

		// 통계 출력 (5초마다)
		static DWORD dwLastStats = 0;
		DWORD dwCur = GetTickCount();
		if (dwCur - dwLastStats > 5000) {
			dwLastStats = dwCur;
			TRACE("Output Stats: Total=%d\n", outputCount);
		}

		Sleep(10);  // CPU 사용률 감소
	}

	// 정리 작업
	TRACE("Output processing loop terminated, starting cleanup\n");

	if (mosq) {
		TRACE("Stopping Mosquitto background thread...\n");
		mosquitto_loop_stop(mosq, true);
		mosquitto_disconnect(mosq);
		mosquitto_destroy(mosq);
		TRACE("MQTT connection closed\n");
	}

	mosquitto_lib_cleanup();

	TRACE("RunPublishMode Terminated\n");
	return 0;
}

// EasyView 출력 제어 기능 구현

int CThreadSub::CreateOutputMap()
{
	SECURITY_ATTRIBUTES sa;
	CString szShareM, szShareS;

	if (m_hCtrlQueue)
		return 1;

	if (NULL == m_psd)
	{
		m_psd = (SECURITY_DESCRIPTOR*)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
		InitializeSecurityDescriptor(m_psd, SECURITY_DESCRIPTOR_REVISION);
		SetSecurityDescriptorDacl(m_psd, TRUE, NULL, FALSE);
	}

	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = m_psd;
	sa.bInheritHandle = TRUE;

	// ===== 전역 변수에서 프로젝트 이름 가져오기 =====
	// EasyView 엔진이 커맨드 라인으로 전달한 프로젝트 이름 사용
	extern CString g_szProjectName;

	CString projectName = g_szProjectName;
	if (projectName.IsEmpty())
	{
		// 전역 변수가 비어있으면 INI 파일에서 읽기 (스탠드얼론 모드)
		char szBuff[256] = { 0 };
		char szProjectName[256] = { 0 };
		EV_GetConfigFile(szBuff);
		::GetPrivateProfileString(
			"EasyView", "Project", "EVMQTT", szProjectName,
			sizeof(szProjectName), szBuff
		);
		projectName = CString(szProjectName);
		projectName.MakeUpper();
	}

	szShareM.Format(_T("EVSM_%s.EVMQTT"), projectName);
	szShareS.Format(_T("EVSS_%s.EVMQTT"), projectName);

	szShareM.MakeUpper();
	szShareS.MakeUpper();

	TRACE("CreateOutputMap: Project=%s, ShareM=%s, ShareS=%s\n", 
		(LPCTSTR)projectName, (LPCTSTR)szShareM, (LPCTSTR)szShareS);

	m_hCtrlQueue = CreateFileMapping((HANDLE)0xFFFFFFFF, &sa, PAGE_READWRITE, 0, 
		sizeof(ST_CONTROL_QUEUE), szShareM);
	if (NULL == m_hCtrlQueue)
	{
		TRACE("CreateFileMapping failed: %d\n", GetLastError());
		return -1;
	}

	// 이미 존재하는지 체크
	BOOL bAlreadyExists = (GetLastError() == ERROR_ALREADY_EXISTS);

	m_pCtrlQueue = (ST_CONTROL_QUEUE*)MapViewOfFile(m_hCtrlQueue, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (NULL == m_pCtrlQueue)
	{
		CloseHandle(m_hCtrlQueue);
		m_hCtrlQueue = NULL;
		TRACE("MapViewOfFile failed: %d\n", GetLastError());
		return -1;
	}

	// 큐 초기화
	if (bAlreadyExists)
	{
		TRACE("Output map already exists: Head=%d, Tail=%d\n", m_pCtrlQueue->nHead, m_pCtrlQueue->nTail);
	}
	else
	{
		TRACE("Output map created new - Initializing queue\n");
		m_pCtrlQueue->nHead = 0;
		m_pCtrlQueue->nTail = 0;
		memset(m_pCtrlQueue->Output, 0, sizeof(m_pCtrlQueue->Output));
	}

	m_hCtrlSync = CreateMutex(&sa, FALSE, szShareS);
	if (NULL == m_hCtrlSync)
	{
		UnmapViewOfFile(m_pCtrlQueue);
		CloseHandle(m_hCtrlQueue);

		m_pCtrlQueue = NULL;
		m_hCtrlQueue = NULL;
		TRACE("CreateMutex failed: %d\n", GetLastError());
		return -1;
	}

	TRACE("Output map created successfully\n");
	return 1;
}

void CThreadSub::CloseOutputMap()
{
	if (m_pCtrlQueue)
	{
		UnmapViewOfFile(m_pCtrlQueue);
		m_pCtrlQueue = NULL;
	}
	if (m_hCtrlQueue)
	{
		CloseHandle(m_hCtrlQueue);
		m_hCtrlQueue = NULL;
	}
	if (m_hCtrlSync)
	{
		CloseHandle(m_hCtrlSync);
		m_hCtrlSync = NULL;
	}
	if (m_psd)
	{
		LocalFree(m_psd);
		m_psd = NULL;
	}
}

int CThreadSub::GetControl(ST_CONTROL2* pCtrl)
{
	int nHead, nTail;

	if (m_hCtrlSync == NULL)
	{
		static int errorCount = 0;
		if (++errorCount % 1000 == 0) {
			TRACE("GetControl: Mutex is NULL (count=%d)\n", errorCount);
		}
		return -1;
	}

	DWORD dwSync = WaitForSingleObject(m_hCtrlSync, 0);
	if (dwSync != WAIT_OBJECT_0)
	{
		// Mutex가 다른 프로세스에 잡혀있음 (정상 - EasyView가 쓰는 중)
		static int lockedCount = 0;
		if (++lockedCount % 5000 == 0) {
			TRACE("GetControl: Mutex locked by another process (count=%d)\n", lockedCount);
		}
		return -2;
	}

	nHead = m_pCtrlQueue->nHead;
	nTail = m_pCtrlQueue->nTail;
	if (nHead == nTail)
	{
		// 큐가 비어있음 (정상 - 출력 명령 없음)
		static int emptyCount = 0;
		if (++emptyCount % 5000 == 0) {
			TRACE("GetControl: Queue empty, Head=%d, Tail=%d (count=%d)\n", nHead, nTail, emptyCount);
		}
		ReleaseMutex(m_hCtrlSync);
		return -3;
	}

	// 출력 명령 수신
	memcpy(pCtrl, &m_pCtrlQueue->Output[nHead], sizeof(ST_CONTROL2));

	TRACE("GetControl: Command received - Tag='%s', Type=%d\n",
		pCtrl->CtrlCommon.szTag, pCtrl->CtrlCommon.nTagType);

	(++nHead) %= EV_MAX_CONTROL_QUEUE;
	m_pCtrlQueue->nHead = nHead;

	ReleaseMutex(m_hCtrlSync);

	return 1;
}

void CThreadSub::ProcessOutput(ST_CONTROL2* pCtrl, struct mosquitto* mosq, const std::map<CString, CString>& pubTagMappings)
{
	if (!pCtrl || !mosq)
		return;

	ST_CTRL_COMMON* pCtrlCommon = &pCtrl->CtrlCommon;
	ST_CTRL_AO* pCtrlAo = &pCtrl->CtrlAo;
	ST_CTRL_DO* pCtrlDo = &pCtrl->CtrlDo;

	// szTag(태그명) 사용
	CString tagName = CString(pCtrlCommon->szTag);

	// PubTagMapping에서 해당 태그 찾기
	auto it = pubTagMappings.find(tagName);
	if (it == pubTagMappings.end()) {
		return;
	}

	// 매핑 형식: "토픽,JSON양식" 또는 "토픽,Station태그명" (Navifra 모드)
	CString mapping = it->second;
	int commaPos = mapping.Find(_T(","));
	if (commaPos <= 0) {
		CLogManager& logManager = CLogManager::GetInstance();
		CString logMsg;
		logMsg.Format(_T("Tag: %s, Invalid mapping format: %s"), tagName, mapping);
		logManager.WriteErrorLog(_T("OUTPUT_ERROR"), _T("ProcessOutput"), logMsg, _T("잘못된 매핑 형식"));
		return;
	}

	CString topic = mapping.Left(commaPos);
	CString jsonTemplate = mapping.Mid(commaPos + 1);
	topic.Trim();
	jsonTemplate.Trim();

	// Device Type 확인
	CConfigManager& configManager = CConfigManager::GetInstance();
	CString deviceType = configManager.GetDeviceType();

	CString jsonPayload, logType;
	CLogManager& logManager = CLogManager::GetInstance();

	// ===== Navifra 모드 전용 처리 =====
	if (deviceType.CompareNoCase(_T("Navifra")) == 0)
	{
		// jsonTemplate은 Station 태그명
		CString stationTagName = jsonTemplate;

		// DI/DO 출력만 처리 (status 전송용)
		if (pCtrlCommon->nTagType == TYPE_DI || pCtrlCommon->nTagType == TYPE_DO)
		{
			int statusValue = pCtrlDo->nOn;
			CString statusStr = (statusValue != 0) ? _T("true") : _T("false");

			// Station 태그(AI)에서 값 읽기
			ST_EV_TAG_INFO stationTagInfo;
			int tagResult = EV_GetTagInfo(stationTagName, &stationTagInfo);
			
			if (tagResult <= 0)
			{
				CString logMsg;
				logMsg.Format(_T("Tag: %s, Station Tag not found: %s"), tagName, stationTagName);
				logManager.WriteErrorLog(_T("OUTPUT_ERROR"), _T("ProcessOutput"), logMsg, _T("Station 태그 없음"));
				return;
			}

			// Station 값 읽기 (AI 타입만 지원)
			int stationValue = 0;
			if (stationTagInfo.nTagType == TYPE_AI || stationTagInfo.nTagType == TYPE_AO)
			{
				double aiValue = 0.0;
				int result = EV_GetAiData(stationTagInfo.nStnPos, stationTagInfo.nTagPos, &aiValue);
				if (result > 0)
				{
					stationValue = (int)aiValue;
				}
				else
				{
					CString logMsg;
					logMsg.Format(_T("Tag: %s, Failed to read Station Tag: %s"), tagName, stationTagName);
					logManager.WriteErrorLog(_T("OUTPUT_ERROR"), _T("ProcessOutput"), logMsg, _T("Station 값 읽기 실패"));
					return;
				}
			}
			else
			{
				CString logMsg;
				logMsg.Format(_T("Tag: %s, Station Tag must be AI/AO type: %s"), tagName, stationTagName);
				logManager.WriteErrorLog(_T("OUTPUT_ERROR"), _T("ProcessOutput"), logMsg, _T("Station 태그 타입 오류"));
				return;
			}

			// Navifra JSON 생성
			jsonPayload.Format(_T("{\"action\":\"complete\",\"station\":%d,\"status\":%s}"),
				stationValue, statusStr);

			CString logMsg;
			logMsg.Format(_T("Tag: %s, Status: %s, Station: %d (from %s)"),
				tagName, statusStr, stationValue, stationTagName);
			logManager.WriteErrorLog(_T("OUTPUT_NAVIFRA"), _T("ProcessOutput"), logMsg, topic);
		}
		else
		{
			CString logMsg;
			logMsg.Format(_T("Tag: %s, Navifra mode only supports DI/DO output"), tagName);
			logManager.WriteErrorLog(_T("OUTPUT_ERROR"), _T("ProcessOutput"), logMsg, _T("지원되지 않는 태그 타입"));
			return;
		}
	}
	// ===== 기존 모드 (IFM 등) 처리 =====
	else
	{
	// 태그 타입별 처리 (AI==AO, DI==DO)
	if (pCtrlCommon->nTagType == TYPE_AI || pCtrlCommon->nTagType == TYPE_AO)
	{
		double value = pCtrlAo->vValue;
		
		// 실제 태그 타입 구분
		CString typeStr = (pCtrlCommon->nTagType == TYPE_AI) ? _T("AI") : _T("AO");
		
		// JSONPath(필드명)가 지정되어 있으면 사용, 없으면 기본 형식
		if (!jsonTemplate.IsEmpty()) {
			// 정수로 변환 (소수점 제거)
			int intValue = (int)value;
			jsonPayload.Format(_T("{\"%s\":%d}"), jsonTemplate, intValue);
		}
		else {
			// 기본 형식
			jsonPayload.Format(_T("{\"tag\":\"%s\",\"type\":\"%s\",\"value\":%.2f}"),
				tagName, typeStr, value);
		}

		CString logMsg;
		logMsg.Format(_T("Tag: %s (Type: %s), Value: %.2f, Field: %s"), 
			tagName, typeStr, value, jsonTemplate.IsEmpty() ? _T("default") : jsonTemplate);
		logType.Format(_T("OUTPUT_%s"), typeStr);
		logManager.WriteErrorLog(logType, _T("ProcessOutput"), logMsg, topic);
	}
	else if (pCtrlCommon->nTagType == TYPE_DI || pCtrlCommon->nTagType == TYPE_DO)
	{
		int value = pCtrlDo->nOn;
		
		// 실제 태그 타입 구분
		CString typeStr = (pCtrlCommon->nTagType == TYPE_DI) ? _T("DI") : _T("DO");
		
		// JSONPath(필드명)가 지정되어 있으면 사용
		CString statusStr = (value != 0) ? _T("true") : _T("false");
		
		if (!jsonTemplate.IsEmpty()) {
			jsonPayload.Format(_T("{\"%s\":%s}"), jsonTemplate, statusStr);
		}
		else {
			// 기본 형식
			jsonPayload.Format(_T("{\"value\":%s}"), statusStr);
		}

		CString logMsg;
		logMsg.Format(_T("Tag: %s (Type: %s), Value: %d, Status: %s, Field: %s"), 
			tagName, typeStr, value, statusStr, jsonTemplate.IsEmpty() ? _T("default") : jsonTemplate);
		logType.Format(_T("OUTPUT_%s"), typeStr);
		logManager.WriteErrorLog(logType, _T("ProcessOutput"), logMsg, topic);
	}
	else
	{
		CString logMsg;
		logMsg.Format(_T("Tag: %s, Unsupported Type: %d"), tagName, pCtrlCommon->nTagType);
		logManager.WriteErrorLog(_T("OUTPUT_ERROR"), _T("ProcessOutput"), logMsg, _T("지원되지 않는 태그 타입"));
		return;
	}
	} // else (기존 모드 처리 끝)

	// ===== MQTT Publish (공통) =====
	CT2A topicA(topic, CP_UTF8);
	CT2A payloadA(jsonPayload, CP_UTF8);

	int result = mosquitto_publish(mosq, NULL, topicA, 
		strlen(payloadA), payloadA, 0, false);

	if (result != MOSQ_ERR_SUCCESS)
	{
		CString logMsg;
		logMsg.Format(_T("Tag: %s, MQTT Publish Failed, Error Code: %d"), tagName, result);
		logManager.WriteErrorLog(_T("OUTPUT_ERROR"), _T("ProcessOutput"), logMsg, topic);
	}
	else
	{
		CString logMsg;
		logMsg.Format(_T("Tag: %s, Payload: %s"), tagName, jsonPayload);
		logManager.WriteErrorLog(_T("OUTPUT_SUCCESS"), _T("ProcessOutput"), logMsg, topic);
	}
}
