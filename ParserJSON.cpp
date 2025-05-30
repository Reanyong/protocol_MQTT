#include "pch.h"
#include "ParserJSON.h"
#include "ConfigManager.h"
#include "JsonPathUtil.h"  // JsonPathUtil.h 대신 JsonUtil.h 사용
#include "ErrorMessages.h"
#include <sstream>
#include <string>

CJsonParser::CJsonParser()
{
    m_isValid = false;
    m_parseStatus = PARSE_SUCCESS;
    m_errorMessage = _T("");
}

CJsonParser::~CJsonParser()
{
}

bool CJsonParser::ParseMessage(const char* payload, int length)
{
    try {
        // 문자열로부터 JSON 파싱
        std::string jsonStr(payload, length);
        m_jsonData = nlohmann::json::parse(jsonStr);

        m_parseStatus = PARSE_SUCCESS;  // 초기값은 성공으로 설정
        m_errorMessage = _T("");
        m_isValid = true;

        // 기존 EventData 구조체도 채우기 (하위 호환성을 위해)
        /*FillEventDataFromJson();*/

        return true;
    }
    catch (const nlohmann::json::parse_error& e) {
        // JSON 파싱 오류
        m_parseStatus = PARSE_JSON_ERROR;
        m_errorMessage.Format(_T("JSON 파싱 오류: %hs"), e.what());
        m_isValid = false;
        TRACE("JSON 파싱 오류: %s\n", e.what());
        return false;
    }
    catch (const nlohmann::json::type_error& e) {
        // JSON 타입 오류
        m_parseStatus = PARSE_TYPE_ERROR;
        m_errorMessage.Format(_T("JSON 타입 오류: %hs"), e.what());
        m_isValid = false;
        TRACE("JSON 타입 오류: %s\n", e.what());
        return false;
    }
    catch (const std::exception& e) {
        // 기타 예외
        m_parseStatus = PARSE_JSON_ERROR;
        m_errorMessage.Format(_T("예외 발생: %hs"), e.what());
        m_isValid = false;
        TRACE("예외 발생: %s\n", e.what());
        return false;
    }
}

// *** 새로운 메서드들 구현 ***

bool CJsonParser::ApplyJsonToTagsUsingMapping() const
{
    if (!m_isValid) {
        return false;
    }

    CConfigManager& configManager = CConfigManager::GetInstance();
    std::map<CString, CString> tagMappings = configManager.GetAllTagMappings();

    bool anySuccess = false;
    int successCount = 0;
    int totalCount = tagMappings.size();

    for (const auto& mapping : tagMappings) {
        const CString& tagName = mapping.first;
        const CString& jsonPath = mapping.second;

        if (ApplyValueToTag(tagName, jsonPath)) {
            successCount++;
            anySuccess = true;
            TRACE("태그 적용 성공: %s <- %s\n", tagName, jsonPath);
        }
        else {
            TRACE("태그 적용 실패: %s <- %s\n", tagName, jsonPath);
        }
    }

    TRACE("태그 매핑 결과: %d/%d 성공\n", successCount, totalCount);
    return anySuccess;
}

bool CJsonParser::ApplyValueToTag(const CString& tagName, const CString& jsonPath) const
{
    if (!m_isValid) {
        return false;
    }

    try {
        // JSONPath로 값 추출
        CString strValue;
        if (CJsonPathUtil::ExtractValue(m_jsonData, jsonPath, strValue)) {
            return ApplyValueToEasyViewTag(tagName, strValue);
        }

        // 문자열 추출이 실패하면 숫자로 시도
        double numValue;
        if (CJsonPathUtil::ExtractValue(m_jsonData, jsonPath, numValue)) {
            return ApplyValueToEasyViewTag(tagName, numValue);
        }

        return false;
    }
    catch (const std::exception& e) {
        TRACE("태그 값 적용 오류: %s\n", e.what());
        return false;
    }
}

bool CJsonParser::ApplyAllMappedTags() const
{
    return ApplyJsonToTagsUsingMapping();
}

bool CJsonParser::GetValueByPath(const CString& jsonPath, CString& outValue) const
{
    if (!m_isValid) {
        return false;
    }

    return CJsonPathUtil::ExtractValue(m_jsonData, jsonPath, outValue);
}

bool CJsonParser::GetValueByPath(const CString& jsonPath, int& outValue) const
{
    if (!m_isValid) {
        return false;
    }

    return CJsonPathUtil::ExtractValue(m_jsonData, jsonPath, outValue);
}

