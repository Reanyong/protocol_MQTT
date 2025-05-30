#include "pch.h"
#include "JsonPathUtil.h"
#include <sstream>

CJsonPathUtil::CJsonPathUtil()
{
}

CJsonPathUtil::~CJsonPathUtil()
{
}

bool CJsonPathUtil::ExtractValue(const nlohmann::json& jsonData, const CString& jsonPath, CString& outValue)
{
    try {
        std::vector<std::string> pathTokens = ParseJsonPath(jsonPath);
        if (pathTokens.empty()) {
            return false;
        }

        const nlohmann::json* pValue = NavigateToValue(jsonData, pathTokens);
        if (!pValue) {
            return false;
        }

        // 값을 문자열로 변환
        if (pValue->is_string()) {
            outValue = CString(pValue->get<std::string>().c_str());
        }
        else if (pValue->is_number_integer()) {
            outValue.Format(_T("%d"), pValue->get<int>());
        }
        else if (pValue->is_number_float()) {
            outValue.Format(_T("%.6f"), pValue->get<double>());
        }
        else if (pValue->is_boolean()) {
            outValue = pValue->get<bool>() ? _T("true") : _T("false");
        }
        else if (pValue->is_null()) {
            outValue = _T("");
        }
        else {
            // 객체나 배열인 경우 JSON 문자열로 변환
            outValue = CString(pValue->dump().c_str());
        }

        return true;
    }
    catch (const std::exception& e) {
        TRACE("JSONPath 추출 오류: %s\n", e.what());
        return false;
    }
}

bool CJsonPathUtil::ExtractValue(const nlohmann::json& jsonData, const CString& jsonPath, int& outValue)
{
    try {
        std::vector<std::string> pathTokens = ParseJsonPath(jsonPath);
        if (pathTokens.empty()) {
            return false;
        }

        const nlohmann::json* pValue = NavigateToValue(jsonData, pathTokens);
        if (!pValue) {
            return false;
        }

        if (pValue->is_number_integer()) {
            outValue = pValue->get<int>();
            return true;
        }
        else if (pValue->is_number_float()) {
            outValue = static_cast<int>(pValue->get<double>());
            return true;
        }
        else if (pValue->is_string()) {
            // 문자열을 숫자로 변환 시도
            try {
                std::string strValue = pValue->get<std::string>();
                outValue = std::stoi(strValue);
                return true;
            }
            catch (...) {
                return false;
            }
        }

        return false;
    }
    catch (const std::exception& e) {
        TRACE("JSONPath 정수 추출 오류: %s\n", e.what());
        return false;
    }
}

bool CJsonPathUtil::ExtractValue(const nlohmann::json& jsonData, const CString& jsonPath, double& outValue)
{
    try {
        std::vector<std::string> pathTokens = ParseJsonPath(jsonPath);
        if (pathTokens.empty()) {
            return false;
        }

        const nlohmann::json* pValue = NavigateToValue(jsonData, pathTokens);
        if (!pValue) {
            return false;
        }

        if (pValue->is_number()) {
            outValue = pValue->get<double>();
            return true;
        }
        else if (pValue->is_string()) {
            // 문자열을 숫자로 변환 시도
            try {
                std::string strValue = pValue->get<std::string>();

                // 16진수 처리
                if (strValue.length() > 2 && (strValue.substr(0, 2) == "0x" || strValue.substr(0, 2) == "0X")) {
                    unsigned int hexValue = 0;
                    if (sscanf_s(strValue.c_str(), "%x", &hexValue) == 1) {
                        outValue = static_cast<double>(hexValue);
                        return true;
                    }
                }
                // 10진수 처리
                else {
                    outValue = std::stod(strValue);
                    return true;
                }
            }
            catch (...) {
                return false;
            }
        }

        return false;
    }
    catch (const std::exception& e) {
        TRACE("JSONPath 실수 추출 오류: %s\n", e.what());
        return false;
    }
}

bool CJsonPathUtil::IsValidJsonPath(const CString& jsonPath)
{
    if (jsonPath.IsEmpty()) {
        return false;
    }

    // 기본적인 JSONPath 검증
    // $ 로 시작해야 함
    if (jsonPath.GetAt(0) != '$') {
        return false;
    }

    // 간단한 구문 검증
    CString path = jsonPath;
    path.Replace(_T("$"), _T(""));

    // 연속된 점이나 잘못된 문자 확인
    if (path.Find(_T("..")) >= 0) {
        return false;
    }

    return true;
}

std::vector<std::string> CJsonPathUtil::ParseJsonPath(const CString& jsonPath)
{
    std::vector<std::string> tokens;

    if (!IsValidJsonPath(jsonPath)) {
        return tokens;
    }

    // CString을 std::string으로 변환
    std::string path = CT2A(jsonPath);

    // $ 제거
    if (path[0] == '$') {
        path = path.substr(1);
    }

    // 빈 경로면 루트 반환
    if (path.empty() || path == ".") {
        return tokens;
    }

    // 점으로 시작하면 제거
    if (path[0] == '.') {
        path = path.substr(1);
    }

    // 토큰 분리
    std::stringstream ss(path);
    std::string token;

    while (std::getline(ss, token, '.')) {
        if (!token.empty()) {
            // 특수 키 처리 (슬래시가 포함된 키 등)
            token = ProcessSpecialKey(token);
            tokens.push_back(token);
        }
    }

    return tokens;
}

const nlohmann::json* CJsonPathUtil::NavigateToValue(const nlohmann::json& jsonData,
    const std::vector<std::string>& pathTokens)
{
    const nlohmann::json* current = &jsonData;

    for (const auto& token : pathTokens) {
        if (!current) {
            return nullptr;
        }

        // 배열 인덱스 처리
        if (IsArrayIndex(token)) {
            int index = ParseArrayIndex(token);
            if (current->is_array() && index >= 0 && index < static_cast<int>(current->size())) {
                current = &(*current)[index];
            }
            else {
                return nullptr;
            }
        }
        // 객체 키 처리
        else {
            if (current->is_object() && current->contains(token)) {
                current = &(*current)[token];
            }
            else {
                return nullptr;
            }
        }
    }

    return current;
}

int CJsonPathUtil::ParseArrayIndex(const std::string& token)
{
    // [숫자] 형태에서 숫자 추출
    if (token.length() >= 3 && token[0] == '[' && token[token.length() - 1] == ']') {
        std::string indexStr = token.substr(1, token.length() - 2);
        try {
            return std::stoi(indexStr);
        }
        catch (...) {
            return -1;
        }
    }
    return -1;
}

bool CJsonPathUtil::IsArrayIndex(const std::string& token)
{
    return token.length() >= 3 && token[0] == '[' && token[token.length() - 1] == ']';
}

std::string CJsonPathUtil::ProcessSpecialKey(const std::string& key)
{
    // 슬래시로 시작하는 키 처리 (예: "/timer[1]/counter")
    std::string processedKey = key;

    // 배열 인덱스가 키에 포함된 경우 분리하지 않음
    // 예: "/timer[1]/counter" 는 하나의 키로 처리

    return processedKey;
}
