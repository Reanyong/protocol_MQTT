#pragma once

#include "json.hpp"
#include <string>

class CJsonParser
{
public:
    CJsonParser();
    virtual ~CJsonParser();

    // JSON 메시지 파싱 메소드
    bool ParseMessage(const char* payload, int length);

    // 파싱 결과 데이터 구조체
    struct EventData
    {
        // 기본 정보
        std::string code;
        int cid;
        std::string adr;

        // 이벤트 정보
        std::string eventNo;
        std::string srcUrl;

        // 페이로드 데이터
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
    bool ApplyJsonToTags(
        const CString& timerCounterTag = _T("TIMER_COUNTER"),
        const CString& temperatureTag = _T("TEMPERATURE"),
        const CString& ioLinkPdinTag = _T("IOLINK_PDIN"),
        int setNumber = 1,
        const CString& tagGroup = _T("")) const;

private:
    EventData m_eventData;
    bool m_isValid;             // 유효성 검사 결과

    ParseStatus m_parseStatus;  // 파싱 상태
    CString m_errorMessage;     // 오류 메세지
};
