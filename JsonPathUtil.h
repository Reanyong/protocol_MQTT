#pragma once
#include "json.hpp"
#include <string>
#include <vector>

// JSONPath 파싱 및 값 추출 유틸리티 클래스
class CJsonPathUtil
{
public:
    CJsonPathUtil();
    virtual ~CJsonPathUtil();

    // JSONPath를 사용해서 JSON에서 값 추출
    static bool ExtractValue(const nlohmann::json& jsonData, const CString& jsonPath, CString& outValue);
    static bool ExtractValue(const nlohmann::json& jsonData, const CString& jsonPath, int& outValue);
    static bool ExtractValue(const nlohmann::json& jsonData, const CString& jsonPath, double& outValue);

    // JSONPath가 유효한지 검증
    static bool IsValidJsonPath(const CString& jsonPath);

    // JSONPath를 토큰으로 분리
    static std::vector<std::string> ParseJsonPath(const CString& jsonPath);

	// JSON 객체에서 경로를 따라 값 찾기
	static const nlohmann::json* NavigateToValue(const nlohmann::json& jsonData,
		const std::vector<std::string>& pathTokens);

private:
    // 배열 인덱스 파싱 (예: "[1]" -> 1)
    static int ParseArrayIndex(const std::string& token);

    // 토큰이 배열 인덱스인지 확인
    static bool IsArrayIndex(const std::string& token);

    // 특수 문자 처리 (점이 포함된 키 등)
    static std::string ProcessSpecialKey(const std::string& key);
};
