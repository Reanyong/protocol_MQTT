#pragma once

#include "json.hpp"
#include <string>
#include <map>

class CJsonParser
{
public:
    CJsonParser();
    virtual ~CJsonParser();

    // JSON 메시지 파싱 메소드
    bool ParseMessage(const char* payload, int length);

    // 파싱 결과 데이터 구조체 (기존 구조 유지)
    struct EventData
    {
        // 기본 정보
        std::string code;
        int cid;
        std::string adr;

        // 이벤트 정보
        std::string eventNo;
        std::string srcUrl;

        // 페이로드 데이터 (하위 호환성을 위해 유지)
        struct TimerCounter {
            int code;
            int data;
            bool valid;
            TimerCounter() : code(0), data(0), valid(false) {}
        } timerCounter;

        struct Temperature {
            int code;
            int data;
            bool valid;
            Temperature() : code(0), data(0), valid(false) {}
        } temperature;

        struct IOLinkDevice {
            int code;
            std::string data;
            bool valid;
            IOLinkDevice() : code(0), valid(false) {}
        } iolinkDevice;

        EventData() : cid(0) {}
    };

    enum ParseStatus {
        PARSE_SUCCESS,       // 정상 파싱 성공
        PARSE_JSON_ERROR,    // JSON 파싱 오류
        PARSE_SCHEMA_ERROR,  // 스키마 유효성 검사 오류
        PARSE_TYPE_ERROR     // 타입 오류
    };

    ParseStatus GetParseStatus() const { return m_parseStatus; }
    CString GetErrorMessage() const { return m_errorMessage; }

    // 파싱된 데이터 반환
    const EventData& GetEventData() const { return m_eventData; }

    // 디버그를 위한 파싱 결과 출력
    void TraceEventData();

    bool IsValid() const { return m_isValid; }

    // *** 기존 메서드 (하위 호환성을 위해 유지) ***
    bool ApplyJsonToTags(
        const CString& timerCounterTag = _T("TIMER_COUNTER"),
        const CString& temperatureTag = _T("TEMPERATURE"),
        const CString& ioLinkPdinTag = _T("IOLINK_PDIN"),
        int setNumber = 1,
        const CString& tagGroup = _T("")) const;

    // *** 새로운 메서드들 - 동적 태그 매핑 시스템 ***

    // 태그 매핑을 사용해서 JSON 데이터를 EasyView 태그에 적용
    bool ApplyJsonToTagsUsingMapping() const;

    // 특정 태그에 JSONPath로 값 적용
    bool ApplyValueToTag(const CString& tagName, const CString& jsonPath) const;

    // 모든 설정된 태그 매핑에 따라 값 적용
    bool ApplyAllMappedTags() const;

    // JSONPath로 값 추출해서 반환
    bool GetValueByPath(const CString& jsonPath, CString& outValue) const;
    bool GetValueByPath(const CString& jsonPath, int& outValue) const;
    bool GetValueByPath(const CString& jsonPath, double& outValue) const;

    // *** Phase 1: MQTT 토픽 기반 처리 메서드들 ***

    // 특정 MQTT 토픽에 대한 모든 태그 매핑 적용
    bool ApplyTopicBasedMapping(const CString& topic) const;

    // MQTT 토픽에 매핑된 태그들 적용 (새로 추가)
    bool ApplyMqttTagMapping(const CString& mqttTopic) const;

    // JSONPath로 값 추출 (동적 경로 지원)
    bool ExtractValueByJSONPath(const CString& jsonPath, CString& outValue) const;

    // 태그에 직접 값 설정 (검증 없이 빠른 처리)
    bool SetTagValueDirect(const CString& tagName, const CString& value) const;

    // JSON 경로에서 배열 인덱스 처리 (/path[1], /path[2] 등)
    bool ExtractArrayValueByPath(const CString& jsonPath, CString& outValue) const;

private:
    EventData m_eventData;
    bool m_isValid;             // 유효성 검사 결과

    ParseStatus m_parseStatus;  // 파싱 상태
    CString m_errorMessage;     // 오류 메세지

    // *** 새로 추가된 멤버 변수 ***
    nlohmann::json m_jsonData;  // 파싱된 JSON 데이터 보관

    // 태그 타입에 따른 값 적용 헬퍼 메서드
    bool ApplyValueToEasyViewTag(const CString& tagName, const CString& value) const;
    bool ApplyValueToEasyViewTag(const CString& tagName, int value) const;
    bool ApplyValueToEasyViewTag(const CString& tagName, double value) const;
};
