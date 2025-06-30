#include "pch.h"
#include "ParserJSON.h"
#include "ConfigManager.h"
#include "JsonPathUtil.h"
#include "LogManager.h"
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
		m_jsonData = nlohmann::json::parse(payload, payload + length);
		m_parseStatus = PARSE_SUCCESS;
		m_errorMessage = _T("");
		m_isValid = true;
		return true;
	}
	catch (const nlohmann::json::parse_error& e) {
		m_parseStatus = PARSE_JSON_ERROR;
		m_errorMessage.Format(_T("JSON 파싱 오류: %hs"), e.what());
		m_isValid = false;
		TRACE("JSON 파싱 오류: %s\n", e.what());

		// 실패 로그 기록 - 이 부분 추가
		CLogManager& logManager = CLogManager::GetInstance();
		CString payloadPreview;
		if (length > 0 && payload) {
			// 처음 100자만 미리보기로 저장
			int previewLen = min(100, length);
			std::string preview(payload, previewLen);
			payloadPreview = CString(preview.c_str());
			if (length > 100) {
				payloadPreview += _T("...");
			}
		}
		logManager.WriteErrorLog(_T("JSON파싱오류"), _T("메시지처리"), m_errorMessage, payloadPreview);

		return false;
	}
	catch (const std::exception& e) {
		m_parseStatus = PARSE_JSON_ERROR;
		m_errorMessage.Format(_T("예외 발생: %hs"), e.what());
		m_isValid = false;
		TRACE("예외 발생: %s\n", e.what());

		// 실패 로그 기록 - 이 부분 추가
		CLogManager& logManager = CLogManager::GetInstance();
		logManager.WriteErrorLog(_T("JSON예외"), _T("메시지처리"), m_errorMessage);

		return false;
	}
}

bool CJsonParser::ApplyMqttTagMapping(const CString& mqttTopic) const
{
	if (!m_isValid) {
		TRACE("JSON 데이터가 유효하지 않음\n");
		return false;
	}

	CConfigManager& configManager = CConfigManager::GetInstance();
	std::map<CString, CString> tagMappings = configManager.GetAllTagMappings();
	CString deviceType = configManager.GetDevice();

	TRACE("=== MQTT 토픽별 태그 매핑 적용 시작 ===\n");
	TRACE("받은 MQTT 토픽: %S\n", (LPCTSTR)mqttTopic);
	TRACE("Device Type: %s\n", (LPCTSTR)deviceType);

	if (tagMappings.empty()) {
		TRACE("태그 매핑이 비어있습니다.\n");
		return false;
	}

	bool anySuccess = false;
	int successCount = 0;
	int totalCount = 0;
	int skippedCount = 0;

	for (const auto& mapping : tagMappings) {
		const CString& tagName = mapping.first;
		const CString& tagMapping = mapping.second;

		// 토픽과 JSONPath 분리
		CString configuredTopic, jsonPath;
		int commaPos = tagMapping.Find(_T(","));
		if (commaPos > 0) {
			configuredTopic = tagMapping.Left(commaPos);
			jsonPath = tagMapping.Mid(commaPos + 1);
			configuredTopic.Trim();
			jsonPath.Trim();
		}
		else {
			configuredTopic = _T("+");  // 모든 토픽 허용
			jsonPath = tagMapping;
			jsonPath.Trim();
		}

		totalCount++;

		// 토픽 필터링
		if (configuredTopic != _T("+") && configuredTopic.CompareNoCase(mqttTopic) != 0) {
			TRACE("토픽 불일치로 건너뜀: 태그=%S, 설정토픽=%S, 수신토픽=%S\n",
				(LPCTSTR)tagName, (LPCTSTR)configuredTopic, (LPCTSTR)mqttTopic);
			skippedCount++;
			continue;
		}

		TRACE("\n--- 태그 처리 ---\n");
		TRACE("태그명: %S\n", (LPCTSTR)tagName);
		TRACE("설정 토픽: %S\n", (LPCTSTR)configuredTopic);
		TRACE("JSONPath: %S\n", (LPCTSTR)jsonPath);

		// EasyView 태그 존재 여부 확인
		ST_EV_TAG_INFO tagInfo;
		if (EV_GetTagInfo(tagName, &tagInfo) <= 0) {
			TRACE("EasyView에서 태그를 찾을 수 없음: %S\n", (LPCTSTR)tagName);
			continue;
		}

		// JSONPath로 값 추출 및 태그에 적용
		if (ApplyValueToTagOptimized(tagName, jsonPath)) {
			successCount++;
			anySuccess = true;
			TRACE("태그 적용 성공: %S\n", (LPCTSTR)tagName);
		}
		else {
			TRACE("태그 적용 실패: %S\n", (LPCTSTR)tagName);
		}
	}

	TRACE("\n=== MQTT 태그 매핑 결과 ===\n");
	TRACE("전체 매핑: %d개\n", totalCount);
	TRACE("토픽 불일치로 건너뜀: %d개\n", skippedCount);
	TRACE("처리 시도: %d개\n", totalCount - skippedCount);
	TRACE("성공: %d개\n", successCount);
	TRACE("최종 결과: %s\n", anySuccess ? "성공" : "실패");

	return anySuccess;
}

