// CThreadSub.cpp : implementation file
//

#include "pch.h"
#include "EVMQTT.h"
#include "EVMQTTDlg.h"
#include "ThreadSub.h"
#include "ConfigManager.h"
#include "JsonResultManager.h"

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
    해당 설정 ConfigManager에서 진행
    sprintf_s(m_szIP, sizeof(m_szIP), "127.0.0.1");
    sprintf_s(m_szTopic, sizeof(m_szTopic), "my_topic");
    m_nPort = 1883;
    m_nKeepAlive = 60;
    */

    // 파싱 통계 초기화
    m_nParsedCount = 0;
    m_nTotalCount = 0;
}

CThreadSub::~CThreadSub()
{
}

BOOL CThreadSub::InitInstance()
{
    // EasyView 엔진에 연결
    char szBuff[256] = { 0, };
    char szProjectName[256] = { 0, };

    EV_GetConfigFile(szBuff);
    ::GetPrivateProfileString(
        "EasyView",        // 섹션 이름
        "Project",         // 키 이름
        "",                // 기본값 (없으면 빈 문자열)
        szProjectName,     // 결과 버퍼
        sizeof(szProjectName),
        szBuff             // INI 파일 경로
    );

    int nResult = EV_OpenMem(szProjectName);
    if (nResult > 0) {
        TRACE(_T("EasyView Engine Connected: %s\n"), szProjectName);
    }
    else {
        TRACE("EasyView Engine Fail Connection: %d\n", nResult);
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
    if (result == 0) {
        TRACE("=== MQTT Connection Success! ===\n");
    } else {
        TRACE("=== MQTT Connection Failed! Error code: %d ===\n", result);
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
    TRACE("=== MQTT Message Received! ===\n");
    TRACE("Topic: '%s'\n", msg->topic);
    TRACE("Message size: %d bytes\n", msg->payloadlen);
    
    if (msg->payloadlen == 0) {
        TRACE("Empty message - skipping processing\n");
        return;
    }

    const char* payload = (const char*)msg->payload;
    
    // Message content preview (max 200 chars)
    int printLen = (msg->payloadlen > 200) ? 200 : msg->payloadlen;
    char preview[201] = {0};
    strncpy_s(preview, 201, payload, printLen);
    TRACE("Message content: %s%s\n", preview, (msg->payloadlen > 200) ? "..." : "");

    if (payload[0] != '{' && payload[0] != '[') {
        TRACE("Non-JSON message: %s\n", payload);
        return; // Skip parsing if not JSON
    }

    // Get ThreadSub object pointer (passed via obj parameter)
    CThreadSub* pThreadSub = static_cast<CThreadSub*>(obj);
    if (!pThreadSub) {
        TRACE("ThreadSub object is NULL\n");
        return;
    }

    CEVMQTTDlg* pDlg = (CEVMQTTDlg*)pThreadSub->m_pOwner;

    try {
        TRACE("JSON parsing started...\n");
        
        // Parse JSON message
        CJsonParser jsonParser;
        bool parsed = jsonParser.ParseMessage(payload, msg->payloadlen);

        // Check for errors based on parser results
        bool hasError = false;
        CString errorMessage;
        CString mqttIdentifier;
        mqttIdentifier.Format(_T("MQTT/%s"), CStringA(msg->topic).GetString());

        if (!parsed) {
            // Basic parsing failed (JSON format error)
            hasError = true;
            errorMessage = _T("MQTT JSON parsing error");
            TRACE("JSON parsing failed\n");
        }
        else if (jsonParser.GetParseStatus() != CJsonParser::PARSE_SUCCESS) {
            // Basic parsing succeeded but data validation error occurred
            hasError = true;
            errorMessage = jsonParser.GetErrorMessage();
            TRACE("JSON data validation failed: %s\n", CStringA(errorMessage).GetString());
        }
        else {
            TRACE("JSON parsing success!\n");
            
            // Process MQTT message using tag mapping from INI file
            CString mqttTopic = CString(msg->topic);
            
            // Get tag mapping information from ConfigManager
            CConfigManager& configManager = CConfigManager::GetInstance();
            
            TRACE("Processing MQTT topic: %s\n", CStringA(mqttTopic).GetString());
            
            // Apply tags mapped to this topic
            bool tagResult = jsonParser.ApplyMqttTagMapping(mqttTopic);

            if (tagResult) {
                TRACE("EasyView tag application success!\n");

                // Add success log
                if (pDlg && ::IsWindow(pDlg->GetSafeHwnd())) {
                    pDlg->AddDebugLog(_T("MQTT message processing success"), CString(msg->topic), DebugLogItem::LOG_SUCCESS);
                }
            }
            else {
                TRACE("EasyView tag application failed\n");
                hasError = true;
                errorMessage = _T("EasyView tag application failed");
            }
        }

        // Add error log if there are errors
        if (hasError && pDlg && ::IsWindow(pDlg->GetSafeHwnd())) {
            pDlg->AddDebugLog(errorMessage, CString(msg->topic), DebugLogItem::LOG_ERROR);
        }

        // Update statistics
        if (!hasError) {
            pThreadSub->m_nParsedCount++;
            pThreadSub->UpdateStats(pThreadSub->m_nParsedCount, pThreadSub->m_nTotalCount);
        }

    }
    catch (const std::exception& e) {
        TRACE("Exception occurred during message processing: %s\n", e.what());
        
        if (pDlg && ::IsWindow(pDlg->GetSafeHwnd())) {
            CString errorMsg;
            errorMsg.Format(_T("MQTT message processing exception: %hs"), e.what());
            pDlg->AddDebugLog(errorMsg, CString(msg->topic), DebugLogItem::LOG_ERROR);
        }
    }
}



// Statistics update method implementation
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
    // MQTT initialization
    CEVMQTTApp* pApp = (CEVMQTTApp*)AfxGetApp();
    DWORD dwCur = GetTickCount();
    DWORD dwOld = dwCur;
    DWORD dwLastParsing = dwCur;
    int nErrorCode = 1;
    int nNetworkLoop;
    TRACE(">>>>Start Loop\n");

    CConfigManager& configManager = CConfigManager::GetInstance();
    configManager.LoadConfig();

    // Get MQTT configuration from ConfigManager
    CString mqttIp = configManager.GetMqttIp();
    int mqttPort = configManager.GetMqttPort();
    int mqttKeepAlive = configManager.GetMqttKeepAlive();

    // Convert CString to char* - Performance optimization: reuse CT2A
    CT2A hostA(mqttIp);
    char* mqtt_host = strdup(hostA);
    char* mqtt_topic = strdup("+");  // Subscribe to all topics
    int mqtt_port = mqttPort;
    int mqtt_keepalive = mqttKeepAlive;

    int mdelay = 0;
    bool clean_session = true;
    struct mosquitto* mosq = NULL;

    // 파싱 통계 초기화
    m_nParsedCount = 0;
    m_nTotalCount = 0;

    mosquitto_lib_init();
    mosq = mosquitto_new(NULL, clean_session, NULL);
    if (!mosq)
    {
        TRACE("mosquitto structure creation failed\n");
        nErrorCode = -1;
    }
    else {
        TRACE("mosquitto structure creation success\n");
    }
    
    // 콜백 함수 등록
    mosquitto_connect_callback_set(mosq, connect_callback);
    mosquitto_message_callback_set(mosq, message_callback);
    mosquitto_subscribe_callback_set(mosq, subscribe_callback);
    
    TRACE("MQTT broker connection attempt: %s:%d\n", mqtt_host, mqtt_port);
    
    if (mosquitto_connect(mosq, mqtt_host, mqtt_port, mqtt_keepalive))
    {
        TRACE("MQTT broker connection failed\n");
        nErrorCode = -2;
    }
    else {
        TRACE("MQTT broker connection request sent\n");
    }
    
    TRACE("MQTT topic subscription attempt: '%s'\n", mqtt_topic);
    int subscribe_result = mosquitto_subscribe(mosq, NULL, mqtt_topic, 0);
    if (subscribe_result == MOSQ_ERR_SUCCESS) {
        TRACE("MQTT subscription request sent successfully\n");
    } else {
        TRACE("MQTT subscription request failed: %d\n", subscribe_result);
    }
    
    mosquitto_user_data_set(mosq, this);

    // Result manager
    CJsonResultManager& resultManager = CJsonResultManager::GetInstance();

    CT2A ipA_log(mqttIp);
    TRACE("MQTT Configuration - IP: %s, Port: %d, Keep-Alive: %d\n",
        ipA_log.m_psz, mqttPort, mqttKeepAlive);

    while (!m_bEndThread)
    {
        dwCur = GetTickCount();

        // MQTT 메시지 처리
        nNetworkLoop = mosquitto_loop(mosq, 1, 1);
        if (nNetworkLoop != MOSQ_ERR_SUCCESS)
        {
            TRACE("mosquitto_loop error: %d\n", nNetworkLoop);
            Sleep(1000);
            mosquitto_reconnect(mosq);
        }

        // UI 업데이트 주기 제어 (500ms마다)
        if (dwCur - dwOld > 500)
        {
            dwOld = dwCur;
            
            // 통계 업데이트
            UpdateStats(m_nParsedCount, m_nTotalCount);
        }

        // 부하 감소를 위한 작은 지연
        Sleep(1);
    }

    // 정리
    if (mosq) {
        mosquitto_disconnect(mosq);
        mosquitto_destroy(mosq);
    }
    mosquitto_lib_cleanup();

    if (mqtt_host) free(mqtt_host);
    if (mqtt_topic) free(mqtt_topic);

    TRACE("<<<<End Loop\n");
    return 0;
}
