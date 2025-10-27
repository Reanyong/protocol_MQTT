#include "pch.h"
#include "ParserJSON.h"
#include "ConfigManager.h"
#include "JsonPathUtil.h"
#include "LogManager.h"
#include "TagInfoCache.h"        // Phase 1: 태그 캐시 추가
#include "JsonPathTokenCache.h"  // Phase 2: JSONPath 토큰 캐시 추가
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

	// ===== Device Type 체크 (대소문자 구분 없음) =====
	CConfigManager& configManager = CConfigManager::GetInstance();
	CString deviceType = configManager.GetDeviceType();

	if (deviceType.CompareNoCase(_T("IFM")) == 0) {
		// IFM device JSON parsing
		TRACE("IFM device parsing: Topic=%s\n", (LPCTSTR)mqttTopic);
	}
	else if (deviceType.CompareNoCase(_T("Navifra")) == 0) {
		// Navifra device JSON parsing (not implemented yet)
		TRACE("Navifra device parsing (not implemented): Topic=%s\n", (LPCTSTR)mqttTopic);
		// TODO: Add Navifra parsing logic
		return false;
	}
	else {
		// Unknown device type
		TRACE("Unknown Device type: %s - Please set Device type\n", (LPCTSTR)deviceType);

		// Log error
		CLogManager& logManager = CLogManager::GetInstance();
		CString errorMsg;
		errorMsg.Format(_T("Unknown Device type: %s"), deviceType);
		logManager.WriteErrorLog(_T("DeviceTypeError"), _T("JSONParsing"),
			errorMsg, _T("Please configure Device type correctly"));

		return false;
	}

	// ===== multimap 기반 중복 토픽 지원 =====
	// 하나의 토픽에 여러 태그 매핑 가능 (예: test → test, test1)
	std::vector<TagMappingInfo> tagInfos = configManager.GetAllTagsByTopic(mqttTopic);

	if (tagInfos.empty()) {
		TRACE("토픽에 매핑된 태그 없음: '%S'\n", (LPCTSTR)mqttTopic);

		// 디버깅: Map에 어떤 토픽들이 있는지 출력 (최초 1회만)
		static bool mapDebugPrinted = false;
		if (!mapDebugPrinted) {
			mapDebugPrinted = true;
			TRACE("=== 디버깅: Topic Map 내용 확인 ===\n");
			std::map<CString, CString> allMappings = configManager.GetAllTagMappings();
			TRACE("전체 매핑 개수: %d\n", allMappings.size());

			int printCount = 0;
			for (const auto& m : allMappings) {
				if (printCount < 5) {  // 처음 5개만 출력
					TRACE("  [%d] 태그='%S', 매핑='%S'\n",
						printCount + 1, (LPCTSTR)m.first, (LPCTSTR)m.second);
					printCount++;
				}
			}
			if (allMappings.size() > 5) {
				TRACE("  ... 외 %d개 더 있음\n", allMappings.size() - 5);
			}
		}
		return false;
	}

	TRACE("\n--- multimap 기반 태그 처리 (토픽: %S, 매핑: %d개) ---\n",
		(LPCTSTR)mqttTopic, tagInfos.size());

	// 같은 토픽의 모든 태그 처리
	bool anySuccess = false;
	int successCount = 0;
	int failCount = 0;

	for (const auto& tagInfo : tagInfos) {
		TRACE("  처리 중 [%d/%d]: 태그='%S', JSONPath='%S'\n",
			successCount + failCount + 1, tagInfos.size(),
			(LPCTSTR)tagInfo.tagName, (LPCTSTR)tagInfo.jsonPath);

		// EasyView 태그 존재 여부 확인
		ST_EV_TAG_INFO evTagInfo;
		if (EV_GetTagInfo(tagInfo.tagName, &evTagInfo) <= 0) {
			TRACE("    → EasyView에서 태그를 찾을 수 없음: %S\n", (LPCTSTR)tagInfo.tagName);
			failCount++;
			continue;
		}

		// JSONPath로 값 추출 및 태그에 적용
		if (ApplyValueToTagOptimized(tagInfo.tagName, tagInfo.jsonPath)) {
			TRACE("    → 태그 적용 성공: %S\n", (LPCTSTR)tagInfo.tagName);
			successCount++;
			anySuccess = true;
		}
		else {
			TRACE("    → 태그 적용 실패: %S\n", (LPCTSTR)tagInfo.tagName);
			failCount++;
		}
	}

	TRACE("--- 처리 완료: 성공 %d개, 실패 %d개 ---\n", successCount, failCount);
	return anySuccess;
}