bool CJsonParser::ApplyValueToTagOptimized(const CString& tagName, const CString& jsonPath) const
{
	try {
		// JSONPath로 원시 JSON 값 추출
		std::vector<std::string> pathTokens = CJsonPathUtil::ParseJsonPath(jsonPath);
		if (pathTokens.empty()) {
			TRACE("JSONPath 파싱 실패: %S\n", (LPCTSTR)jsonPath);
			return false;
		}

		TRACE("JSONPath 토큰 개수: %d\n", pathTokens.size());
		for (size_t i = 0; i < pathTokens.size(); i++) {
			TRACE("  토큰[%d]: %s\n", i, pathTokens[i].c_str());
		}

		const nlohmann::json* pValue = CJsonPathUtil::NavigateToValue(m_jsonData, pathTokens);
		if (!pValue) {
			TRACE("JSONPath로 값을 찾을 수 없음: %S\n", (LPCTSTR)jsonPath);

			// JSON 구조 출력 (디버깅용)
			try {
				std::string jsonStr = m_jsonData.dump(2);
				TRACE("현재 JSON 구조:\n%s\n", jsonStr.c_str());
			}
			catch (...) {
				TRACE("JSON 구조 출력 실패\n");
			}

			return false;
		}

		TRACE("JSONPath로 값 추출 성공\n");

		// SMC_PF3A703H Device일 때 32bit에서 상위 16bit 추출
		CConfigManager& configManager = CConfigManager::GetInstance();
		CString deviceType = configManager.GetDevice();

		nlohmann::json processedValue = *pValue;  // 기본값은 원본 그대로

		if (deviceType.CompareNoCase(_T("SMC_PF3A703H")) == 0) {
			uint32_t processData = 0;
			bool validData = false;

			// 숫자 타입인 경우
			if (pValue->is_number()) {
				processData = static_cast<uint32_t>(pValue->get<double>());
				validData = true;
				TRACE("SMC 숫자 데이터: %.0f (0x%08X)\n", pValue->get<double>(), processData);
			}
			// 문자열 타입인 경우 (16진수 처리)
			else if (pValue->is_string()) {
				std::string hexStr = pValue->get<std::string>();
				TRACE("SMC 문자열 데이터: %s\n", hexStr.c_str());

				// 16진수 문자열 변환 (8자리 또는 0x 접두사 처리)
				if ((hexStr.length() == 8 && hexStr.find_first_not_of("0123456789ABCDEFabcdef") == std::string::npos) ||
					(hexStr.length() == 10 && hexStr.substr(0, 2) == "0x")) {

					const char* hexStart = (hexStr.substr(0, 2) == "0x") ? hexStr.c_str() + 2 : hexStr.c_str();
					processData = static_cast<uint32_t>(strtoul(hexStart, nullptr, 16));
					validData = true;
					TRACE("16진수 문자열 변환: %s → 0x%08X (%u)\n", hexStr.c_str(), processData, processData);
				}
			}

			if (validData) {
				// SMC 스펙: Bit 16-31이 Flow measurement value (상위 16비트)
				int16_t flowPD = static_cast<int16_t>((processData >> 16) & 0xFFFF);
				processedValue = flowPD;

				TRACE("SMC 32bit → 16bit 처리: 전체=0x%08X(%u) → 상위16bit(FlowPD)=%d\n",
					processData, processData, flowPD);
			}
		}

		// 추출된 값의 타입과 내용 출력
		if (processedValue.is_string()) {
			TRACE("처리된 값 (문자열): %s\n", processedValue.get<std::string>().c_str());
		}
		else if (processedValue.is_number()) {
			TRACE("처리된 값 (숫자): %f\n", processedValue.get<double>());
		}
		else if (processedValue.is_boolean()) {
			TRACE("처리된 값 (불린): %s\n", processedValue.get<bool>() ? "true" : "false");
		}
		else {
			TRACE("처리된 값 (기타): %s\n", processedValue.dump().c_str());
		}

		// EasyView 태그 정보 조회
		ST_EV_TAG_INFO tagInfo;
		int tagResult = EV_GetTagInfo(tagName, &tagInfo);
		if (tagResult <= 0) {
			TRACE("태그를 찾을 수 없음: %S (결과: %d)\n", (LPCTSTR)tagName, tagResult);
			return false;
		}

		TRACE("태그 정보 - 타입: %d, Station: %d, Position: %d\n",
			tagInfo.nTagType, tagInfo.nStnPos, tagInfo.nTagPos);

		// JSON 값 타입에 따른 최적화된 처리 (processedValue 사용)
		switch (tagInfo.nTagType) {
		case TYPE_DI:
		case TYPE_DO:
			return ApplyDigitalValue(tagInfo, processedValue);

		case TYPE_AI:
		case TYPE_AO:
			return ApplyAnalogValue(tagInfo, processedValue);

		case TYPE_SI:
			return ApplyStringValue(tagInfo, processedValue);

		default:
			TRACE("지원하지 않는 태그 타입: %d\n", tagInfo.nTagType);
			return false;
		}
	}
	catch (const std::exception& e) {
		TRACE("태그 값 설정 중 예외 발생: %s\n", e.what());
		return false;
	}
}

