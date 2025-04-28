#include "pch.h"
#include "ParserJSON.h"
#include "ErrorMessages.h"

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

        m_parseStatus = PARSE_SUCCESS;  // 초기값은 성공으로 설정
        m_errorMessage = _T("");

        // 스키마 검증
        bool hasRequiredFields = true;

        // code 필드 검증
        if (!jsonData.contains("code")) {
            hasRequiredFields = false;
            m_parseStatus = PARSE_SCHEMA_ERROR;
            m_errorMessage = _T("필수 필드 'code'가 없습니다");
        }
        else {
            m_eventData.code = jsonData["code"];
        }

        // cid 필드 검증
        if (!jsonData.contains("cid")) {
            hasRequiredFields = false;
            m_parseStatus = PARSE_SCHEMA_ERROR;
            m_errorMessage = _T("필수 필드 'cid'가 없습니다");
        }
        else {
            // cid가 숫자인지 확인하고 처리
            if (jsonData["cid"].is_number()) {
                m_eventData.cid = jsonData["cid"];
            }
            else if (jsonData["cid"].is_string()) {
                // 문자열을 숫자로 변환 시도
                try {
                    m_eventData.cid = std::stoi(jsonData["cid"].get<std::string>());
                    m_parseStatus = PARSE_TYPE_ERROR;
                    m_errorMessage = _T("'cid' 필드가 문자열입니다. 숫자 형식이어야 합니다");
                }
                catch (const std::exception& e) {
                    m_eventData.cid = 0;
                    m_parseStatus = PARSE_TYPE_ERROR;
                    m_errorMessage = _T("'cid' 필드 변환 오류: 유효한 숫자가 아닙니다");
                }
            }
            else {
                m_eventData.cid = 0;
                m_parseStatus = PARSE_TYPE_ERROR;
                m_errorMessage = _T("'cid' 필드 타입 오류: 숫자 또는 숫자 문자열이어야 합니다");
            }
        }

        // adr 필드 검증
        if (!jsonData.contains("adr")) {
            hasRequiredFields = false;
            m_parseStatus = PARSE_SCHEMA_ERROR;
            m_errorMessage = _T("필수 필드 'adr'이 없습니다");
        }
        else {
            m_eventData.adr = jsonData["adr"];
        }

        // data 객체 검증
        if (!jsonData.contains("data")) {
            hasRequiredFields = false;
            m_parseStatus = PARSE_SCHEMA_ERROR;
            m_errorMessage = _T("필수 필드 'data'가 없습니다");
        }
        else if (!jsonData["data"].is_object()) {
            m_parseStatus = PARSE_TYPE_ERROR;
            m_errorMessage = _T("'data' 필드가 객체 형식이 아닙니다");
        }
        else {
            const auto& data = jsonData["data"];

            // eventno 필드 검증
            if (!data.contains("eventno")) {
                m_parseStatus = PARSE_SCHEMA_ERROR;
                m_errorMessage = _T("'data.eventno' 필드가 없습니다");
            }
            else {
                m_eventData.eventNo = data["eventno"];
            }

            // srcurl 필드 검증
            if (!data.contains("srcurl")) {
                m_parseStatus = PARSE_SCHEMA_ERROR;
                m_errorMessage = _T("'data.srcurl' 필드가 없습니다");
            }
            else {
                m_eventData.srcUrl = data["srcurl"];
            }

            // payload 필드 처리 (선택 사항)
            if (data.contains("payload")) {
                if (!data["payload"].is_object()) {
                    m_parseStatus = PARSE_TYPE_ERROR;
                    m_errorMessage = _T("'data.payload' 필드가 객체 형식이 아닙니다");
                }
                else {
                    const auto& payload = data["payload"];

                    // 타이머 카운터 값 처리
                    if (payload.contains("/timer[1]/counter")) {
                        const auto& timer = payload["/timer[1]/counter"];
                        if (!timer.is_object()) {
                            m_parseStatus = PARSE_TYPE_ERROR;
                            m_errorMessage = _T("'/timer[1]/counter' 필드가 객체 형식이 아닙니다");
                        }
                        else {
                            bool hasCode = timer.contains("code");
                            bool hasData = timer.contains("data");

                            if (!hasCode || !hasData) {
                                m_parseStatus = PARSE_SCHEMA_ERROR;
                                m_errorMessage = _T("타이머 카운터에 필수 필드가 없습니다");
                            }
                            else if (!timer["code"].is_number() || !timer["data"].is_number()) {
                                m_parseStatus = PARSE_TYPE_ERROR;
                                m_errorMessage = _T("타이머 카운터 필드가 숫자 형식이 아닙니다");
                            }
                            else {
                                m_eventData.timerCounter.code = timer["code"];
                                m_eventData.timerCounter.data = timer["data"];
                                m_eventData.timerCounter.valid = true;
                            }
                        }
                    }

                    // 온도 데이터 처리
                    if (payload.contains("/processdatamaster/temperature")) {
                        const auto& temp = payload["/processdatamaster/temperature"];
                        if (!temp.is_object()) {
                            m_parseStatus = PARSE_TYPE_ERROR;
                            m_errorMessage = _T("'/processdatamaster/temperature' 필드가 객체 형식이 아닙니다");
                        }
                        else {
                            bool hasCode = temp.contains("code");
                            bool hasData = temp.contains("data");

                            if (!hasCode || !hasData) {
                                m_parseStatus = PARSE_SCHEMA_ERROR;
                                m_errorMessage = _T("온도 데이터에 필수 필드가 없습니다");
                            }
                            else if (!temp["code"].is_number() || !temp["data"].is_number()) {
                                m_parseStatus = PARSE_TYPE_ERROR;
                                m_errorMessage = _T("온도 데이터 필드가 숫자 형식이 아닙니다");
                            }
                            else {
                                m_eventData.temperature.code = temp["code"];
                                m_eventData.temperature.data = temp["data"];
                                m_eventData.temperature.valid = true;
                            }
                        }
                    }

                    // IOLink 데이터 처리
                    if (payload.contains("/iolinkmaster/port[2]/iolinkdevice/pdin")) {
                        const auto& iolink = payload["/iolinkmaster/port[2]/iolinkdevice/pdin"];
                        if (!iolink.is_object()) {
                            m_parseStatus = PARSE_TYPE_ERROR;
                            m_errorMessage = _T("'/iolinkmaster/port[2]/iolinkdevice/pdin' 필드가 객체 형식이 아닙니다");
                        }
                        else {
                            bool hasCode = iolink.contains("code");
                            bool hasData = iolink.contains("data");

                            if (!hasCode || !hasData) {
                                m_parseStatus = PARSE_SCHEMA_ERROR;
                                m_errorMessage = _T("IOLink 데이터에 필수 필드가 없습니다");
                            }
                            else {
                                try {
                                    // code 필드 처리
                                    if (iolink["code"].is_number()) {
                                        m_eventData.iolinkDevice.code = iolink["code"];
                                    }
                                    else {
                                        m_parseStatus = PARSE_TYPE_ERROR;
                                        m_errorMessage = _T("IOLink 'code' 필드가 숫자 형식이 아닙니다");
                                        m_eventData.iolinkDevice.code = 0;
                                    }

                                    // data 필드 처리
                                    if (iolink["data"].is_string()) {
                                        m_eventData.iolinkDevice.data = iolink["data"];
                                    }
                                    else if (iolink["data"].is_number()) {
                                        m_eventData.iolinkDevice.data = std::to_string(iolink["data"].get<int>());
                                    }
                                    else {
                                        m_parseStatus = PARSE_TYPE_ERROR;
                                        m_errorMessage = _T("IOLink 'data' 필드가 문자열이나 숫자 형식이 아닙니다");
                                        m_eventData.iolinkDevice.data = "unknown";
                                    }

                                    m_eventData.iolinkDevice.valid = true;
                                }
                                catch (const std::exception& e) {
                                    m_parseStatus = PARSE_TYPE_ERROR;
                                    m_errorMessage.Format(_T("IOLink 처리 오류: %hs"), e.what());
                                    m_eventData.iolinkDevice.valid = false;
                                }
                            }
                        }
                    }
                }
            }
        }

        // 파싱 결과 반환 (스키마 오류나 타입 오류가 있어도 기본 파싱은 성공한 것으로 간주)
        return true;
    }
    catch (const nlohmann::json::parse_error& e) {
        // JSON 파싱 오류
        m_parseStatus = PARSE_JSON_ERROR;
        m_errorMessage.Format(_T("JSON 파싱 오류: %hs"), e.what());
        TRACE("JSON 파싱 오류: %s\n", e.what());
        return false;
    }
    catch (const nlohmann::json::type_error& e) {
        // JSON 타입 오류
        m_parseStatus = PARSE_TYPE_ERROR;
        m_errorMessage.Format(_T("JSON 타입 오류: %hs"), e.what());
        TRACE("JSON 타입 오류: %s\n", e.what());
        return false;
    }
    catch (const std::exception& e) {
        // 기타 예외
        m_parseStatus = PARSE_JSON_ERROR;
        m_errorMessage.Format(_T("예외 발생: %hs"), e.what());
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

bool CJsonParser::ApplyJsonToTags() const
{
    // JSON 파싱
    CJsonParser jsonParser;

    // 이벤트 코드가 "event"인 경우에만 처리
    const CJsonParser::EventData& eventData = this->GetEventData(); // this-> 또는 m_eventData 직접 참조

    if (eventData.code != "event") {
        return false;
    }

    bool result = false;
    ST_EV_TAG_INFO tagInfo;

    /*
    EasyView3.5 Connection TagStn Test
    TRACE("Checking tag: TIMER_COUNTER\n");
    int tagResult = EV_GetTagInfo("TIMER_COUNTER", &tagInfo);
    if (tagResult > 0) {
        TRACE("Found TIMER_COUNTER tag: Type=%d, StnPos=%d, TagPos=%d\n",
            tagInfo.nTagType, tagInfo.nStnPos, tagInfo.nTagPos);
    }
    else {
        TRACE("TIMER_COUNTER tag not found: %d\n", tagResult);
    }
    */

    // 타이머 카운터 처리
    if (eventData.timerCounter.valid && eventData.timerCounter.code == 200) {
        // 태그 정보 조회
        if (EV_GetTagInfo("TIMER_COUNTER", &tagInfo) > 0) {
            // 태그 유형에 따라 처리
            switch (tagInfo.nTagType) {
            case TYPE_DI:
            case TYPE_DO:
                EV_PutSBDiValue(tagInfo.nStnPos, tagInfo.nTagPos, eventData.timerCounter.data ? 1 : 0);
                break;
            case TYPE_AI:
            case TYPE_AO:
                EV_PutSBAiValue(tagInfo.nStnPos, tagInfo.nTagPos, eventData.timerCounter.data);
                break;
            }
            result = true;
        }
    }

    // 온도 데이터 처리
    if (eventData.temperature.valid && eventData.temperature.code == 200) {
        if (EV_GetTagInfo("TEMPERATURE", &tagInfo) > 0) {
            if (tagInfo.nTagType == TYPE_AI || tagInfo.nTagType == TYPE_AO) {
                EV_PutSBAiValue(tagInfo.nStnPos, tagInfo.nTagPos, eventData.temperature.data);
                result = true;
            }
        }
    }

    // IOLink 데이터 처리
    if (eventData.iolinkDevice.valid && eventData.iolinkDevice.code == 200) {
        if (EV_GetTagInfo("IOLINK_PDIN", &tagInfo) > 0) {
            if (tagInfo.nTagType == TYPE_SI) {
                EV_PutSBString(tagInfo.nStnPos, tagInfo.nTagPos * 2,
                               eventData.iolinkDevice.data.c_str(),
                               eventData.iolinkDevice.data.length());
                result = true;
            }
        }
    }

    return result;
}
