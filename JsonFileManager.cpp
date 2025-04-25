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
        // Check if file exists
        CFileStatus status;
        if (!CFile::GetStatus(filePath, status)) {
            OutputDebugString(_T("File does not exist: "));
            OutputDebugString(filePath);
            OutputDebugString(_T("\n"));
            return false;
        }

        // Check file time
        time_t fileTime = GetFileModifiedTime(filePath);

        // Check if file has already been processed
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_jsonFiles.find(filePath);
            if (it != m_jsonFiles.end() && it->second.lastModified == fileTime && it->second.processed) {
                // Skip if file has already been processed and hasn't changed
                return true;
            }
        }

        // Open file
        CFile file;
        if (!file.Open(filePath, CFile::modeRead | CFile::shareDenyWrite)) {
            OutputDebugString(_T("Cannot open file: "));
            OutputDebugString(filePath);
            OutputDebugString(_T("\n"));
            return false;
        }

        // Check file size and allocate memory
        ULONGLONG fileSize = file.GetLength();
        if (fileSize > 10 * 1024 * 1024) {  // 10MB limit
            OutputDebugString(_T("File size is too large: "));
            OutputDebugString(filePath);
            OutputDebugString(_T("\n"));
            file.Close();
            return false;
        }

        std::vector<char> buffer(static_cast<size_t>(fileSize) + 1, 0);

        // Read file
        file.Read(buffer.data(), static_cast<UINT>(fileSize));
        file.Close();

        // Verify JSON syntax (actual parsing will be done separately later)
        std::string content(buffer.data(), static_cast<size_t>(fileSize));
        try {
            auto json = nlohmann::json::parse(content);

            // Store file data
            JsonFileData fileData;
            fileData.filePath = filePath;
            fileData.content = std::move(content);
            fileData.lastModified = fileTime;
            fileData.processed = false;

            // Add to map and queue
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_jsonFiles[filePath] = std::move(fileData);
                m_pendingQueue.push_back(filePath);
            }

            OutputDebugString(_T("File loaded successfully: "));
            OutputDebugString(filePath);
            OutputDebugString(_T("\n"));
            return true;
        }
        catch (const nlohmann::json::parse_error& e) {
            OutputDebugString(_T("JSON parsing error: "));
            OutputDebugStringA(e.what());
            OutputDebugString(_T("\n"));
            return false;
        }
    }
    catch (const std::exception& e) {
        OutputDebugString(_T("Exception occurred while loading file: "));
        OutputDebugStringA(e.what());
        OutputDebugString(_T("\n"));
        return false;
    }
}

void CJsonFileManager::ScanJsonFolder(const CString& folderPath, FileSortMethod sortMethod)
{
    // Skip processing if folder is empty
    if (folderPath.IsEmpty()) {
        OutputDebugString(_T("Folder path is not specified.\n"));
        return;
    }

    // Find JSON files in folder
    std::vector<FileInfo> files = FindJsonFilesInFolder(folderPath);

    // Sort files according to sort method
    switch (sortMethod) {
    case FileSortMethod::BY_NAME:
        std::sort(files.begin(), files.end(), CompareByName);
        OutputDebugString(_T("Files sorted by name.\n"));
        break;
    case FileSortMethod::BY_CREATION:
        std::sort(files.begin(), files.end(), CompareByCreation);
        OutputDebugString(_T("Files sorted by creation time.\n"));
        break;
    case FileSortMethod::BY_MODIFIED:
        std::sort(files.begin(), files.end(), CompareByModified);
        OutputDebugString(_T("Files sorted by modification time.\n"));
        break;
    case FileSortMethod::NONE:
    default:
        OutputDebugString(_T("Files not sorted.\n"));
        break;
    }

    // Process sorted file list
    for (const auto& fileInfo : files) {
        LoadJsonFile(fileInfo.filePath);
    }

    CString msg;
    msg.Format(_T("Scanned a total of %d JSON files.\n"), static_cast<int>(files.size()));
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
        return JsonFileData(); // Return empty data
    }

    CString filePath = m_pendingQueue.front();
    m_pendingQueue.pop_front();

    auto it = m_jsonFiles.find(filePath);
    if (it != m_jsonFiles.end()) {
        // Return a copy to preserve the original
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

    // Remove only processed files
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

size_t CJsonFileManager::GetTotalJsonCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_jsonFiles.size();
}
