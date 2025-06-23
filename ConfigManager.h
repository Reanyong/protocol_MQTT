#pragma once
#include <map>

class CConfigManager
{
public:
	CConfigManager();
	virtual ~CConfigManager();

	// 설정 로드/저장 (INI 파일 사용)
	bool LoadConfig();
	bool SaveConfig();

	// 파싱 주기 설정 (ms)
	void SetParsingInterval(int interval);
	int GetParsingInterval() const;

	// 싱글톤 패턴
	static CConfigManager& GetInstance();

	// 태그 매핑 관련 메서드
	CString GetTagGroup() const;
	void SetTagGroup(const CString& tagGroup);

	// 모든 태그 매핑 조회
	std::map<CString, CString> GetAllTagMappings() const;

	// 태그 매핑 로드/저장
	bool LoadTagMappings();
	bool SaveTagMappings();

	// 기본 태그 매핑 생성
	void CreateDefaultTagMappings();

	// 태그 매핑 삭제
	void RemoveTagMapping(const CString& tagName);

	// 태그 매핑 존재 여부 확인
	bool HasTagMapping(const CString& tagName) const;

	// MQTT 설정
	void SetMqttIp(const CString& ip);
	CString GetMqttIp() const;

	void SetMqttPort(int port);
	int GetMqttPort() const;

	void SetMqttKeepAlive(int keepAlive);
	int GetMqttKeepAlive() const;

private:
	int m_parsingInterval;
	CString m_iniFilePath;
	CString m_tagGroup;
	CString m_mqttIp;
	int m_mqttPort;
	int m_mqttKeepAlive;
	std::map<CString, CString> m_tagMappings;  // 태그명 -> JSONPath 매핑
};
