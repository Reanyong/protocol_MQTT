#include "pch.h"
#include "ConfigManager.h"

CConfigManager::CConfigManager()
    : m_jsonFolderPath(_T(""))
    , m_sortMethod(FileSortMethod::BY_NAME)  // 기본값: 이름순
    , m_parsingInterval(1000) // 기본값 1초
    , m_tagGroup(_T(""))
    , m_mqttTopic(_T("my_topic")) // 기본 토픽
    , m_mqttIp(_T("127.0.0.1"))   // 기본 IP
    , m_mqttPort(1883)            // 기본 포트
    , m_mqttKeepAlive(60)         // 기본 keepalive
{
    // INI 파일 경로 설정 (실행 파일과 같은 경로에 저장)
    TCHAR szPath[MAX_PATH] = { 0 };
    GetModuleFileName(NULL, szPath, MAX_PATH);

    // 실행 파일 이름 부분 제거하고 INI 파일 이름 추가
    CString strPath(szPath);
    int nPos = strPath.ReverseFind('\\');
    if (nPos > 0) {
        m_iniFilePath = strPath.Left(nPos + 1) + _T("EVMQTT_Config.ini");
    }
    else {
        m_iniFilePath = _T("EVMQTT_Config.ini"); // 현재 디렉토리에 저장
    }
}

CConfigManager::~CConfigManager()
{
}

CConfigManager& CConfigManager::GetInstance()
{
    static CConfigManager instance;
    return instance;
}

bool CConfigManager::LoadConfig()
{
    bool result = true;

    try {
        // INI 파일에서 폴더 경로 읽기
        TCHAR szFolderPath[MAX_PATH] = { 0 };
        GetPrivateProfileString(_T("General"), _T("JsonFolderPath"), _T(""),
            szFolderPath, MAX_PATH, m_iniFilePath);
        m_jsonFolderPath = szFolderPath;

        // 파싱 간격 읽기
        m_parsingInterval = GetPrivateProfileInt(_T("General"), _T("ParsingInterval"),
            1000, m_iniFilePath);

        // 정렬 방식 읽기
        TCHAR szSortMethod[32] = { 0 };
        GetPrivateProfileString(_T("General"), _T("SortMethod"), _T("BY_NAME"),
            szSortMethod, 32, m_iniFilePath);
        m_sortMethod = StringToSortMethod(szSortMethod);

        TCHAR szTagGroup[64] = { 0 };
        GetPrivateProfileString(_T("TagInfo"), _T("TagGroup"), _T("MQTT"),
            szTagGroup, 64, m_iniFilePath);
        m_tagGroup = szTagGroup;

        // MQTT 설정 읽기
        TCHAR szMqttTopic[64] = { 0 };
        GetPrivateProfileString(_T("General"), _T("Topic"), _T("my_topic"),
            szMqttTopic, 64, m_iniFilePath);
        m_mqttTopic = szMqttTopic;

        TCHAR szMqttIp[64] = { 0 };
        GetPrivateProfileString(_T("General"), _T("Ip"), _T("127.0.0.1"),
            szMqttIp, 64, m_iniFilePath);
        m_mqttIp = szMqttIp;

        m_mqttPort = GetPrivateProfileInt(_T("General"), _T("Port"), 1883, m_iniFilePath);
        m_mqttKeepAlive = GetPrivateProfileInt(_T("General"), _T("KeepAlive"), 60, m_iniFilePath);

        // 태그셋 정보 초기화
        m_setToJsonFile.clear();
        m_jsonFileToSet.clear();

        for (int i = 1; i <= 20; i++) {  // 최대 20개 세트 지원
            CString key;
            key.Format(_T("%dset"), i);

            TCHAR szJsonFile[64] = { 0 };
            GetPrivateProfileString(_T("TagInfo"), key, _T(""),
                szJsonFile, 64, m_iniFilePath);

            CString jsonFile = szJsonFile;
            if (!jsonFile.IsEmpty()) {
                // JSON 확장자 추가 (.json이 없다면)
                if (jsonFile.Right(5).CompareNoCase(_T(".json")) != 0) {
                    jsonFile += _T(".json");
                }

                m_setToJsonFile[i] = jsonFile;
                m_jsonFileToSet[jsonFile] = i;

                TRACE("태그셋 로드: %d -> %s\n", i, jsonFile);
            }
        }

        result = result && LoadTagSets();

        return result;
    }
    catch (const std::exception& e) {
        OutputDebugStringW(L"설정 로드 오류: ");
        OutputDebugStringA(e.what());
        OutputDebugStringW(L"\n");
        return false;
    }
}

