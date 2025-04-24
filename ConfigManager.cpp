#include "pch.h"
#include "ConfigManager.h"

CConfigManager::CConfigManager()
    : m_jsonFolderPath(_T(""))
    , m_sortMethod(FileSortMethod::BY_NAME)  // 기본값: 이름순
    , m_parsingInterval(1000) // 기본값 1초
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

        return true;
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

        // 정렬 방식 저장
        CString strSortMethod = SortMethodToString(m_sortMethod);
        WritePrivateProfileString(_T("General"), _T("SortMethod"),
            strSortMethod, m_iniFilePath);

        return true;
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
