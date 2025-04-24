#include "pch.h"
#include "ParserJSON.h"

CJsonParser::CJsonParser()
{
}

CJsonParser::~CJsonParser()
{
}

bool CJsonParser::ParseMessage(const char* payload, int length)
{
    try {
        // 문자열로부터 JSON 파싱
        std::string jsonStr(payload, length);
        nlohmann::json jsonData = nlohmann::json::parse(jsonStr);

        // 기본 필드 파싱
        if (jsonData.contains("code")) {
            m_eventData.code = jsonData["code"];
        }

        if (jsonData.contains("cid")) {
            m_eventData.cid = jsonData["cid"];
        }

        if (jsonData.contains("adr")) {
            m_eventData.adr = jsonData["adr"];
        }

        // "data" 객체 파싱
        if (jsonData.contains("data") && jsonData["data"].is_object()) {
            const auto& data = jsonData["data"];

            // eventno 처리
            if (data.contains("eventno")) {
                m_eventData.eventNo = data["eventno"];
            }

            // srcurl 처리
            if (data.contains("srcurl")) {
                m_eventData.srcUrl = data["srcurl"];
            }

            // payload 처리
            if (data.contains("payload") && data["payload"].is_object()) {
                const auto& payload = data["payload"];

                // 타이머 카운터 값 처리
                if (payload.contains("/timer[1]/counter") && payload["/timer[1]/counter"].is_object()) {
                    const auto& timer = payload["/timer[1]/counter"];
                    if (timer.contains("code") && timer.contains("data")) {
                        m_eventData.timerCounter.code = timer["code"];
                        m_eventData.timerCounter.data = timer["data"];
                        m_eventData.timerCounter.valid = true;
                    }
                }

                // 온도 데이터 처리
                if (payload.contains("/processdatamaster/temperature") &&
                    payload["/processdatamaster/temperature"].is_object()) {
                    const auto& temp = payload["/processdatamaster/temperature"];
                    if (temp.contains("code") && temp.contains("data")) {
                        m_eventData.temperature.code = temp["code"];
                        m_eventData.temperature.data = temp["data"];
                        m_eventData.temperature.valid = true;
                    }
                }

                // IOLink 데이터 처리
                if (payload.contains("/iolinkmaster/port[2]/iolinkdevice/pdin") &&
                    payload["/iolinkmaster/port[2]/iolinkdevice/pdin"].is_object()) {
                    const auto& iolink = payload["/iolinkmaster/port[2]/iolinkdevice/pdin"];
                    if (iolink.contains("code") && iolink.contains("data")) {
                        m_eventData.iolinkDevice.code = iolink["code"];
                        m_eventData.iolinkDevice.data = iolink["data"];
                        m_eventData.iolinkDevice.valid = true;
                    }
                }
            }
        }

        return true;
    }
    catch (const nlohmann::json::parse_error& e) {
        // 파싱 오류 처리
        TRACE("JSON 파싱 오류: %s\n", e.what());
        return false;
    }
    catch (const std::exception& e) {
        // 기타 예외 처리
        TRACE("예외 발생: %s\n", e.what());
        return false;
    }
}

void CJsonParser::TraceEventData()
{
    OutputDebugStringW(L"--- JSON Parsing ---\n");

    wchar_t buffer[1024];

    swprintf_s(buffer, L"Code: %hs\n", m_eventData.code.c_str());
    OutputDebugStringW(buffer);

    swprintf_s(buffer, L"CID: %d\n", m_eventData.cid);
    OutputDebugStringW(buffer);

    swprintf_s(buffer, L"ADR: %hs\n", m_eventData.adr.c_str());
    OutputDebugStringW(buffer);

    swprintf_s(buffer, L"EVENT Number: %hs\n", m_eventData.eventNo.c_str());
    OutputDebugStringW(buffer);

    swprintf_s(buffer, L"Source URL: %hs\n", m_eventData.srcUrl.c_str());
    OutputDebugStringW(buffer);

    if (m_eventData.timerCounter.valid) {
        swprintf_s(buffer, L"Timer Counter - Code: %d, Data: %d\n",
            m_eventData.timerCounter.code,
            m_eventData.timerCounter.data);
        OutputDebugStringW(buffer);
    }

    if (m_eventData.temperature.valid) {
        swprintf_s(buffer, L"Temp - Code: %d, Data: %d\n",
            m_eventData.temperature.code,
            m_eventData.temperature.data);
        OutputDebugStringW(buffer);
    }

    if (m_eventData.iolinkDevice.valid) {
        swprintf_s(buffer, L"IOLink - Code: %d, data: %hs\n",
            m_eventData.iolinkDevice.code,
            m_eventData.iolinkDevice.data.c_str());
        OutputDebugStringW(buffer);
    }

    OutputDebugStringW(L"----------------------\n");
}
