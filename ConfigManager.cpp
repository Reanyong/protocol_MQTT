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
        result = result && LoadTagMappings();

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
        result = result && SaveTagMappings();

        return result;
    }
    catch (const std::exception& e) {
        OutputDebugStringW(L"설정 저장 오류: ");
        OutputDebugStringA(e.what());
        OutputDebugStringW(L"\n");
        return false;
    }
}

// *** 태그 매핑 관련 새로운 메서드들 구현 ***

void CConfigManager::SetTagJsonPath(const CString& tagName, const CString& jsonPath)
{
    m_tagMappings[tagName] = jsonPath;
    TRACE("태그 매핑 설정: %s -> %s\n", tagName, jsonPath);
}

CString CConfigManager::GetJsonPathForTag(const CString& tagName) const
{
    auto it = m_tagMappings.find(tagName);
    if (it != m_tagMappings.end()) {
        return it->second;
    }
    return _T("");
}

std::map<CString, CString> CConfigManager::GetAllTagMappings() const
{
    return m_tagMappings;
}

bool CConfigManager::LoadTagMappings()
{
    try {
        m_tagMappings.clear();

        // INI 파일에서 [TagMapping] 섹션 읽기
        TCHAR szSection[8192] = { 0 };
        DWORD dwRet = GetPrivateProfileSection(_T("TagMapping"), szSection,
            sizeof(szSection) / sizeof(TCHAR), m_iniFilePath);

        if (dwRet > 0) {
            TCHAR* pStart = szSection;
            while (*pStart) {
                CString strLine = pStart;
                int nPos = strLine.Find('=');
                if (nPos > 0) {
                    CString tagName = strLine.Left(nPos);
                    CString jsonPath = strLine.Mid(nPos + 1);

                    tagName.Trim();
                    jsonPath.Trim();

                    if (!tagName.IsEmpty() && !jsonPath.IsEmpty()) {
                        m_tagMappings[tagName] = jsonPath;
                        TRACE("태그 매핑 로드: %s -> %s\n", tagName, jsonPath);
                    }
                }
                pStart += strLine.GetLength() + 1;
            }
        }

        // 태그 매핑이 없으면 기본값 생성
        if (m_tagMappings.empty()) {
            CreateDefaultTagMappings();
            SaveTagMappings(); // 기본값을 INI에 저장
        }

        TRACE("총 %d개의 태그 매핑을 로드했습니다.\n", m_tagMappings.size());
        return true;
    }
    catch (const std::exception& e) {
        OutputDebugStringW(L"태그 매핑 로드 오류: ");
        OutputDebugStringA(e.what());
        OutputDebugStringW(L"\n");
        return false;
    }
}

bool CConfigManager::SaveTagMappings()
{
    try {
        // 기존 [TagMapping] 섹션 삭제
        WritePrivateProfileString(_T("TagMapping"), NULL, NULL, m_iniFilePath);

        // 새로운 태그 매핑 저장
        for (const auto& pair : m_tagMappings) {
            WritePrivateProfileString(_T("TagMapping"), pair.first, pair.second, m_iniFilePath);
            TRACE("태그 매핑 저장: %s -> %s\n", pair.first, pair.second);
        }

        TRACE("총 %d개의 태그 매핑을 저장했습니다.\n", m_tagMappings.size());
        return true;
    }
    catch (const std::exception& e) {
        OutputDebugStringW(L"태그 매핑 저장 오류: ");
        OutputDebugStringA(e.what());
        OutputDebugStringW(L"\n");
        return false;
    }
}

void CConfigManager::CreateDefaultTagMappings()
{
    // 기본 태그 매핑 설정
    m_tagMappings[_T("TIMER_COUNTER")] = _T("$.data.payload./timer[1]/counter.data");
    m_tagMappings[_T("TEMPERATURE")] = _T("$.data.payload./processdatamaster/temperature.data");
    m_tagMappings[_T("IOLINK_PDIN")] = _T("$.data.payload./iolinkmaster/port[2]/iolinkdevice/pdin.data");

    // 기본적인 JSON 필드들
    m_tagMappings[_T("MY_CUSTOM_TAG")] = _T("$.code");
    m_tagMappings[_T("ANOTHER_TAG")] = _T("$.cid");

    // 다른 형태의 JSON을 위한 예시
    m_tagMappings[_T("SIMPLE_VALUE")] = _T("$.data.value");
    m_tagMappings[_T("SIMPLE_CODE")] = _T("$.code");

    TRACE("기본 태그 매핑을 생성했습니다.\n");
}

void CConfigManager::RemoveTagMapping(const CString& tagName)
{
    auto it = m_tagMappings.find(tagName);
    if (it != m_tagMappings.end()) {
        m_tagMappings.erase(it);
        TRACE("태그 매핑 삭제: %s\n", tagName);
    }
}

bool CConfigManager::HasTagMapping(const CString& tagName) const
{
    return m_tagMappings.find(tagName) != m_tagMappings.end();
}

// 기존 메서드들은 그대로 유지...

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

void CConfigManager::SetTagGroup(const CString& tagGroup)
{
    m_tagGroup = tagGroup;
}

CString CConfigManager::GetJsonFileForSet(int setNumber) const
{
    auto it = m_setToJsonFile.find(setNumber);
    if (it != m_setToJsonFile.end()) {
        return it->second;
    }
    return _T("");
}

int CConfigManager::GetSetNumberForJsonFile(const CString& jsonFileName) const
{
    CString fileName = jsonFileName;
    int pos = fileName.ReverseFind('\\');
    if (pos >= 0) {
        fileName = fileName.Mid(pos + 1);
    }

    if (fileName.Right(5).CompareNoCase(_T(".json")) != 0) {
        fileName += _T(".json");
    }

    auto it = m_jsonFileToSet.find(fileName);
    if (it != m_jsonFileToSet.end()) {
        return it->second;
    }
    return 0;
}

void CConfigManager::GetTagNamesForSet(int setNumber, CString& timerCounterTag,
    CString& temperatureTag, CString& ioLinkPdinTag) const
{
    if (setNumber == 1) {
        timerCounterTag = _T("TIMER_COUNTER");
        temperatureTag = _T("TEMPERATURE");
        ioLinkPdinTag = _T("IOLINK_PDIN");
    }
    else {
        timerCounterTag.Format(_T("TIMER_COUNTER%d"), setNumber);
        temperatureTag.Format(_T("TEMPERATURE%d"), setNumber);
        ioLinkPdinTag.Format(_T("IOLINK_PDIN%d"), setNumber);
    }
}

void CConfigManager::AddTagSet(int setNumber, const CString& jsonFileName)
{
    if (setNumber <= 0) return;

    CString fileName = jsonFileName;
    if (fileName.Right(5).CompareNoCase(_T(".json")) != 0) {
        fileName += _T(".json");
    }

    m_setToJsonFile[setNumber] = fileName;
    m_jsonFileToSet[fileName] = setNumber;
}

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
    return true;
}

bool CConfigManager::SaveTagSets()
{
    return true;
}

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