bool CJsonParser::ApplyDigitalValue(const ST_EV_TAG_INFO& tagInfo, const nlohmann::json& jsonValue) const
{
	int digitalValue = 0;

	if (jsonValue.is_boolean()) {
		digitalValue = jsonValue.get<bool>() ? 1 : 0;
	}
	else if (jsonValue.is_number()) {
		digitalValue = (jsonValue.get<double>() != 0.0) ? 1 : 0;
	}
	else if (jsonValue.is_string()) {
		std::string strValue = jsonValue.get<std::string>();
		if (strValue == "true" || strValue == "1" || strValue == "on") {
			digitalValue = 1;
		}
		else if (strValue == "false" || strValue == "0" || strValue == "off") {
			digitalValue = 0;
		}
		else {
			try {
				digitalValue = (std::stod(strValue) != 0.0) ? 1 : 0;
			}
			catch (...) {
				digitalValue = 0;
			}
		}
	}
	else {
		TRACE("디지털 태그에 적용할 수 없는 JSON 값 타입\n");
		return false;
	}

	int result = EV_PutSBDiValue(tagInfo.nStnPos, tagInfo.nTagPos, digitalValue);
	TRACE("DI/DO 태그 적용 결과: %d (값: %d)\n", result, digitalValue);
	return result > 0;
}

