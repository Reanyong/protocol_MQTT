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

    // 파싱된 데이터 반환
    const EventData& GetEventData() const { return m_eventData; }

    // 디버그를 위한 파싱 결과 출력
    void TraceEventData();

private:
    EventData m_eventData;
};
