#pragma once
#include <string>

// 파일 정렬 방식 열거형
enum class FileSortMethod {
    BY_NAME,        // 이름 순서
    BY_CREATION,    // 생성 시간 순서
    BY_MODIFIED,    // 수정 시간 순서
    NONE            // 정렬 없음
};

class CConfigManager
{
public:
    CConfigManager();
    virtual ~CConfigManager();

    // 설정 로드/저장 (INI 파일 사용)
    bool LoadConfig();
    bool SaveConfig();

    // JSON 파일 폴더 경로 관리
    void SetJsonFolderPath(const CString& folderPath);
    CString GetJsonFolderPath() const;

    // 정렬 방식 설정
    void SetSortMethod(FileSortMethod sortMethod);
    FileSortMethod GetSortMethod() const;

    // 파싱 주기 설정 (ms)
    void SetParsingInterval(int interval);
    int GetParsingInterval() const;

    // 싱글톤 패턴
    static CConfigManager& GetInstance();

private:
    CString m_jsonFolderPath;
    FileSortMethod m_sortMethod;
    int m_parsingInterval;

    // INI 파일 경로
    CString m_iniFilePath;

    // FileSortMethod를 문자열로 변환
    CString SortMethodToString(FileSortMethod method);
    // 문자열을 FileSortMethod로 변환
    FileSortMethod StringToSortMethod(const CString& methodStr);
};
