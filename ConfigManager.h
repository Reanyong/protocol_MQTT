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
	// CString GetTagGroup() const;
	// void SetTagGroup(const CString& tagGroup);

	// 모든 태그 매핑 조회
	std::map<CString, CString> GetAllTagMappings() const;

	// 태그 매핑 로드/저장
	bool LoadTagMappings();
	bool SaveTagMappings();

	// 태그 매핑 추가/수정
	void AddTagMapping(const CString& tagName, const CString& mapping);
	void SetTagMapping(const CString& tagName, const CString& mapping);

	//INI 섹션 읽기 메서드
	bool ReadIniSectionSmart(const CString& sectionName, std::vector<CString>& lines);

	// 태그 매핑 라인 파싱
	bool ParseTagMappingLine(const CString& line, CString& tagName, CString& mapping);

	// 버퍼 크기 자동 조정
	DWORD GetOptimalBufferSize(const CString& sectionName);

	// 태그 매핑 삭제
	void RemoveTagMapping(const CString& tagName);

	// 태그 매핑 존재 여부 확인
	bool HasTagMapping(const CString& tagName) const;

	// 특정 태그 매핑 조회
	CString GetTagMapping(const CString& tagName) const;

	// MQTT 설정
	void SetMqttIp(const CString& ip);
	CString GetMqttIp() const;

	void SetMqttPort(int port);
	int GetMqttPort() const;

	void SetMqttKeepAlive(int keepAlive);
	int GetMqttKeepAlive() const;

	// Device 설정 메서드
	void SetDevice(const CString& deviceType);
	CString GetDevice() const;

private:
	int m_parsingInterval;
	CString m_iniFilePath;
	// CString m_tagGroup;
	CString m_mqttIp;
	int m_mqttPort;
	int m_mqttKeepAlive;
	std::map<CString, CString> m_tagMappings;  // 태그명 -> JSONPath 매핑

	// Device 타입
	CString m_device;

	// 내부 헬퍼 메서드
	CString GetIniFilePath() const;
};
