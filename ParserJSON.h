#pragma once

#include "json.hpp"

class CJsonParser
{
public:
	CJsonParser();
	virtual ~CJsonParser();

	// JSON 메시지 파싱
	bool ParseMessage(const char* payload, int length);

	// 파싱 상태
	enum ParseStatus {
		PARSE_SUCCESS,
		PARSE_JSON_ERROR,
		PARSE_SCHEMA_ERROR,
		PARSE_TYPE_ERROR
	};

	// 상태 조회
	ParseStatus GetParseStatus() const { return m_parseStatus; }
	CString GetErrorMessage() const { return m_errorMessage; }
	bool IsValid() const { return m_isValid; }

	// MQTT 토픽별 태그 매핑 적용
	bool ApplyMqttTagMapping(const CString& mqttTopic) const;

	// JSONPath로 값 추출
	bool GetValueByPath(const CString& jsonPath, CString& outValue) const;
	bool GetValueByPath(const CString& jsonPath, int& outValue) const;
	bool GetValueByPath(const CString& jsonPath, double& outValue) const;

private:
	bool m_isValid;
	ParseStatus m_parseStatus;
	CString m_errorMessage;
	nlohmann::json m_jsonData;

	// 내부 처리 메서드
	bool ApplyValueToTagOptimized(const CString& tagName, const CString& jsonPath) const;
	bool ApplyDigitalValue(const ST_EV_TAG_INFO& tagInfo, const nlohmann::json& jsonValue) const;
	bool ApplyAnalogValue(const ST_EV_TAG_INFO& tagInfo, const nlohmann::json& jsonValue) const;
	bool ApplyStringValue(const ST_EV_TAG_INFO& tagInfo, const nlohmann::json& jsonValue) const;

	// Raw HEX 데이터를 ScanBuffer에 직접 쓰기
	bool ApplyRawHexToScanBuffer(const CString& tagName, const CString& jsonPath) const;
	bool ConvertHexToWordArray(const std::string& hexStr, std::vector<short>& wordArray) const;
};