bool CJsonParser::GetValueByPath(const CString& jsonPath, double& outValue) const
{
    if (!m_isValid) {
        return false;
    }

    return CJsonPathUtil::ExtractValue(m_jsonData, jsonPath, outValue);
}

bool CJsonParser::ApplyValueToEasyViewTag(const CString& tagName, const CString& value) const
{
    ST_EV_TAG_INFO tagInfo;
    if (EV_GetTagInfo(tagName, &tagInfo) <= 0) {
        TRACE("태그를 찾을 수 없음: %s\n", tagName);
        return false;
    }

    try {
        switch (tagInfo.nTagType) {
        case TYPE_DI:
        case TYPE_DO:
        {
            // 문자열을 논리값으로 변환
            int boolValue = 0;
            if (value.CompareNoCase(_T("true")) == 0 || value == _T("1")) {
                boolValue = 1;
            }
            else if (value.CompareNoCase(_T("false")) == 0 || value == _T("0")) {
                boolValue = 0;
            }
            else {
                // 숫자로 변환 시도
                boolValue = _ttoi(value) ? 1 : 0;
            }
            EV_PutSBDiValue(tagInfo.nStnPos, tagInfo.nTagPos, boolValue);
            TRACE("DI/DO 태그 적용: %s = %d\n", tagName, boolValue);
            return true;
        }
        case TYPE_AI:
        case TYPE_AO:
        {
            double numValue = _ttof(value);
            EV_PutSBAiValue(tagInfo.nStnPos, tagInfo.nTagPos, numValue);
            TRACE("AI/AO 태그 적용: %s = %f\n", tagName, numValue);
            return true;
        }
        case TYPE_SI:
        {
            CT2A utf8Value(value, CP_UTF8);
            EV_PutSBString(tagInfo.nStnPos, tagInfo.nTagPos * 2,
                utf8Value, strlen(utf8Value));
            TRACE("SI 태그 적용: %s = %s\n", tagName, value);
            return true;
        }
        default:
            TRACE("지원하지 않는 태그 타입: %s (타입: %d)\n", tagName, tagInfo.nTagType);
            return false;
        }
    }
    catch (const std::exception& e) {
        TRACE("태그 값 설정 오류: %s\n", e.what());
        return false;
    }
}

bool CJsonParser::ApplyValueToEasyViewTag(const CString& tagName, int value) const
{
    ST_EV_TAG_INFO tagInfo;
    if (EV_GetTagInfo(tagName, &tagInfo) <= 0) {
        TRACE("태그를 찾을 수 없음: %s\n", tagName);
        return false;
    }

    try {
        switch (tagInfo.nTagType) {
        case TYPE_DI:
        case TYPE_DO:
            EV_PutSBDiValue(tagInfo.nStnPos, tagInfo.nTagPos, value ? 1 : 0);
            TRACE("DI/DO 태그 적용: %s = %d\n", tagName, value ? 1 : 0);
            return true;
        case TYPE_AI:
        case TYPE_AO:
            EV_PutSBAiValue(tagInfo.nStnPos, tagInfo.nTagPos, static_cast<double>(value));
            TRACE("AI/AO 태그 적용: %s = %d\n", tagName, value);
            return true;
        case TYPE_SI:
        {
            CString strValue;
            strValue.Format(_T("%d"), value);
            CT2A utf8Value(strValue, CP_UTF8);
            EV_PutSBString(tagInfo.nStnPos, tagInfo.nTagPos * 2,
                utf8Value, strlen(utf8Value));
            TRACE("SI 태그 적용: %s = %d\n", tagName, value);
            return true;
        }
        default:
            TRACE("지원하지 않는 태그 타입: %s (타입: %d)\n", tagName, tagInfo.nTagType);
            return false;
        }
    }
    catch (const std::exception& e) {
        TRACE("태그 값 설정 오류: %s\n", e.what());
        return false;
    }
}

