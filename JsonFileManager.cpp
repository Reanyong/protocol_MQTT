#include "pch.h"
#include "JsonFileManager.h"

CJsonFileManager::CJsonFileManager()
{
}

CJsonFileManager::~CJsonFileManager()
{
}

CJsonFileManager& CJsonFileManager::GetInstance()
{
    static CJsonFileManager instance;
    return instance;
}

bool CJsonFileManager::LoadJsonFile(const CString& filePath)
{
    try {
        // 파일 존재 여부 확인
        CFileStatus status;
        if (!CFile::GetStatus(filePath, status)) {
            OutputDebugString(_T("파일이 존재하지 않습니다: "));
            OutputDebugString(filePath);
            OutputDebugString(_T("\n"));
            return false;
        }

        // 파일 시간 확인
        time_t fileTime = GetFileModifiedTime(filePath);

        // 이미 처리된 파일인지 확인
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_jsonFiles.find(filePath);
            if (it != m_jsonFiles.end() && it->second.lastModified == fileTime && it->second.processed) {
                // 이미 처리된 파일이고 변경되지 않았으면 건너뜀
                return true;
            }
        }

        // 파일 열기
        CFile file;
        if (!file.Open(filePath, CFile::modeRead | CFile::shareDenyWrite)) {
            OutputDebugString(_T("파일을 열 수 없습니다: "));
            OutputDebugString(filePath);
            OutputDebugString(_T("\n"));
            return false;
        }

        // 파일 크기 확인 및 메모리 할당
        ULONGLONG fileSize = file.GetLength();
        if (fileSize > 10 * 1024 * 1024) {  // 10MB 제한
            OutputDebugString(_T("파일 크기가 너무 큽니다: "));
            OutputDebugString(filePath);
            OutputDebugString(_T("\n"));
            file.Close();
            return false;
        }

        std::vector<char> buffer(static_cast<size_t>(fileSize) + 1, 0);

        // 파일 읽기
        file.Read(buffer.data(), static_cast<UINT>(fileSize));
        file.Close();

        // JSON 구문 확인 (실제 파싱은 나중에 별도로 함)
        std::string content(buffer.data(), static_cast<size_t>(fileSize));
        try {
            auto json = nlohmann::json::parse(content);

            // 파일 데이터 저장
            JsonFileData fileData;
            fileData.filePath = filePath;
            fileData.content = std::move(content);
            fileData.lastModified = fileTime;
            fileData.processed = false;

            // 맵과 큐에 추가
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_jsonFiles[filePath] = std::move(fileData);
                m_pendingQueue.push_back(filePath);
            }

            OutputDebugString(_T("파일 로드 성공: "));
            OutputDebugString(filePath);
            OutputDebugString(_T("\n"));
            return true;
        }
        catch (const nlohmann::json::parse_error& e) {
            OutputDebugString(_T("JSON 파싱 오류: "));
            OutputDebugStringA(e.what());
            OutputDebugString(_T("\n"));
            return false;
        }
    }
    catch (const std::exception& e) {
        OutputDebugString(_T("파일 로드 중 예외 발생: "));
        OutputDebugStringA(e.what());
        OutputDebugString(_T("\n"));
        return false;
    }
}

void CJsonFileManager::ScanJsonFolder(const CString& folderPath, FileSortMethod sortMethod)
{
    // 폴더가 비어있으면 처리하지 않음
    if (folderPath.IsEmpty()) {
        OutputDebugString(_T("폴더 경로가 지정되지 않았습니다.\n"));
        return;
    }

    // 폴더에서 JSON 파일 찾기
    std::vector<FileInfo> files = FindJsonFilesInFolder(folderPath);

    // 정렬 방식에 따라 파일 정렬
    switch (sortMethod) {
    case FileSortMethod::BY_NAME:
        std::sort(files.begin(), files.end(), CompareByName);
        OutputDebugString(_T("파일을 이름 순서로 정렬했습니다.\n"));
        break;
    case FileSortMethod::BY_CREATION:
        std::sort(files.begin(), files.end(), CompareByCreation);
        OutputDebugString(_T("파일을 생성 시간 순서로 정렬했습니다.\n"));
        break;
    case FileSortMethod::BY_MODIFIED:
        std::sort(files.begin(), files.end(), CompareByModified);
        OutputDebugString(_T("파일을 수정 시간 순서로 정렬했습니다.\n"));
        break;
    case FileSortMethod::NONE:
    default:
        OutputDebugString(_T("파일을 정렬하지 않았습니다.\n"));
        break;
    }

    // 정렬된 파일 목록을 처리
    for (const auto& fileInfo : files) {
        LoadJsonFile(fileInfo.filePath);
    }

    CString msg;
    msg.Format(_T("총 %d개의 JSON 파일을 스캔했습니다.\n"), static_cast<int>(files.size()));
    OutputDebugString(msg);
}

