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
		TRACE("���ŵ� �޽����� JSON ������ �ƴմϴ�: %s\n", payload);
		return; // JSON�� �ƴϸ� �Ľ����� �ʰ� ����
	}

	// JSON �޽��� �Ľ�
	if (CThreadSub::s_jsonParser.ParseMessage(payload, msg->payloadlen)) {
		// �Ľ� ��� ���
		CThreadSub::s_jsonParser.TraceEventData();

		// ���⼭ �Ľ̵� �����͸� ����Ͽ� �߰� ó�� ����
		// ��: UI ������Ʈ, �����ͺ��̽� ���� ��
		const CJsonParser::EventData& eventData = CThreadSub::s_jsonParser.GetEventData();

		// �̺�Ʈ �ڵ尡 "event"�� ��쿡�� ó��
		if (eventData.code == "event") {
			// �ʿ��� ó�� ����...

			// ��: Ÿ�̸� ī���� ���� ����� ��� ó��
			if (eventData.timerCounter.valid) {
				// Ÿ�̸� ī���� ���� ó��
			}

			// ��: �µ� ���� ����� ��� ó��
			if (eventData.temperature.valid) {
				// �µ� ���� ó��
			}

			// ��: IOLink �����Ͱ� ����� ��� ó��
			if (eventData.iolinkDevice.valid) {
				// IOLink ���� ó��
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

	// ���� �ε�
	CConfigManager& configManager = CConfigManager::GetInstance();
	configManager.LoadConfig();

	// JSON ���� ������
	CJsonFileManager& fileManager = CJsonFileManager::GetInstance();

	// ��� ������
	CJsonResultManager& resultManager = CJsonResultManager::GetInstance();

	// �ʱ⿡ �� �� ���� ��ĵ
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

				// 1�и��� JSON ���� ���� �ٽ� ��ĵ
				if (!configManager.GetJsonFolderPath().IsEmpty()) {
					fileManager.ScanJsonFolder(configManager.GetJsonFolderPath(), configManager.GetSortMethod());
				}
			}
		}

		// �Ľ� ���� üũ (�⺻ 1��)
		int parsingInterval = configManager.GetParsingInterval();
		if (dwCur - dwLastParsing >= static_cast<DWORD>(parsingInterval)) {
			dwLastParsing = dwCur;

			// ó���� ������ ������ �Ľ� ����
			if (fileManager.GetPendingCount() > 0) {
				JsonFileData fileData = fileManager.GetNextPendingFile();
				if (!fileData.filePath.IsEmpty()) {
					try {
						// JSON �Ľ�
						CJsonParser jsonParser;
						if (jsonParser.ParseMessage(fileData.content.c_str(), fileData.content.length())) {
							// ����� ���
							jsonParser.TraceEventData();

							// ��� ����
							resultManager.StoreResult(fileData.filePath, jsonParser.GetEventData());

							// ó�� �Ϸ� ǥ��
							fileManager.MarkFileAsProcessed(fileData.filePath);

							OutputDebugString(_T("���� �Ľ� ����: "));
							OutputDebugString(fileData.filePath);
							OutputDebugString(_T("\n"));
						}
					}
					catch (const std::exception& e) {
						OutputDebugString(_T("�Ľ� �� ���� �߻�: "));
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