bool CJsonParser::ApplyValueToEasyViewTag(const CString& tagName, double value) const
{
    ST_EV_TAG_INFO tagInfo;
    if (EV_GetTagInfo(tagName, &tagInfo) <= 0) {
        TRACE("태그를 찾을 수 없음: %s\n", tagName);
        return false;
    }

    try {
        switch (tagInfo.nTagType) {
        case TYPE_DI:
        case TYPE_DO:
            EV_PutSBDiValue(tagInfo.nStnPos, tagInfo.nTagPos, value != 0.0 ? 1 : 0);
            TRACE("DI/DO 태그 적용: %s = %d\n", tagName, value != 0.0 ? 1 : 0);
            return true;
        case TYPE_AI:
        case TYPE_AO:
            EV_PutSBAiValue(tagInfo.nStnPos, tagInfo.nTagPos, value);
            TRACE("AI/AO 태그 적용: %s = %f\n", tagName, value);
            return true;
        case TYPE_SI:
        {
            CString strValue;
            strValue.Format(_T("%.6f"), value);
            CT2A utf8Value(strValue, CP_UTF8);
            EV_PutSBString(tagInfo.nStnPos, tagInfo.nTagPos * 2,
                utf8Value, strlen(utf8Value));
            TRACE("SI 태그 적용: %s = %f\n", tagName, value);
            return true;
        }
        default:
            TRACE("지원하지 않는 태그 타입: %s (타입: %d)\n", tagName, tagInfo.nTagType);
            return false;
        }
    }
    catch (const std::exception& e) {
        TRACE("태그 값 설정 오류: %s\n", e.what());
        return false;
    }
}

// *** 기존 메서드들 (하위 호환성을 위해 유지) ***

//void CJsonParser::FillEventDataFromJson()
//{
//    try {
//        // 기본 정보 추출
//        if (m_jsonData.contains("code")) {
//            if (m_jsonData["code"].is_string()) {
//                m_eventData.code = m_jsonData["code"];
//            }
//        }
//
//        if (m_jsonData.contains("cid")) {
//            if (m_jsonData["cid"].is_number()) {
//                m_eventData.cid = m_jsonData["cid"];
//            }
//        }
//
//        if (m_jsonData.contains("adr")) {
//            if (m_jsonData["adr"].is_string()) {
//                m_eventData.adr = m_jsonData["adr"];
//            }
//        }
//
//        // data 객체 처리
//        if (m_jsonData.contains("data") && m_jsonData["data"].is_object()) {
//            const auto& data = m_jsonData["data"];
//
//            if (data.contains("eventno")) {
//                m_eventData.eventNo = data["eventno"];
//            }
//
//            if (data.contains("srcurl")) {
//                m_eventData.srcUrl = data["srcurl"];
//            }
//
//            // payload 처리
//            if (data.contains("payload") && data["payload"].is_object()) {
//                const auto& payload = data["payload"];
//
//                // 타이머 카운터
//                if (payload.contains("/timer[1]/counter")) {
//                    const auto& timer = payload["/timer[1]/counter"];
//                    if (timer.is_object() && timer.contains("code") && timer.contains("data")) {
//                        m_eventData.timerCounter.code = timer["code"];
//                        m_eventData.timerCounter.data = timer["data"];
//                        m_eventData.timerCounter.valid = true;
//                    }
//                }
//
//                // 온도
//                if (payload.contains("/processdatamaster/temperature")) {
//                    const auto& temp = payload["/processdatamaster/temperature"];
//                    if (temp.is_object() && temp.contains("code") && temp.contains("data")) {
//                        m_eventData.temperature.code = temp["code"];
//                        m_eventData.temperature.data = temp["data"];
//                        m_eventData.temperature.valid = true;
//                    }
//                }
//
//                // IOLink
//                if (payload.contains("/iolinkmaster/port[2]/iolinkdevice/pdin")) {
//                    const auto& iolink = payload["/iolinkmaster/port[2]/iolinkdevice/pdin"];
//                    if (iolink.is_object() && iolink.contains("code") && iolink.contains("data")) {
//                        m_eventData.iolinkDevice.code = iolink["code"];
//                        if (iolink["data"].is_string()) {
//                            m_eventData.iolinkDevice.data = iolink["data"];
//                        }
//                        else if (iolink["data"].is_number()) {
//                            m_eventData.iolinkDevice.data = std::to_string(iolink["data"].get<int>());
//                        }
//                        m_eventData.iolinkDevice.valid = true;
//                    }
//                }
//            }
//        }
//    }
//    catch (const std::exception& e) {
//        TRACE("EventData 채우기 오류: %s\n", e.what());
//    }
//}

