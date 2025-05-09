#pragma once
#include <string>
#include <map>

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

    // 태그 매핑 관련 메서드 추가
    CString GetTagGroup() const;
    void SetTagGroup(const CString& tagGroup);

    // 세트 번호로 JSON 파일 이름 가져오기
    CString GetJsonFileForSet(int setNumber) const;

    // JSON 파일명으로 세트 번호 가져오기
    int GetSetNumberForJsonFile(const CString& jsonFileName) const;

    // 세트 번호로 태그 이름 생성
    void GetTagNamesForSet(int setNumber, CString& timerCounterTag,
        CString& temperatureTag, CString& ioLinkPdinTag) const;

    // 모든 태그셋 로드/저장 (INI 파일 TagInfo 섹션)
    bool LoadTagSets();
    bool SaveTagSets();

    // 태그셋 추가
    void AddTagSet(int setNumber, const CString& jsonFileName);

    // 태그셋 삭제
    void RemoveTagSet(int setNumber);

    // 태그셋 개수 반환
    int GetTagSetCount() const;

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

    CString m_tagGroup;                       // 태그 그룹 이름
    std::map<int, CString> m_setToJsonFile;   // 세트 번호 -> JSON 파일명
    std::map<CString, int> m_jsonFileToSet;   // JSON 파일명 -> 세트 번호

    CString m_mqttTopic;
    CString m_mqttIp;
    int m_mqttPort;
    int m_mqttKeepAlive;

public:
    // MQTT Config Get & Set
    void SetMqttTopic(const CString& topic);
    CString GetMqttTopic() const;

    void SetMqttIp(const CString& ip);
    CString GetMqttIp() const;

    void SetMqttPort(int port);
    int GetMqttPort() const;

    void SetMqttKeepAlive(int keepAlive);
    int GetMqttKeepAlive() const;
};