bool CConfigManager::SaveConfig()
{
    bool result = true;

    try {
        // INI 파일에 설정 저장

        // 폴더 경로 저장
        WritePrivateProfileString(_T("General"), _T("JsonFolderPath"),
            m_jsonFolderPath, m_iniFilePath);

        // 파싱 간격 저장
        CString strInterval;
        strInterval.Format(_T("%d"), m_parsingInterval);
        WritePrivateProfileString(_T("General"), _T("ParsingInterval"),
            strInterval, m_iniFilePath);

        // MQTT 설정 저장
        WritePrivateProfileString(_T("General"), _T("Topic"), m_mqttTopic, m_iniFilePath);
        WritePrivateProfileString(_T("General"), _T("Ip"), m_mqttIp, m_iniFilePath);

        CString strPort;
        strPort.Format(_T("%d"), m_mqttPort);
        WritePrivateProfileString(_T("General"), _T("Port"), strPort, m_iniFilePath);

        CString strKeepAlive;
        strKeepAlive.Format(_T("%d"), m_mqttKeepAlive);
        WritePrivateProfileString(_T("General"), _T("KeepAlive"), strKeepAlive, m_iniFilePath);

        // 정렬 방식 저장
        CString strSortMethod = SortMethodToString(m_sortMethod);
        WritePrivateProfileString(_T("General"), _T("SortMethod"),
            strSortMethod, m_iniFilePath);

        WritePrivateProfileString(_T("TagInfo"), _T("TagGroup"),
            m_tagGroup, m_iniFilePath);

        for (int i = 1; i <= 20; i++) {
            CString key;
            key.Format(_T("%dset"), i);
            WritePrivateProfileString(_T("TagInfo"), key, NULL, m_iniFilePath);
        }

        for (const auto& pair : m_setToJsonFile) {
            CString key;
            key.Format(_T("%dset"), pair.first);

            // .json 확장자 제거하여 저장
            CString jsonFile = pair.second;
            if (jsonFile.Right(5).CompareNoCase(_T(".json")) == 0) {
                jsonFile = jsonFile.Left(jsonFile.GetLength() - 5);
            }

            WritePrivateProfileString(_T("TagInfo"), key, jsonFile, m_iniFilePath);
        }

        result = result && SaveTagSets();

        return result;
    }
    catch (const std::exception& e) {
        OutputDebugStringW(L"설정 저장 오류: ");
        OutputDebugStringA(e.what());
        OutputDebugStringW(L"\n");
        return false;
    }
}

void CConfigManager::SetJsonFolderPath(const CString& folderPath)
{
    m_jsonFolderPath = folderPath;
}

CString CConfigManager::GetJsonFolderPath() const
{
    return m_jsonFolderPath;
}

void CConfigManager::SetSortMethod(FileSortMethod sortMethod)
{
    m_sortMethod = sortMethod;
}

FileSortMethod CConfigManager::GetSortMethod() const
{
    return m_sortMethod;
}

void CConfigManager::SetParsingInterval(int interval)
{
    m_parsingInterval = interval;
}

int CConfigManager::GetParsingInterval() const
{
    return m_parsingInterval;
}

CString CConfigManager::SortMethodToString(FileSortMethod method)
{
    switch (method) {
    case FileSortMethod::BY_NAME:
        return _T("BY_NAME");
    case FileSortMethod::BY_CREATION:
        return _T("BY_CREATION");
    case FileSortMethod::BY_MODIFIED:
        return _T("BY_MODIFIED");
    case FileSortMethod::NONE:
    default:
        return _T("NONE");
    }
}

FileSortMethod CConfigManager::StringToSortMethod(const CString& methodStr)
{
    if (methodStr == _T("BY_NAME"))
        return FileSortMethod::BY_NAME;
    else if (methodStr == _T("BY_CREATION"))
        return FileSortMethod::BY_CREATION;
    else if (methodStr == _T("BY_MODIFIED"))
        return FileSortMethod::BY_MODIFIED;
    else
        return FileSortMethod::NONE;
}