bool CJsonParser::ApplyJsonToTags(
    const CString& timerCounterTag,
    const CString& temperatureTag,
    const CString& ioLinkPdinTag,
    int setNumber,
    const CString& tagGroup) const
{
    // 새로운 매핑 시스템 우선 사용
    bool newSystemResult = ApplyJsonToTagsUsingMapping();
    if (newSystemResult) {
        return true;
    }

    // 새로운 시스템이 실패하면 기존 방식으로 fallback
    if (!m_isValid) {
        return false;
    }

    // 이벤트 코드가 "event"인 경우에만 처리 (기존 로직)
    if (m_eventData.code != "event") {
        return false;
    }

    bool result = false;
    ST_EV_TAG_INFO tagInfo;

    // 타이머 카운터 처리
    if (m_eventData.timerCounter.valid && m_eventData.timerCounter.code == 200) {
        if (EV_GetTagInfo(timerCounterTag, &tagInfo) > 0) {
            switch (tagInfo.nTagType) {
            case TYPE_DI:
            case TYPE_DO:
                EV_PutSBDiValue(tagInfo.nStnPos, tagInfo.nTagPos, m_eventData.timerCounter.data ? 1 : 0);
                break;
            case TYPE_AI:
            case TYPE_AO:
                EV_PutSBAiValue(tagInfo.nStnPos, tagInfo.nTagPos, m_eventData.timerCounter.data);
                break;
            }
            result = true;
        }
    }

    // 온도 데이터 처리
    if (m_eventData.temperature.valid && m_eventData.temperature.code == 200) {
        if (EV_GetTagInfo(temperatureTag, &tagInfo) > 0) {
            if (tagInfo.nTagType == TYPE_AI || tagInfo.nTagType == TYPE_AO) {
                EV_PutSBAiValue(tagInfo.nStnPos, tagInfo.nTagPos, m_eventData.temperature.data);
                result = true;
            }
        }
    }

    // IOLink 데이터 처리
    if (m_eventData.iolinkDevice.valid && m_eventData.iolinkDevice.code == 200) {
        if (EV_GetTagInfo(ioLinkPdinTag, &tagInfo) > 0) {
            if (tagInfo.nTagType == TYPE_AI || tagInfo.nTagType == TYPE_AO) {
                // 문자열을 숫자로 변환
                double value = 0.0;
                bool conversionSuccess = true;

                // 모든 문자가 16진수에 유효한지 확인하는 함수
                auto isHexString = [](const std::string& str) {
                    return str.find_first_not_of("0123456789ABCDEFabcdef") == std::string::npos;
                };

                // 접두사 확인과 16진수 판별
                bool isHex = false;
                std::string dataStr = m_eventData.iolinkDevice.data;

                // "0x" 또는 "0X" 접두사가 있는 경우
                if (dataStr.length() > 2 && (dataStr.substr(0, 2) == "0x" || dataStr.substr(0, 2) == "0X")) {
                    dataStr = dataStr.substr(2);  // 접두사 제거
                    isHex = true;
                }
                // 접두사 없이 16진수 형태인지 확인 (A-F 문자 포함)
                else if (isHexString(dataStr) &&
                    dataStr.find_first_of("ABCDEFabcdef") != std::string::npos) {
                    isHex = true;
                }

                // 16진수로 처리
                if (isHex) {
                    unsigned int hexValue = 0;
                    if (sscanf_s(dataStr.c_str(), "%x", &hexValue) == 1) {
                        value = static_cast<double>(hexValue);
                        TRACE("16진수 문자열 '%s'를 숫자 %f로 변환\n",
                            m_eventData.iolinkDevice.data.c_str(), value);
                    }
                    else {
                        TRACE("16진수 변환 실패: '%s'\n", m_eventData.iolinkDevice.data.c_str());
                        conversionSuccess = false;
                    }
                }
                // 10진수로 처리
                else {
                    try {
                        value = atof(m_eventData.iolinkDevice.data.c_str());
                        TRACE("10진수 문자열 '%s'를 숫자 %f로 변환\n",
                            m_eventData.iolinkDevice.data.c_str(), value);
                    }
                    catch (...) {
                        TRACE("10진수 변환 실패: '%s'\n", m_eventData.iolinkDevice.data.c_str());
                        conversionSuccess = false;
                    }
                }

                // 변환 성공 시 값 저장
                if (conversionSuccess) {
                    EV_PutSBAiValue(tagInfo.nStnPos, tagInfo.nTagPos, value);
                    TRACE("IOLink 데이터를 AI/AO 태그에 값 %f로 저장\n", value);
                    result = true;
                }
            }
            else if (tagInfo.nTagType == TYPE_SI) {
                // 기존 문자열 처리 유지
                EV_PutSBString(tagInfo.nStnPos, tagInfo.nTagPos * 2,
                    m_eventData.iolinkDevice.data.c_str(),
                    m_eventData.iolinkDevice.data.length());
                result = true;
            }
        }
    }

    return result;
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
