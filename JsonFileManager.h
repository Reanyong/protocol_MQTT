#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <deque>
#include "json.hpp"
#include "ConfigManager.h"  // FileSortMethod 열거형 사용

// 파일 정보 구조체
struct FileInfo {
    CString filePath;
    time_t creationTime;
    time_t modifiedTime;

    FileInfo() : creationTime(0), modifiedTime(0) {}
};

// JSON 파일 데이터 구조체
struct JsonFileData
{
    CString filePath;                // 파일 경로
    std::string content;             // 파일 내용
    time_t lastModified;             // 마지막 수정 시간
    bool processed;                  // 처리 여부 플래그

    JsonFileData() : lastModified(0), processed(false) {}
};

class CJsonFileManager
{
public:
    CJsonFileManager();
    virtual ~CJsonFileManager();

    // 파일 관리
    bool LoadJsonFile(const CString& filePath);
    void ScanJsonFolder(const CString& folderPath, FileSortMethod sortMethod = FileSortMethod::BY_NAME);

    // 처리 대기 큐 관련
    size_t GetPendingCount() const;
    JsonFileData GetNextPendingFile();
    void MarkFileAsProcessed(const CString& filePath);
    void ClearProcessedFiles();

    // 싱글톤 패턴
    static CJsonFileManager& GetInstance();

    // total json file convert
    size_t GetTotalJsonCount() const;

    struct FileProcessResult
    {
        CString filePath;      // 파일 경로
        bool hasError;         // 오류 포함 여부
        CString errorMessage;  // 오류 메시지

        FileProcessResult() : hasError(false) {}
    };

    void AddProcessResult(const CString& filePath, bool hasError, const CString& errorMessage);
    bool GetFileProcessResult(const CString& filePath, FileProcessResult& result) const;

    // 오류가 있는 파일만 가져오는 메소드
    std::vector<FileProcessResult> GetErrorFiles() const;

    // 파일 처리 결과 초기화
    void ClearProcessResults();

private:
    std::map<CString, JsonFileData> m_jsonFiles;     // 파일 데이터 맵 (키: 파일경로)
    std::deque<CString> m_pendingQueue;              // 처리 대기 큐
    mutable std::mutex m_mutex;                      // 스레드 동기화 뮤텍스

    // 파일 시간 확인
    time_t GetFileModifiedTime(const CString& filePath);
    time_t GetFileCreationTime(const CString& filePath);

    // 파일 목록 정렬 함수
    static bool CompareByName(const FileInfo& a, const FileInfo& b);
    static bool CompareByCreation(const FileInfo& a, const FileInfo& b);
    static bool CompareByModified(const FileInfo& a, const FileInfo& b);

    // 폴더에서 JSON 파일 찾기
    std::vector<FileInfo> FindJsonFilesInFolder(const CString& folderPath);

    std::map<CString, FileProcessResult> m_processResults;
};