size_t CJsonFileManager::GetPendingCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pendingQueue.size();
}

JsonFileData CJsonFileManager::GetNextPendingFile()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_pendingQueue.empty()) {
        return JsonFileData(); // 빈 데이터 반환
    }

    CString filePath = m_pendingQueue.front();
    m_pendingQueue.pop_front();

    auto it = m_jsonFiles.find(filePath);
    if (it != m_jsonFiles.end()) {
        // 원본을 유지하기 위해 복사본 반환
        return it->second;
    }

    return JsonFileData();
}

void CJsonFileManager::MarkFileAsProcessed(const CString& filePath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_jsonFiles.find(filePath);
    if (it != m_jsonFiles.end()) {
        it->second.processed = true;
    }
}

void CJsonFileManager::ClearProcessedFiles()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 처리완료된 파일만 제거
    auto it = m_jsonFiles.begin();
    while (it != m_jsonFiles.end()) {
        if (it->second.processed) {
            it = m_jsonFiles.erase(it);
        }
        else {
            ++it;
        }
    }
}

time_t CJsonFileManager::GetFileModifiedTime(const CString& filePath)
{
    CFileStatus status;
    if (CFile::GetStatus(filePath, status)) {
        SYSTEMTIME systemTime;
        status.m_mtime.GetAsSystemTime(systemTime);

        struct tm timeInfo = { 0 };
        timeInfo.tm_year = systemTime.wYear - 1900;
        timeInfo.tm_mon = systemTime.wMonth - 1;
        timeInfo.tm_mday = systemTime.wDay;
        timeInfo.tm_hour = systemTime.wHour;
        timeInfo.tm_min = systemTime.wMinute;
        timeInfo.tm_sec = systemTime.wSecond;

        return mktime(&timeInfo);
    }
    return 0;
}

time_t CJsonFileManager::GetFileCreationTime(const CString& filePath)
{
    CFileStatus status;
    if (CFile::GetStatus(filePath, status)) {
        SYSTEMTIME systemTime;
        status.m_ctime.GetAsSystemTime(systemTime);

        struct tm timeInfo = { 0 };
        timeInfo.tm_year = systemTime.wYear - 1900;
        timeInfo.tm_mon = systemTime.wMonth - 1;
        timeInfo.tm_mday = systemTime.wDay;
        timeInfo.tm_hour = systemTime.wHour;
        timeInfo.tm_min = systemTime.wMinute;
        timeInfo.tm_sec = systemTime.wSecond;

        return mktime(&timeInfo);
    }
    return 0;
}

std::vector<FileInfo> CJsonFileManager::FindJsonFilesInFolder(const CString& folderPath)
{
    std::vector<FileInfo> files;

    CString path = folderPath;
    if (path.Right(1) != _T("\\")) {
        path += _T("\\");
    }

    CString searchPath = path + _T("*.json");

    CFileFind finder;
    BOOL working = finder.FindFile(searchPath);

    while (working) {
        working = finder.FindNextFile();
        if (finder.IsDots() || finder.IsDirectory()) {
            continue;
        }

        FileInfo fileInfo;
        fileInfo.filePath = finder.GetFilePath();
        fileInfo.creationTime = GetFileCreationTime(fileInfo.filePath);
        fileInfo.modifiedTime = GetFileModifiedTime(fileInfo.filePath);

        files.push_back(fileInfo);
    }

    finder.Close();
    return files;
}

bool CJsonFileManager::CompareByName(const FileInfo& a, const FileInfo& b)
{
    return a.filePath.CompareNoCase(b.filePath) < 0;
}

bool CJsonFileManager::CompareByCreation(const FileInfo& a, const FileInfo& b)
{
    return a.creationTime < b.creationTime;
}

bool CJsonFileManager::CompareByModified(const FileInfo& a, const FileInfo& b)
{
    return a.modifiedTime < b.modifiedTime;
}
