// CThreadSub.cpp : implementation file
//

#include "pch.h"
#include "EVMQTT.h"
#include "ThreadSub.h"
#include "ConfigManager.h"
#include "JsonFileManager.h"
#include "JsonResultManager.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

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

	sprintf_s(m_szIP, sizeof(m_szIP), "127.0.0.1");
	sprintf_s(m_szTopic, sizeof(m_szTopic), "my_topic");
	m_nPort = 1883;
	m_nKeepAlive = 60;
}

CThreadSub::~CThreadSub()
{
}

BOOL CThreadSub::InitInstance()
{
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
	TRACE("connect callback, rc=%d\n", result);
}

void message_callback(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg)
{
	if (msg->payloadlen == 0)
		return;

	TRACE("topic '%s': message %s[%d] bytes\n", msg->topic, (char*)msg->payload, msg->payloadlen);

	const char* payload = (const char*)msg->payload;
	if (payload[0] != '{' && payload[0] != '[') {
		TRACE("수신된 메시지가 JSON 형식이 아닙니다: %s\n", payload);
		return; // JSON이 아니면 파싱하지 않고 종료
	}

	// JSON 메시지 파싱
	if (CThreadSub::s_jsonParser.ParseMessage(payload, msg->payloadlen)) {
		// 파싱 결과 출력
		CThreadSub::s_jsonParser.TraceEventData();

		// 여기서 파싱된 데이터를 사용하여 추가 처리 수행
		// 예: UI 업데이트, 데이터베이스 저장 등
		const CJsonParser::EventData& eventData = CThreadSub::s_jsonParser.GetEventData();

		// 이벤트 코드가 "event"인 경우에만 처리
		if (eventData.code == "event") {
			// 필요한 처리 수행...

			// 예: 타이머 카운터 값이 변경된 경우 처리
			if (eventData.timerCounter.valid) {
				// 타이머 카운터 관련 처리
			}

			// 예: 온도 값이 변경된 경우 처리
			if (eventData.temperature.valid) {
				// 온도 관련 처리
			}

			// 예: IOLink 데이터가 변경된 경우 처리
			if (eventData.iolinkDevice.valid) {
				// IOLink 관련 처리
			}
		}
	}
}

int CThreadSub::Run()
{
	CEVMQTTApp* pApp = (CEVMQTTApp*)AfxGetApp();

	SYSTEMTIME tmOld, tmCur;
	GetLocalTime(&tmOld);
	tmCur = tmOld;

	DWORD dwCur = GetTickCount();
	DWORD dwOld = dwCur;
	DWORD dwLastParsing = dwCur;

	int nErrorCode = 1;
	int nNetworkLoop;

	TRACE(">>>>Start Loop\n");

	char* mqtt_host = strdup(m_szIP);
	char* mqtt_topic = strdup(m_szTopic);
	int mqtt_port = m_nPort;
	int mqtt_keepalive = m_nKeepAlive;

	int mdelay = 0;
	bool clean_session = true;

	struct mosquitto* mosq = NULL;

	mosquitto_lib_init();
	mosq = mosquitto_new(NULL, clean_session, NULL);
	if (!mosq)
	{
		TRACE("Could not create new mosquitto struct\n");
		nErrorCode = -1;
	}

	mosquitto_connect_callback_set(mosq, connect_callback);
	mosquitto_message_callback_set(mosq, message_callback);

	if (mosquitto_connect(mosq, mqtt_host, mqtt_port, mqtt_keepalive))
	{
		TRACE("Unable to connect mosquitto.\n");
		nErrorCode = -2;
	}

	mosquitto_subscribe(mosq, NULL, mqtt_topic, 0);

	// 설정 로드
	CConfigManager& configManager = CConfigManager::GetInstance();
	configManager.LoadConfig();

	// JSON 파일 관리자
	CJsonFileManager& fileManager = CJsonFileManager::GetInstance();

	// 결과 관리자
	CJsonResultManager& resultManager = CJsonResultManager::GetInstance();

	// 초기에 한 번 폴더 스캔
	if (!configManager.GetJsonFolderPath().IsEmpty()) {
		fileManager.ScanJsonFolder(configManager.GetJsonFolderPath(), configManager.GetSortMethod());
	}

	while (!m_bEndThread)
	{
		Sleep(2);

		dwCur = GetTickCount();
		if (dwCur - dwOld >= 1000)
		{
			dwOld = dwCur;
			GetLocalTime(&tmCur);

			if (tmOld.wMinute != tmCur.wMinute)
			{
				tmOld = tmCur;

				// 1분마다 JSON 파일 폴더 다시 스캔
				if (!configManager.GetJsonFolderPath().IsEmpty()) {
					fileManager.ScanJsonFolder(configManager.GetJsonFolderPath(), configManager.GetSortMethod());
				}
			}
		}

		// 파싱 간격 체크 (기본 1초)
		int parsingInterval = configManager.GetParsingInterval();
		if (dwCur - dwLastParsing >= static_cast<DWORD>(parsingInterval)) {
			dwLastParsing = dwCur;

			// 처리할 파일이 있으면 파싱 실행
			if (fileManager.GetPendingCount() > 0) {
				JsonFileData fileData = fileManager.GetNextPendingFile();
				if (!fileData.filePath.IsEmpty()) {
					try {
						// JSON 파싱
						CJsonParser jsonParser;
						if (jsonParser.ParseMessage(fileData.content.c_str(), fileData.content.length())) {
							// 디버그 출력
							jsonParser.TraceEventData();

							// 결과 저장
							resultManager.StoreResult(fileData.filePath, jsonParser.GetEventData());

							// 처리 완료 표시
							fileManager.MarkFileAsProcessed(fileData.filePath);

							OutputDebugString(_T("파일 파싱 성공: "));
							OutputDebugString(fileData.filePath);
							OutputDebugString(_T("\n"));
						}
					}
					catch (const std::exception& e) {
						OutputDebugString(_T("파싱 중 오류 발생: "));
						OutputDebugStringA(e.what());
						OutputDebugString(_T("\n"));
					}
				}
			}
		}

		nNetworkLoop = mosquitto_loop(mosq, -1, 1);
		if (nNetworkLoop)
		{
			TRACE("mosquitto connection error!\n");
			Sleep(1000);
			mosquitto_reconnect(mosq);

			nErrorCode = -100;
		}
		else if (nErrorCode < 1)
			nErrorCode = 1;
	}

	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
	free(mqtt_host);
	free(mqtt_topic);

	TRACE(">>>>End Loop\n");

	PostThreadMessage(WM_QUIT, 0, 0);

	return CWinThread::Run();
}