bool CJsonParser::ApplyValueToTagOptimized(const CString& tagName, const CString& jsonPath) const
{
	// ===== 새로운 Raw HEX 모드 =====
	// HEX 원본 데이터를 ScanBuffer에 직접 쓰기
	// 모든 IODD 장비 대응 (EasyView 태그 속성에 따라 자동 해석)
	TRACE("ApplyValueToTagOptimized: Raw HEX 모드로 처리\n");
	return ApplyRawHexToScanBuffer(tagName, jsonPath);

	// ===== 기존 코드 (주석 처리 - 필요시 복원 가능) =====
	/*
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

		// 모든 데이터에 대해 32bit에서 상위 16bit 추출 (DEVICE 설정 무관)
		uint32_t processData = 0;
		bool validData = false;

		// 숫자 타입인 경우
		if (pValue->is_number()) {
			processData = static_cast<uint32_t>(pValue->get<double>());
			validData = true;
			TRACE("숫자 데이터: %.0f (0x%08X)\n", pValue->get<double>(), processData);
		}
		// 문자열 타입인 경우 (16진수 처리)
		else if (pValue->is_string()) {
			std::string hexStr = pValue->get<std::string>();
			TRACE("문자열 데이터: %s\n", hexStr.c_str());

			// 16진수 문자열 변환 (8자리 또는 0x 접두사 처리)
			if ((hexStr.length() == 8 && hexStr.find_first_not_of("0123456789ABCDEFabcdef") == std::string::npos) ||
				(hexStr.length() == 10 && hexStr.substr(0, 2) == "0x")) {

				const char* hexStart = (hexStr.substr(0, 2) == "0x") ? hexStr.c_str() + 2 : hexStr.c_str();
				processData = static_cast<uint32_t>(strtoul(hexStart, nullptr, 16));
				validData = true;
				TRACE("16진수 문자열 변환: %s → 0x%08X (%u)\n", hexStr.c_str(), processData, processData);
			}
		}

		nlohmann::json processedValue = *pValue;  // 기본값은 원본 그대로

		if (validData) {
			// Bit 16-31이 Flow measurement value (상위 16비트)
			int16_t flowPD = static_cast<int16_t>((processData >> 16) & 0xFFFF);
			processedValue = flowPD;

			TRACE("32bit → 16bit 처리: 전체=0x%08X(%u) → 상위16bit(FlowPD)=%d\n",
				processData, processData, flowPD);
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
	*/
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

	// 모든 데이터에 대해 1을 곱하는 변환 공식 적용
	double originalValue = analogValue;
	analogValue = 1.0 * analogValue + 0.0;
	TRACE("데이터 변환 공식 적용: 원본=%.0f → 변환값=%.2f\n",
		originalValue, analogValue);

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

// ============================================================================
// Raw HEX 데이터를 ScanBuffer에 직접 쓰기 (모든 IODD 장비 대응)
// ============================================================================

// EasyView API 선언
//extern "C" int APIENTRY EV_PutSBBuffer(int nStnPos, int nWordOffset, short *pSrcSb, int nBuffCnt);
//extern "C" ST_EV_TAG_ANALOG_INPUT* APIENTRY EV_GetAiTagInfo(int nStnPos, int nTagPos, int *ErrorCode);
//extern "C" ST_EV_TAG_DIGITAL_INPUT* APIENTRY EV_GetDiTagInfo(int nStnPos, int nTagPos, int *ErrorCode);
//extern "C" ST_EV_TAG_STRING_INPUT* APIENTRY EV_GetSiTagInfo(int nStnPos, int nTagPos, int *ErrorCode);

bool CJsonParser::ApplyRawHexToScanBuffer(const CString& tagName, const CString& jsonPath) const
{
	try {
		// ===== Phase 1+2: 성능 측정 시작 =====
		// DWORD cacheStartTime = GetTickCount();  // 성능 최적화: 측정 제거

		// ===== Phase 2: JSONPath 토큰 캐시 조회 (핵심 최적화!) =====
		std::vector<std::string> pathTokens;
		bool pathCacheHit = g_jsonPathCache.GetCachedTokens(jsonPath, pathTokens);

		if (!pathCacheHit) {
			// 캐시 미스: 직접 파싱 (fallback)
			// TRACE("[JSONPath CACHE MISS] %S - parsing manually\n", (LPCTSTR)jsonPath);
			pathTokens = CJsonPathUtil::ParseJsonPath(jsonPath);
		}

		if (pathTokens.empty()) {
			// TRACE("JSONPath 파싱 실패: %S\n", (LPCTSTR)jsonPath);
			return false;
		}

		const nlohmann::json* pValue = CJsonPathUtil::NavigateToValue(m_jsonData, pathTokens);
		if (!pValue) {
			// TRACE("JSONPath로 값을 찾을 수 없음: %S\n", (LPCTSTR)jsonPath);  // 성능 최적화
			return false;
		}

		// ===== JSON 객체인 경우: code 체크 후 data 추출 =====
		std::string hexStr;
		if (pValue->is_object()) {
			// { "code": 200, "data": "000100" } 형식 처리
			if (pValue->contains("code")) {
				int code = (*pValue)["code"].get<int>();
				if (code != 200) {
					// 에러 코드 - 스킵 (503 등)
					// TRACE("JSONPath 응답 에러: code=%d (태그: %S)\n", code, (LPCTSTR)tagName);  // 성능 최적화
					return false;
				}
			}

			// data 필드 추출
			if (pValue->contains("data")) {
				const auto& dataValue = (*pValue)["data"];
				if (dataValue.is_string()) {
					hexStr = dataValue.get<std::string>();
				} else if (dataValue.is_number()) {
					// 숫자를 16진수 문자열로 변환
					char hexBuf[32];
					sprintf_s(hexBuf, "%X", dataValue.get<int>());
					hexStr = hexBuf;
					TRACE("숫자 데이터를 HEX로 변환: %d → %s\n", dataValue.get<int>(), hexStr.c_str());
				} else {
					TRACE("data 필드가 문자열/숫자가 아님\n");
					return false;
				}
			} else {
				TRACE("JSON 객체에 data 필드가 없음\n");
				return false;
			}
		}
		// ===== 문자열인 경우: 바로 사용 =====
		else if (pValue->is_string()) {
			hexStr = pValue->get<std::string>();
		}
		// ===== 그 외: 처리 불가 =====
		else {
			TRACE("JSONPath 값이 객체도 문자열도 아님: %S\n", (LPCTSTR)jsonPath);
			return false;
		}

		// HEX 검증 (16진수 문자만 포함)
		if (hexStr.find_first_not_of("0123456789ABCDEFabcdef") != std::string::npos) {
			TRACE("유효하지 않은 HEX 문자열: %s\n", hexStr.c_str());
			return false;
		}

		// HEX → Word 배열 변환
		std::vector<short> wordArray;
		if (!ConvertHexToWordArray(hexStr, wordArray)) {
			TRACE("HEX → Word 변환 실패\n");
			return false;
		}

		// ===== Phase 1: 캐시에서 태그 정보 조회 (핵심 최적화!) =====
		// TRACE("[DEBUG] 캐시 조회 시도: 태그명 = '%S'\n", (LPCTSTR)tagName);  // 성능 최적화

		CTagInfoCache::TagCacheEntry cacheEntry;
		bool cacheHit = g_tagCache.GetCachedTagInfo(tagName, cacheEntry);

		// TRACE("[DEBUG] 캐시 조회 결과: %s\n", cacheHit ? "HIT" : "MISS");  // 성능 최적화

		// DWORD cacheElapsed = GetTickCount() - cacheStartTime;  // 성능 최적화: 측정 제거

		int nStnPos, nTagPos, nSBOffset, nTagType;

		// ===== Phase 3: AI/AO 태그는 EV_PutSBAiValue 사용, 나머지는 EV_PutSBBuffer =====
		if (cacheHit) {
			nStnPos = cacheEntry.nStnPos;
			nTagPos = cacheEntry.nTagPos;
			nSBOffset = cacheEntry.nSBOffset;
			nTagType = cacheEntry.nTagType;

			// AI/AO 태그: EV_PutSBAiValue 호출
			if (nTagType == TYPE_AI || nTagType == TYPE_AO) {
				if (wordArray.size() > 0) {
					double engValue = static_cast<double>(wordArray[0]);
					int result = EV_PutSBAiValue(nStnPos, nTagPos, engValue);

					if (result > 0) {
						return true;
					}
					else {
						TRACE("EV_PutSBAiValue 실패: result=%d\n", result);
						return false;
					}
				}
			}
			// 그 외 태그: EV_PutSBBuffer 사용
			else {
				int result = EV_PutSBBuffer(nStnPos, nSBOffset, wordArray.data(),
					static_cast<int>(wordArray.size()));

				if (result > 0) {
					return true;
				}
				else {
					TRACE("EV_PutSBBuffer 실패: result=%d\n", result);
					return false;
				}
			}
		}
		// ===== 캐시 미스: 기존 API 호출 =====
		else {
			ST_EV_TAG_INFO tagInfo;
			int tagResult = EV_GetTagInfo(tagName, &tagInfo);
			if (tagResult <= 0) {
				TRACE("태그를 찾을 수 없음: %S (결과: %d)\n", (LPCTSTR)tagName, tagResult);
				return false;
			}

			nStnPos = tagInfo.nStnPos;
			nTagPos = tagInfo.nTagPos;
			nTagType = tagInfo.nTagType;
			nSBOffset = tagInfo.nTagPos;

			// AI/AO 태그
			if (nTagType == TYPE_AI || nTagType == TYPE_AO) {
				if (wordArray.size() > 0) {
					double engValue = static_cast<double>(wordArray[0]);
					int result = EV_PutSBAiValue(nStnPos, nTagPos, engValue);
					return (result > 0);
				}
			}
			// 그 외 태그
			else {
				int errorCode = 0;
				if (nTagType == TYPE_DI || nTagType == TYPE_DO) {
					ST_EV_TAG_DIGITAL_INPUT* pDiTag = EV_GetDiTagInfo(nStnPos, nTagPos, &errorCode);
					if (pDiTag && errorCode == 0 && pDiTag->nSBOffset >= 0) {
						nSBOffset = pDiTag->nSBOffset;
					}
				}
				else if (nTagType == TYPE_SI) {
					ST_EV_TAG_STRING_INPUT* pSiTag = EV_GetSiTagInfo(nStnPos, nTagPos, &errorCode);
					if (pSiTag && errorCode == 0 && pSiTag->nSBOffset >= 0) {
						nSBOffset = pSiTag->nSBOffset;
					}
				}

				int result = EV_PutSBBuffer(nStnPos, nSBOffset, wordArray.data(),
					static_cast<int>(wordArray.size()));
				return (result > 0);
			}
		}

		return false;
	}
	catch (const std::exception& e) {
		TRACE("Raw HEX 처리 중 예외 발생: %s\n", e.what());
		return false;
	}
}

bool CJsonParser::ConvertHexToWordArray(const std::string& hexStr, std::vector<short>& wordArray) const
{
	wordArray.clear();

	// HEX 문자열 길이가 짝수여야 함
	if (hexStr.length() % 2 != 0) {
		TRACE("HEX 문자열 길이가 홀수: %d\n", hexStr.length());
		return false;
	}

	size_t byteCount = hexStr.length() / 2;

	// ===== 성능 최적화: 문자열 할당 제거, 직접 파싱 =====
	// Byte 단위로 변환 후 Word로 조합 - IO-Link 데이터는 보통 Big Endian

	// Inline HEX 문자 → 숫자 변환 함수 (매우 빠름!)
	auto hexCharToInt = [](char c) -> int {
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		return 0;
	};

	for (size_t i = 0; i < byteCount; i += 2) {
		if (i + 1 < byteCount) {
			// 2 bytes → 1 word (Big Endian)
			// 직접 HEX 문자 파싱 (substr, strtoul 제거!)
			size_t idx1 = i * 2;
			size_t idx2 = i * 2 + 2;

			unsigned char byte1 = (hexCharToInt(hexStr[idx1]) << 4) | hexCharToInt(hexStr[idx1 + 1]);
			unsigned char byte2 = (hexCharToInt(hexStr[idx2]) << 4) | hexCharToInt(hexStr[idx2 + 1]);

			// Big Endian: [High Byte][Low Byte]
			short wordValue = (byte1 << 8) | byte2;
			wordArray.push_back(wordValue);

			// TRACE 제거 (Release에서는 무시되지만 일관성을 위해)
			// TRACE("  Byte[%d-%d]: 0x%02X%02X → Word: 0x%04X (%d)\n",
			//       i, i+1, byte1, byte2, (unsigned short)wordValue, wordValue);
		}
		else {
			// 마지막 바이트가 홀수개인 경우 (패딩)
			size_t idx = i * 2;
			unsigned char byte1 = (hexCharToInt(hexStr[idx]) << 4) | hexCharToInt(hexStr[idx + 1]);
			short wordValue = byte1 << 8;  // 상위 바이트에 배치
			wordArray.push_back(wordValue);

			// TRACE("  Byte[%d]: 0x%02X → Word: 0x%04X (패딩)\n",
			//       i, byte1, (unsigned short)wordValue);
		}
	}

	TRACE("총 %d개의 Word로 변환 완료\n", wordArray.size());
	return true;
}