bool CJsonParser::ApplyAnalogValue(const ST_EV_TAG_INFO& tagInfo, const nlohmann::json& jsonValue) const
{
	double analogValue = 0.0;

	if (jsonValue.is_number()) {
		analogValue = jsonValue.get<double>();
	}
	else if (jsonValue.is_string()) {
		std::string strValue = jsonValue.get<std::string>();

		try {
			// 16진수 처리
			if (strValue.length() > 2 && (strValue.substr(0, 2) == "0x" || strValue.substr(0, 2) == "0X")) {
				unsigned int hexValue = 0;
				if (sscanf_s(strValue.c_str(), "%x", &hexValue) == 1) {
					analogValue = static_cast<double>(hexValue);
				}
				else {
					TRACE("16진수 변환 실패: %s\n", strValue.c_str());
					return false;
				}
			}
			// 일반 16진수 (A-F 포함)
			else if (strValue.find_first_of("ABCDEFabcdef") != std::string::npos &&
				strValue.find_first_not_of("0123456789ABCDEFabcdef") == std::string::npos) {
				unsigned int hexValue = 0;
				if (sscanf_s(strValue.c_str(), "%x", &hexValue) == 1) {
					analogValue = static_cast<double>(hexValue);
				}
				else {
					TRACE("16진수 변환 실패: %s\n", strValue.c_str());
					return false;
				}
			}
			// 10진수 처리
			else {
				analogValue = std::stod(strValue);
			}
		}
		catch (const std::exception& e) {
			TRACE("아날로그 값 변환 실패: %s (%s)\n", strValue.c_str(), e.what());
			return false;
		}
	}
	else if (jsonValue.is_boolean()) {
		analogValue = jsonValue.get<bool>() ? 1.0 : 0.0;
	}
	else {
		TRACE("아날로그 태그에 적용할 수 없는 JSON 값 타입\n");
		return false;
	}

	// SMC_PF3A703H Device일 때 IODD 변환 공식 적용 (이미 16bit 값이 들어옴)
	CConfigManager& configManager = CConfigManager::GetInstance();
	CString deviceType = configManager.GetDevice();

	if (deviceType.CompareNoCase(_T("SMC_PF3A703H")) == 0) {
		// IODD 스펙: FlowValue(L/min) = 0.75 × PD + 0
		double originalPD = analogValue;
		analogValue = 0.75 * analogValue + 0.0;
		TRACE("SMC PF3A703H 변환 공식 적용: PD=%.0f → FlowValue=%.2f L/min\n",
			originalPD, analogValue);
	}

	int result = EV_PutSBAiValue(tagInfo.nStnPos, tagInfo.nTagPos, analogValue);
	TRACE("AI/AO 태그 적용 결과: %d (값: %f)\n", result, analogValue);
	return result > 0;
}

bool CJsonParser::ApplyStringValue(const ST_EV_TAG_INFO& tagInfo, const nlohmann::json& jsonValue) const
{
	std::string stringValue;

	if (jsonValue.is_string()) {
		stringValue = jsonValue.get<std::string>();
	}
	else if (jsonValue.is_number()) {
		if (jsonValue.is_number_integer()) {
			stringValue = std::to_string(jsonValue.get<int>());
		}
		else {
			stringValue = std::to_string(jsonValue.get<double>());
		}
	}
	else if (jsonValue.is_boolean()) {
		stringValue = jsonValue.get<bool>() ? "true" : "false";
	}
	else if (jsonValue.is_null()) {
		stringValue = "";
	}
	else {
		stringValue = jsonValue.dump();
	}

	int result = EV_PutSBString(tagInfo.nStnPos, tagInfo.nTagPos * 2,
		stringValue.c_str(), stringValue.length());
	TRACE("SI 태그 적용 결과: %d (값: %s)\n", result, stringValue.c_str());
	return result > 0;
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
