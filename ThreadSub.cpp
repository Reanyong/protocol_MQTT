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

    sprintf_s(m_szIP, sizeof(m_szIP), "127.0.0.1");
    sprintf_s(m_szTopic, sizeof(m_szTopic), "my_topic");
    m_nPort = 1883;
    m_nKeepAlive = 60;

    m_directoryHandle = INVALID_HANDLE_VALUE;
    m_overlapped.hEvent = NULL;
    m_bWatchDirectory = false;

    // 파싱 통계 초기화
    m_nParsedCount = 0;
    m_nTotalCount = 0;
}

CThreadSub::~CThreadSub()
{
    StopDirectoryWatch();
}

BOOL CThreadSub::InitInstance()
{

    CString folderPath = CConfigManager::GetInstance().GetJsonFolderPath();
    if (!folderPath.IsEmpty())
    {
        StartDirectoryWatch(folderPath);
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
    TRACE("connect callback, rc=%d\n", result);
}

void message_callback(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg)
{
    if (msg->payloadlen == 0)
        return;

    TRACE("topic '%s': message %s[%d] bytes\n", msg->topic, (char*)msg->payload, msg->payloadlen);

    const char* payload = (const char*)msg->payload;
    if (payload[0] != '{' && payload[0] != '[') {
        TRACE("Received message is not in JSON format: %s\n", payload);
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

bool CThreadSub::StartDirectoryWatch(const CString& folderPath)
{
    // 이미 감시 중이면 중지
    if (m_directoryHandle != INVALID_HANDLE_VALUE)
    {
        StopDirectoryWatch();
    }

    // 디렉토리 핸들 열기
    m_directoryHandle = CreateFile(
        folderPath,                             // 감시할 디렉토리 경로
        FILE_LIST_DIRECTORY,                    // 디렉토리 접근 권한
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, // 공유 모드
        NULL,                                   // 보안 속성
        OPEN_EXISTING,                          // 열기 옵션
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, // 파일 속성
        NULL);                                  // 템플릿 파일

    if (m_directoryHandle == INVALID_HANDLE_VALUE)
    {
        TRACE(_T("Failed to open directory handle: %d\n"), GetLastError());
        return false;
    }

    // OVERLAPPED 구조체 초기화
    ZeroMemory(&m_overlapped, sizeof(OVERLAPPED));
    m_overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (m_overlapped.hEvent == NULL)
    {
        TRACE(_T("Failed to create event: %d\n"), GetLastError());
        CloseHandle(m_directoryHandle);
        m_directoryHandle = INVALID_HANDLE_VALUE;
        return false;
    }

    // 변경 감시 시작
    m_bWatchDirectory = true;

    // 비동기 디렉토리 변경 감시 시작
    BOOL success = ReadDirectoryChangesW(
        m_directoryHandle,                      // 디렉토리 핸들
        m_buffer,                               // 결과 버퍼
        sizeof(m_buffer),                       // 버퍼 크기
        FALSE,                                  // 하위 디렉토리 포함 여부
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE, // 감시할 변경 유형
        &m_bytesReturned,                       // 반환된 바이트 수
        &m_overlapped,                          // OVERLAPPED 구조체
        NULL);                                  // 완료 루틴

    if (!success)
    {
        TRACE(_T("ReadDirectoryChangesW failed: %d\n"), GetLastError());
        CloseHandle(m_overlapped.hEvent);
        CloseHandle(m_directoryHandle);
        m_directoryHandle = INVALID_HANDLE_VALUE;
        return false;
    }

    return true;
}

void CThreadSub::StopDirectoryWatch()
{
    m_bWatchDirectory = false;

    if (m_overlapped.hEvent != NULL)
    {
        CloseHandle(m_overlapped.hEvent);
        m_overlapped.hEvent = NULL;
    }

    if (m_directoryHandle != INVALID_HANDLE_VALUE)
    {
        CancelIo(m_directoryHandle);
        CloseHandle(m_directoryHandle);
        m_directoryHandle = INVALID_HANDLE_VALUE;
    }
}

void CThreadSub::ProcessDirectoryChanges()
{
    if (!m_bWatchDirectory || m_directoryHandle == INVALID_HANDLE_VALUE)
        return;

    // 변경 이벤트 대기
    DWORD waitResult = WaitForSingleObject(m_overlapped.hEvent, 100); // 100ms 타임아웃

    if (waitResult == WAIT_OBJECT_0)
    {
        // 비동기 I/O 완료 확인
        DWORD bytesTransferred = 0;
        if (GetOverlappedResult(m_directoryHandle, &m_overlapped, &bytesTransferred, FALSE))
        {
            if (bytesTransferred > 0)
            {
                // 버퍼 처리
                FILE_NOTIFY_INFORMATION* pNotify = (FILE_NOTIFY_INFORMATION*)m_buffer;

                for (;;)
                {
                    // 파일 이름 변환
                    WCHAR fileName[MAX_PATH] = { 0 };
                    wcsncpy_s(fileName, MAX_PATH, pNotify->FileName, pNotify->FileNameLength / sizeof(WCHAR));

                    // 파일 확장자 확인 (.json 파일만 처리)
                    CString strFileName(fileName);
                    if (strFileName.Right(5).CompareNoCase(_T(".json")) == 0)
                    {
                        // 변경 유형에 따른 처리
                        if (pNotify->Action == FILE_ACTION_ADDED ||
                            pNotify->Action == FILE_ACTION_MODIFIED)
                        {
                            // 파일 경로 생성
                            CString folderPath = CConfigManager::GetInstance().GetJsonFolderPath();
                            if (folderPath.Right(1) != _T("\\"))
                                folderPath += _T("\\");

                            CString fullPath = folderPath + strFileName;

                            // 새 파일 로드
                            CJsonFileManager::GetInstance().LoadJsonFile(fullPath);

                            TRACE(_T("New file detected: %s\n"), fullPath);
                        }
                    }

                    // 다음 항목으로 이동
                    if (pNotify->NextEntryOffset == 0)
                        break;

                    pNotify = (FILE_NOTIFY_INFORMATION*)((BYTE*)pNotify + pNotify->NextEntryOffset);
                }
            }

            // 다시 변경 감시 설정
            ResetEvent(m_overlapped.hEvent);
            BOOL success = ReadDirectoryChangesW(
                m_directoryHandle,
                m_buffer,
                sizeof(m_buffer),
                FALSE,
                FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
                &m_bytesReturned,
                &m_overlapped,
                NULL);

            if (!success)
            {
                TRACE(_T("Failed to restart ReadDirectoryChangesW: %d\n"), GetLastError());
                StopDirectoryWatch();
            }
        }
    }
    else if (waitResult == WAIT_FAILED)
    {
        TRACE(_T("WaitForSingleObject failed: %d\n"), GetLastError());
    }
}

// 통계 업데이트 메서드 구현
void CThreadSub::UpdateStats(int parsedCount, int totalCount)
{
    m_nParsedCount = parsedCount;
    m_nTotalCount = totalCount;

    if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
    {
        // UI 스레드에 메시지 전송
        ::PostMessage(m_pOwner->GetSafeHwnd(), WM_USER + 100, parsedCount, totalCount);
    }
}

int CThreadSub::Run()
{
    // MQTT 초기화
    CEVMQTTApp* pApp = (CEVMQTTApp*)AfxGetApp();
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

    // 파싱 통계 초기화
    m_nParsedCount = 0;
    m_nTotalCount = 0;

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

    // Settings load
    CConfigManager& configManager = CConfigManager::GetInstance();
    configManager.LoadConfig();

    // JSON file manager
    CJsonFileManager& fileManager = CJsonFileManager::GetInstance();

    // Result manager
    CJsonResultManager& resultManager = CJsonResultManager::GetInstance();

    // Variables for directory change detection
    HANDLE hDirectory = INVALID_HANDLE_VALUE;
    OVERLAPPED overlapped = { 0 };
    char buffer[8192] = { 0 };  // Buffer to store change information
    DWORD bytesReturned = 0;

    // Directory monitoring setup
    CString folderPath = configManager.GetJsonFolderPath();
    if (!folderPath.IsEmpty())
    {
        // Initial folder scan once
        fileManager.ScanJsonFolder(folderPath, configManager.GetSortMethod());

        // 총 파일 수 업데이트
        m_nTotalCount = fileManager.GetTotalJsonCount();
        UpdateStats(m_nParsedCount, m_nTotalCount);

        // Open directory handle
        hDirectory = CreateFile(
            folderPath,                           // Directory path to monitor
            FILE_LIST_DIRECTORY,                  // Directory access rights
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, // Share mode
            NULL,                                 // Security attributes
            OPEN_EXISTING,                        // Open options
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, // File attributes
            NULL);                                // Template file

        if (hDirectory != INVALID_HANDLE_VALUE)
        {
            // Initialize OVERLAPPED structure
            overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

            // Start change monitoring
            if (overlapped.hEvent)
            {
                BOOL result = ReadDirectoryChangesW(
                    hDirectory,                    // Directory handle
                    buffer,                        // Result buffer
                    sizeof(buffer),                // Buffer size
                    FALSE,                         // Include subdirectories
                    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE, // Change types to monitor
                    &bytesReturned,                // Returned bytes
                    &overlapped,                   // OVERLAPPED structure
                    NULL);                         // Completion routine

                if (!result && GetLastError() != ERROR_IO_PENDING)
                {
                    TRACE("ReadDirectoryChangesW failed: %d\n", GetLastError());
                    CloseHandle(overlapped.hEvent);
                    overlapped.hEvent = NULL;
                    CloseHandle(hDirectory);
                    hDirectory = INVALID_HANDLE_VALUE;
                }
                else
                {
                    TRACE("Started monitoring directory changes: %s\n", folderPath);
                }
            }
            else
            {
                TRACE("Failed to create event: %d\n", GetLastError());
                CloseHandle(hDirectory);
                hDirectory = INVALID_HANDLE_VALUE;
            }
        }
        else
        {
            TRACE("Failed to open directory handle: %d\n", GetLastError());
        }
    }

    while (!m_bEndThread)
    {
        Sleep(2);
        dwCur = GetTickCount();

        // Directory change detection processing
        if (hDirectory != INVALID_HANDLE_VALUE && overlapped.hEvent)
        {
            DWORD waitStatus = WaitForSingleObject(overlapped.hEvent, 0);
            if (waitStatus == WAIT_OBJECT_0)
            {
                // Verify asynchronous I/O completion
                DWORD bytesTransferred = 0;
                if (GetOverlappedResult(hDirectory, &overlapped, &bytesTransferred, FALSE))
                {
                    if (bytesTransferred > 0)
                    {
                        // Process buffer
                        FILE_NOTIFY_INFORMATION* pNotify = (FILE_NOTIFY_INFORMATION*)buffer;
                        bool fileAdded = false;

                        for (;;)
                        {
                            // Convert file name
                            WCHAR fileName[MAX_PATH] = { 0 };
                            wcsncpy_s(fileName, MAX_PATH, pNotify->FileName, pNotify->FileNameLength / sizeof(WCHAR));

                            // Check file extension (.json files only)
                            CString strFileName(fileName);
                            if (strFileName.Right(5).CompareNoCase(_T(".json")) == 0)
                            {
                                // Process by change type
                                if (pNotify->Action == FILE_ACTION_ADDED ||
                                    pNotify->Action == FILE_ACTION_MODIFIED)
                                {
                                    // Create file path
                                    CString fullPath = folderPath;
                                    if (fullPath.Right(1) != _T("\\"))
                                        fullPath += _T("\\");

                                    fullPath += strFileName;

                                    // Load new file
                                    if (fileManager.LoadJsonFile(fullPath))
                                    {
                                        if (pNotify->Action == FILE_ACTION_ADDED)
                                        {
                                            fileAdded = true;
                                        }
                                    }

                                    TRACE(_T("New file detected: %s\n"), fullPath);
                                }
                            }

                            // Move to next item
                            if (pNotify->NextEntryOffset == 0)
                                break;

                            pNotify = (FILE_NOTIFY_INFORMATION*)((BYTE*)pNotify + pNotify->NextEntryOffset);
                        }

                        // 파일이 추가되었으면 총 파일 수 업데이트
                        if (fileAdded)
                        {
                            m_nTotalCount = fileManager.GetTotalJsonCount();
                            UpdateStats(m_nParsedCount, m_nTotalCount);
                        }
                    }

                    // Set change monitoring again
                    ResetEvent(overlapped.hEvent);
                    BOOL success = ReadDirectoryChangesW(
                        hDirectory,
                        buffer,
                        sizeof(buffer),
                        FALSE,
                        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
                        &bytesReturned,
                        &overlapped,
                        NULL);

                    if (!success && GetLastError() != ERROR_IO_PENDING)
                    {
                        TRACE("Failed to restart ReadDirectoryChangesW: %d\n", GetLastError());
                        CloseHandle(overlapped.hEvent);
                        overlapped.hEvent = NULL;
                        CloseHandle(hDirectory);
                        hDirectory = INVALID_HANDLE_VALUE;
                    }
                }
            }
        }

        // Parsing interval check (default 1 second)
        int parsingInterval = configManager.GetParsingInterval();
        if (dwCur - dwLastParsing >= static_cast<DWORD>(parsingInterval)) {
            dwLastParsing = dwCur;
            // Parse if there are files to process
            if (fileManager.GetPendingCount() > 0) {
                JsonFileData fileData = fileManager.GetNextPendingFile();
                if (!fileData.filePath.IsEmpty()) {
                    try {
                        // JSON parsing
                        CJsonParser jsonParser;
                        if (jsonParser.ParseMessage(fileData.content.c_str(), fileData.content.length())) {
                            // Debug output
                            jsonParser.TraceEventData();
                            // Save result
                            resultManager.StoreResult(fileData.filePath, jsonParser.GetEventData());
                            // Mark as processed
                            fileManager.MarkFileAsProcessed(fileData.filePath);
                            OutputDebugString(_T("File parsing successful: "));
                            OutputDebugString(fileData.filePath);
                            OutputDebugString(_T("\n"));

                            // 파싱 성공 카운트 증가 및 통계 업데이트
                            m_nParsedCount++;
                            UpdateStats(m_nParsedCount, m_nTotalCount);
                        }
                        else {
                            // 파싱 실패해도 처리 완료로 표시
                            fileManager.MarkFileAsProcessed(fileData.filePath);
                            OutputDebugString(_T("File parsing failed: "));
                            OutputDebugString(fileData.filePath);
                            OutputDebugString(_T("\n"));
                        }
                    }
                    catch (const std::exception& e) {
                        // 예외 발생해도 처리 완료로 표시
                        fileManager.MarkFileAsProcessed(fileData.filePath);
                        OutputDebugString(_T("Error during parsing: "));
                        OutputDebugStringA(e.what());
                        OutputDebugString(_T("\n"));
                    }
                }
            }
        }

        // MQTT loop processing
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

    // Cleanup: Stop directory monitoring
    if (overlapped.hEvent)
    {
        CloseHandle(overlapped.hEvent);
        overlapped.hEvent = NULL;
    }

    if (hDirectory != INVALID_HANDLE_VALUE)
    {
        CancelIo(hDirectory);
        CloseHandle(hDirectory);
        hDirectory = INVALID_HANDLE_VALUE;
    }

    // MQTT cleanup
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    free(mqtt_host);
    free(mqtt_topic);
    TRACE(">>>>End Loop\n");
    PostThreadMessage(WM_QUIT, 0, 0);
    return CWinThread::Run();
}
