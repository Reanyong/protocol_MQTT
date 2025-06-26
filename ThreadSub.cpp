// CThreadSub.cpp : implementation file
//

#include "pch.h"
#include "EVMQTT.h"
#include "EVMQTTDlg.h"
#include "ThreadSub.h"
#include "ConfigManager.h"
#include "JsonResultManager.h"
#include "MqttWorkerThread.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#pragma execution_character_set("utf-8")

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
    m_workerThreadCount = 3;
}

CThreadSub::~CThreadSub()
{
    DestroyWorkerThreads();

    if (m_pMessageQueue) {
        delete m_pMessageQueue;
        m_pMessageQueue = nullptr;
    }
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
#pragma comment(lib, "..\\Lib\\mosquitto.lib")

#define strdup _strdup

void connect_callback(struct mosquitto* mosq, void* obj, int result)
{
    CThreadSub* pThreadSub = static_cast<CThreadSub*>(obj);

    if (result == 0) {
        TRACE("=== MQTT Connection Success! ===\n");

        // UI에 연결 성공 알림
        if (pThreadSub && pThreadSub->m_pOwner && ::IsWindow(pThreadSub->m_pOwner->GetSafeHwnd()))
        {
            CEVMQTTDlg* pDlg = (CEVMQTTDlg*)pThreadSub->m_pOwner;
            pDlg->OnMqttConnectionChanged(true);
        }
    }
    else {
        TRACE("=== MQTT Connection Failed! Error code: %d ===\n", result);

        // UI에 연결 실패 알림
        if (pThreadSub && pThreadSub->m_pOwner && ::IsWindow(pThreadSub->m_pOwner->GetSafeHwnd()))
        {
            CEVMQTTDlg* pDlg = (CEVMQTTDlg*)pThreadSub->m_pOwner;
            pDlg->OnMqttConnectionChanged(false);
        }

        switch (result) {
        case 1: TRACE("Connection refused: bad protocol version\n"); break;
        case 2: TRACE("Connection refused: client ID rejected\n"); break;
        case 3: TRACE("Connection refused: server unavailable\n"); break;
        case 4: TRACE("Connection refused: bad username/password\n"); break;
        case 5: TRACE("Connection refused: not authorized\n"); break;
        default: TRACE("Connection refused: unknown error\n"); break;
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
    TRACE("=== ThreadSub Started ===\n");

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

    // 메시지 큐 생성
    TRACE("Creating message queue...\n");
    m_pMessageQueue = new CMqttMessageQueue(5000);
    TRACE("Message queue creation completed\n");

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

    CConfigManager& configManager = CConfigManager::GetInstance();
    configManager.LoadConfig();

    CString mqttIp = configManager.GetMqttIp();
    int mqttPort = configManager.GetMqttPort();
    int mqttKeepAlive = configManager.GetMqttKeepAlive();

    // UI에 초기 연결 시도 알림
    if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
    {
        CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;
        pDlg->AddActivityLog(_T("MQTT"), mqttIp, ActivityLogItem::LOG_CONNECTION, _T("연결중"));
    }

    CT2A hostA(mqttIp);
    char* mqtt_host = strdup(hostA);
    char* mqtt_topic = strdup("+");
    int mqtt_port = mqttPort;
    int mqtt_keepalive = mqttKeepAlive;

    bool clean_session = true;
    struct mosquitto* mosq = NULL;

    // Mosquitto 초기화
    mosquitto_lib_init();
    mosq = mosquitto_new(NULL, clean_session, NULL);

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

    TRACE("MQTT topic subscription attempt: '%s'\n", mqtt_topic);
    int subscribe_result = mosquitto_subscribe(mosq, NULL, mqtt_topic, 0);
    if (subscribe_result == MOSQ_ERR_SUCCESS) {
        TRACE("MQTT subscription request successful\n");
    }
    else {
        TRACE("MQTT subscription request failed: %d\n", subscribe_result);
    }

    // 메인 루프
    TRACE("Main loop started\n");
    while (!m_bEndThread)
    {
        dwCur = GetTickCount();

        // MQTT 메시지 처리 (논블로킹)
        nNetworkLoop = mosquitto_loop(mosq, 1, 1);
        if (nNetworkLoop != MOSQ_ERR_SUCCESS) {
            TRACE("mosquitto_loop error: %d\n", nNetworkLoop);

            // UI에 연결 끊김 알림
            if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
            {
                CEVMQTTDlg* pDlg = (CEVMQTTDlg*)m_pOwner;
                pDlg->OnMqttConnectionChanged(false);
            }

            Sleep(1000);

            // 재연결 시도
            if (mosquitto_reconnect(mosq) == MOSQ_ERR_SUCCESS) {
                TRACE("MQTT reconnection successful\n");
            }
        }

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
        }

        // CPU 사용률 조절
        Sleep(1);
    }

    // 정리 작업
    TRACE("Main loop terminated, starting cleanup\n");

    if (mosq) {
        mosquitto_disconnect(mosq);
        mosquitto_destroy(mosq);
        TRACE("MQTT connection closed\n");
    }
    mosquitto_lib_cleanup();

    // 워커 스레드 종료
    DestroyWorkerThreads();

    // 메시지 큐 삭제
    if (m_pMessageQueue) {
        TRACE("Final message queue cleanup\n");
        delete m_pMessageQueue;
        m_pMessageQueue = nullptr;
        TRACE("Message queue cleanup completed\n");
    }

    if (mqtt_host) free(mqtt_host);
    if (mqtt_topic) free(mqtt_topic);

    TRACE("=== ThreadSub Terminated ===\n");
    return 0;
}

void CThreadSub::CreateWorkerThreads()
{
    TRACE("=== Worker Thread Creation Started ===\n");
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
                pWorker->SetBatchSize(30);
                pWorker->SetBatchTimeout(100);

                // AddRef() 제거 - 일반 포인터이므로 불필요
                // m_pMessageQueue->AddRef();  // 이 줄 제거

                TRACE("Worker thread %d properties set, resuming thread...\n", i + 1);

                // 스레드 시작
                DWORD resumeResult = pWorker->ResumeThread();
                TRACE("Worker thread %d ResumeThread result: %d\n", i + 1, resumeResult);

                // 스레드가 실제로 시작될 시간 주기
                Sleep(50);

                // 스레드 상태 확인
                DWORD exitCode;
                if (GetExitCodeThread(pWorker->m_hThread, &exitCode)) {
                    if (exitCode == STILL_ACTIVE) {
                        TRACE("Worker thread %d is running (STILL_ACTIVE)\n", i + 1);
                    }
                    else {
                        TRACE("WARNING: Worker thread %d already exited with code %d\n", i + 1, exitCode);
                    }
                }
                else {
                    TRACE("ERROR: Cannot get thread status for worker %d\n", i + 1);
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

    TRACE("Worker thread creation completed - Total: %d\n", m_workerThreads.size());

    // 모든 워커가 실제로 시작되었는지 한번 더 확인
    Sleep(200);
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
    TRACE("=== Worker Thread Termination Started ===\n");

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

            Sleep(100);
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
