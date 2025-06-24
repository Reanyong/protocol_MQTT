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

	// 두 가지 형식 지원:
	// 1. 표준 JSONPath: $.data.payload...
	// 2. 슬래시 경로: /data/payload...
	TCHAR firstChar = jsonPath.GetAt(0);
	if (firstChar != '$' && firstChar != '/') {
		return false;
	}

	// 기본적인 구문 검증
	CString path = jsonPath;
	
	// 연속된 점이나 슬래시 확인
	if (path.Find(_T("..")) >= 0 || path.Find(_T("//")) >= 0) {
		return false;
	}

	return true;
}

std::vector<std::string> CJsonPathUtil::ParseJsonPath(const CString& jsonPath)
{
	std::vector<std::string> tokens;

	TRACE("=== JSONPath 파싱 시작 ===\n");
	TRACE("입력 경로: '%S'\n", (LPCTSTR)jsonPath);

	if (!IsValidJsonPath(jsonPath)) {
		TRACE("JSONPath 유효성 검사 실패\n");
		return tokens;
	}

	// CString을 std::string으로 변환
	std::string path = CT2A(jsonPath);
	TRACE("변환된 경로: '%s'\n", path.c_str());

	// 경로 형식에 따른 처리
	if (path[0] == '$') {
		TRACE("표준 JSONPath 형식으로 처리\n");
		// 표준 JSONPath 처리: $.data.payload...
		path = path.substr(1);  // $ 제거

		// 빈 경로면 루트 반환
		if (path.empty() || path == ".") {
			TRACE("루트 경로 반환\n");
			return tokens;
		}

		// 점으로 시작하면 제거
		if (path[0] == '.') {
			path = path.substr(1);
		}

		// 점으로 토큰 분리
		std::stringstream ss(path);
		std::string token;
		while (std::getline(ss, token, '.')) {
			if (!token.empty()) {
				token = ProcessSpecialKey(token);
				tokens.push_back(token);
				TRACE("토큰 추가: '%s'\n", token.c_str());
			}
		}
	}
	else if (path[0] == '/') {
		TRACE("슬래시 경로 형식으로 처리\n");
		path = path.substr(1);  // 첫 번째 / 제거
		TRACE("/ 제거 후 경로: '%s'\n", path.c_str());

		if (path.empty()) {
			TRACE("빈 경로 - 루트 반환\n");
			return tokens;
		}

		// **핵심 수정: 특별한 패턴 감지 및 처리**
		// /data/payload/iolinkmaster/port[3]/iolinkdevice/pdin/data 패턴을
		// data, payload, "/iolinkmaster/port[3]/iolinkdevice/pdin", data로 분리

		if (path.find("data/payload/") == 0) {
			TRACE("data/payload/ 패턴 감지\n");

			tokens.push_back("data");
			tokens.push_back("payload");

			// data/payload/ 제거
			std::string remainingPath = path.substr(12); // "data/payload/" 길이
			TRACE("남은 경로: '%s'\n", remainingPath.c_str());

			// 마지막 /data 부분 찾기
			size_t lastSlashPos = remainingPath.find_last_of('/');
			if (lastSlashPos != std::string::npos &&
				remainingPath.substr(lastSlashPos + 1) == "data") {

				// 중간 부분을 하나의 키로 처리 - **여기서 슬래시 중복 제거**
				std::string middleKey = remainingPath.substr(0, lastSlashPos);

				// 만약 middleKey가 슬래시로 시작하지 않으면 추가
				if (!middleKey.empty() && middleKey[0] != '/') {
					middleKey = "/" + middleKey;
				}

				tokens.push_back(middleKey);
				tokens.push_back("data");

				TRACE("중간 키로 처리: '%s'\n", middleKey.c_str());
				TRACE("최종 키: 'data'\n");
			}
			else {
				// 마지막이 data가 아닌 경우, 전체를 하나의 키로 처리
				std::string wholeKey = remainingPath;

				// 만약 wholeKey가 슬래시로 시작하지 않으면 추가
				if (!wholeKey.empty() && wholeKey[0] != '/') {
					wholeKey = "/" + wholeKey;
				}

				tokens.push_back(wholeKey);
				TRACE("전체 키로 처리: '%s'\n", wholeKey.c_str());
			}
		}
	}

	TRACE("=== JSONPath 파싱 완료 - 총 %d개 토큰 ===\n", tokens.size());
	for (size_t i = 0; i < tokens.size(); ++i) {
		TRACE("토큰[%d]: '%s'\n", i, tokens[i].c_str());
	}

	return tokens;
}

const nlohmann::json* CJsonPathUtil::NavigateToValue(const nlohmann::json& jsonData,
	const std::vector<std::string>& pathTokens)
{
	const nlohmann::json* current = &jsonData;

	TRACE("=== JSON 경로 탐색 시작 ===\n");
	TRACE("총 토큰 개수: %d\n", pathTokens.size());
	
	for (size_t i = 0; i < pathTokens.size(); ++i) {
		const auto& token = pathTokens[i];
		
		if (!current) {
			TRACE("토큰 %d: '%s' - current가 nullptr\n", i, token.c_str());
			return nullptr;
		}

		TRACE("토큰 %d: '%s'\n", i, token.c_str());
		
		// 현재 JSON 타입 출력
		if (current->is_object()) {
			TRACE("  현재 타입: object (키 개수: %d)\n", current->size());
			// 사용 가능한 키들 출력
			TRACE("  사용 가능한 키: ");
			for (auto it = current->begin(); it != current->end(); ++it) {
				TRACE("'%s' ", it.key().c_str());
			}
			TRACE("\n");
		}
		else if (current->is_array()) {
			TRACE("  현재 타입: array (크기: %d)\n", current->size());
		}
		else {
			TRACE("  현재 타입: 기타 (%s)\n", current->type_name());
		}

		// 배열 인덱스 처리
		if (IsArrayIndex(token)) {
			int index = ParseArrayIndex(token);
			TRACE("  배열 인덱스 처리: %s -> 인덱스 %d\n", token.c_str(), index);
			
			if (current->is_array() && index >= 0 && index < static_cast<int>(current->size())) {
				current = &(*current)[index];
				TRACE("  배열 인덱스 접근 성공\n");
			}
			else {
				TRACE("  배열 인덱스 접근 실패 - is_array: %s, index: %d, size: %d\n", 
					  current->is_array() ? "true" : "false", 
					  index, 
					  current->is_array() ? current->size() : 0);
				return nullptr;
			}
		}
		// 객체 키 처리
		else {
			TRACE("  객체 키 처리: '%s'\n", token.c_str());
			
			if (current->is_object() && current->contains(token)) {
				current = &(*current)[token];
				TRACE("  객체 키 접근 성공\n");
			}
			else {
				TRACE("  객체 키 접근 실패 - is_object: %s, contains: %s\n", 
					  current->is_object() ? "true" : "false",
					  current->is_object() && current->contains(token) ? "true" : "false");
				return nullptr;
			}
		}
	}

	TRACE("=== JSON 경로 탐색 완료 - 성공 ===\n");
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