CString CConfigManager::GetTagGroup() const
{
    return m_tagGroup;
}

// 태그 그룹 setter
void CConfigManager::SetTagGroup(const CString& tagGroup)
{
    m_tagGroup = tagGroup;
}

// 세트 번호로 JSON 파일 이름 가져오기
CString CConfigManager::GetJsonFileForSet(int setNumber) const
{
    auto it = m_setToJsonFile.find(setNumber);
    if (it != m_setToJsonFile.end()) {
        return it->second;
    }
    return _T("");
}

// JSON 파일명으로 세트 번호 가져오기
int CConfigManager::GetSetNumberForJsonFile(const CString& jsonFileName) const
{
    // 파일명에서 경로 제거
    CString fileName = jsonFileName;
    int pos = fileName.ReverseFind('\\');
    if (pos >= 0) {
        fileName = fileName.Mid(pos + 1);
    }

    // JSON 확장자 추가 (.json이 없다면)
    if (fileName.Right(5).CompareNoCase(_T(".json")) != 0) {
        fileName += _T(".json");
    }

    auto it = m_jsonFileToSet.find(fileName);
    if (it != m_jsonFileToSet.end()) {
        return it->second;
    }
    return 0; // 0은 유효하지 않은 세트 번호
}

// 세트 번호로 태그 이름 생성
void CConfigManager::GetTagNamesForSet(int setNumber, CString& timerCounterTag,
    CString& temperatureTag, CString& ioLinkPdinTag) const
{
    if (setNumber == 1) {
        // 기본 태그 이름
        timerCounterTag = _T("TIMER_COUNTER");
        temperatureTag = _T("TEMPERATURE");
        ioLinkPdinTag = _T("IOLINK_PDIN");
    }
    else {
        // 세트 번호를 붙인 태그 이름
        timerCounterTag.Format(_T("TIMER_COUNTER%d"), setNumber);
        temperatureTag.Format(_T("TEMPERATURE%d"), setNumber);
        ioLinkPdinTag.Format(_T("IOLINK_PDIN%d"), setNumber);
    }
}

// 태그셋 추가
void CConfigManager::AddTagSet(int setNumber, const CString& jsonFileName)
{
    if (setNumber <= 0) return;

    // JSON 확장자 추가 (.json이 없다면)
    CString fileName = jsonFileName;
    if (fileName.Right(5).CompareNoCase(_T(".json")) != 0) {
        fileName += _T(".json");
    }

    m_setToJsonFile[setNumber] = fileName;
    m_jsonFileToSet[fileName] = setNumber;
}

// 태그셋 삭제
void CConfigManager::RemoveTagSet(int setNumber)
{
    auto it = m_setToJsonFile.find(setNumber);
    if (it != m_setToJsonFile.end()) {
        m_jsonFileToSet.erase(it->second);
        m_setToJsonFile.erase(it);
    }
}

bool CConfigManager::LoadTagSets()
{
    // 이미 LoadConfig()에서 모든 작업을 수행하므로 여기서는 항상 성공 반환
    return true;
}

// 태그셋 저장 (별도 메서드로 분리)
bool CConfigManager::SaveTagSets()
{
    // 이미 SaveConfig()에서 모든 작업을 수행하므로 여기서는 항상 성공 반환
    return true;
}

// 태그셋 개수 반환
int CConfigManager::GetTagSetCount() const
{
    return static_cast<int>(m_setToJsonFile.size());
}

void CConfigManager::SetMqttTopic(const CString& topic)
{
    m_mqttTopic = topic;
}

CString CConfigManager::GetMqttTopic() const
{
    return m_mqttTopic;
}

void CConfigManager::SetMqttIp(const CString& ip)
{
    m_mqttIp = ip;
}

CString CConfigManager::GetMqttIp() const
{
    return m_mqttIp;
}

void CConfigManager::SetMqttPort(int port)
{
    m_mqttPort = port;
}

int CConfigManager::GetMqttPort() const
{
    return m_mqttPort;
}

void CConfigManager::SetMqttKeepAlive(int keepAlive)
{
    m_mqttKeepAlive = keepAlive;
}

int CConfigManager::GetMqttKeepAlive() const
{
    return m_mqttKeepAlive;
}
