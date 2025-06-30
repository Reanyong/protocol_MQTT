#include "pch.h"
#include "ConfigManager.h"

CConfigManager::CConfigManager()
	: m_parsingInterval(1000) // 기본값 1초
	, m_tagGroup(_T(""))
	, m_mqttIp(_T("127.0.0.1"))   // 기본 IP
	, m_mqttPort(1883)            // 기본 포트
	, m_mqttKeepAlive(60)         // 기본 keepalive
	, m_device(_T(""))
{
	// INI 파일 경로 설정 (실행 파일과 같은 경로에 저장)
	m_iniFilePath = GetIniFilePath();
}

CConfigManager::~CConfigManager()
{
}

CConfigManager& CConfigManager::GetInstance()
{
	static CConfigManager instance;
	return instance;
}

CString CConfigManager::GetIniFilePath() const
{
	TCHAR szPath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, szPath, MAX_PATH);

	// 실행 파일 이름 부분 제거하고 INI 파일 이름 추가
	CString strPath(szPath);
	int nPos = strPath.ReverseFind('\\');
	if (nPos > 0) {
		return strPath.Left(nPos + 1) + _T("EVMQTT_Config.ini");
	}
	else {
		return _T("EVMQTT_Config.ini"); // 현재 디렉토리에 저장
	}
}

bool CConfigManager::LoadConfig()
{
	bool result = true;

	try {
		// 파싱 간격
		m_parsingInterval = GetPrivateProfileInt(_T("General"), _T("ParsingInterval"),
			1000, m_iniFilePath);

		TCHAR szTagGroup[64] = { 0 };
		GetPrivateProfileString(_T("TagInfo"), _T("TagGroup"), _T("MQTT"),
			szTagGroup, 64, m_iniFilePath);
		m_tagGroup = szTagGroup;

		// MQTT 설정
		TCHAR szMqttIp[64] = { 0 };
		GetPrivateProfileString(_T("General"), _T("Ip"), _T("127.0.0.1"),
			szMqttIp, 64, m_iniFilePath);
		m_mqttIp = szMqttIp;

		// Device 설정
		TCHAR szDevice[64] = { 0 };
		GetPrivateProfileString(_T("General"), _T("Device"), _T(""),
			szDevice, 64, m_iniFilePath);
		m_device = szDevice;

		m_mqttPort = GetPrivateProfileInt(_T("General"), _T("Port"), 1883, m_iniFilePath);
		m_mqttKeepAlive = GetPrivateProfileInt(_T("General"), _T("KeepAlive"), 60, m_iniFilePath);

		result = result && LoadTagMappings();

		return result;
	}
	catch (const std::exception& e) {
		OutputDebugStringA("설정 로드 오류: ");
		OutputDebugStringA(e.what());
		OutputDebugStringA("\n");
		return false;
	}
}

bool CConfigManager::SaveConfig()
{
	bool result = true;

	try {
		// 파싱 간격 저장
		CString strInterval;
		strInterval.Format(_T("%d"), m_parsingInterval);
		WritePrivateProfileString(_T("General"), _T("ParsingInterval"),
			strInterval, m_iniFilePath);

		// MQTT 설정 저장
		WritePrivateProfileString(_T("General"), _T("Ip"), m_mqttIp, m_iniFilePath);

		CString strPort;
		strPort.Format(_T("%d"), m_mqttPort);
		WritePrivateProfileString(_T("General"), _T("Port"), strPort, m_iniFilePath);

		CString strKeepAlive;
		strKeepAlive.Format(_T("%d"), m_mqttKeepAlive);
		WritePrivateProfileString(_T("General"), _T("KeepAlive"), strKeepAlive, m_iniFilePath);

		WritePrivateProfileString(_T("TagInfo"), _T("TagGroup"), m_tagGroup, m_iniFilePath);

		WritePrivateProfileString(_T("General"), _T("Device"), m_device, m_iniFilePath);

		result = result && SaveTagMappings();

		return result;
	}
	catch (const std::exception& e) {
		OutputDebugStringA("설정 저장 오류: ");
		OutputDebugStringA(e.what());
		OutputDebugStringA("\n");
		return false;
	}
}

void CConfigManager::SetParsingInterval(int interval)
{
	m_parsingInterval = interval;
}

int CConfigManager::GetParsingInterval() const
{
	return m_parsingInterval;
}

CString CConfigManager::GetTagGroup() const
{
	return m_tagGroup;
}

void CConfigManager::SetTagGroup(const CString& tagGroup)
{
	m_tagGroup = tagGroup;
}

std::map<CString, CString> CConfigManager::GetAllTagMappings() const
{
	return m_tagMappings;
}

bool CConfigManager::LoadTagMappings()
{
	try {
		m_tagMappings.clear();

		// INI 파일에서 [TagMapping] 섹션 읽기
		TCHAR szBuffer[8192] = { 0 };
		DWORD dwRead = GetPrivateProfileSection(_T("TagMapping"), szBuffer, 8192, m_iniFilePath);

		if (dwRead > 0) {
			TCHAR* p = szBuffer;
			while (*p) {
				CString line(p);
				int equalPos = line.Find(_T("="));

				if (equalPos > 0) {
					CString tagName = line.Left(equalPos);
					CString value = line.Mid(equalPos + 1);

					tagName.Trim();
					value.Trim();

					// 주석 라인은 건너뛰기
					if (tagName.IsEmpty() || tagName[0] == _T(';')) {
						p += lstrlen(p) + 1;
						continue;
					}

					if (!tagName.IsEmpty() && !value.IsEmpty()) {
						m_tagMappings[tagName] = value;
						TRACE("태그 매핑 로드: %s -> %s\n", tagName, value);
					}
				}

				p += lstrlen(p) + 1;  // 다음 줄로 이동
			}
		}

		TRACE("총 %d개의 태그 매핑을 로드했습니다.\n", m_tagMappings.size());
		return true;
	}
	catch (const std::exception& e) {
		OutputDebugStringA("태그 매핑 로드 오류: ");
		OutputDebugStringA(e.what());
		OutputDebugStringA("\n");
		return false;
	}
}

bool CConfigManager::SaveTagMappings()
{
	try {
		WritePrivateProfileSection(_T("TagMapping"), NULL, m_iniFilePath);

		// 새로운 태그 매핑들 저장
		for (const auto& mapping : m_tagMappings) {
			WritePrivateProfileString(_T("TagMapping"), mapping.first, mapping.second, m_iniFilePath);
		}

		return true;
	}
	catch (const std::exception& e) {
		OutputDebugStringA("태그 매핑 저장 오류: ");
		OutputDebugStringA(e.what());
		OutputDebugStringA("\n");
		return false;
	}
}

void CConfigManager::AddTagMapping(const CString& tagName, const CString& mapping)
{
	m_tagMappings[tagName] = mapping;
	TRACE("태그 매핑 추가: %s -> %s\n", (LPCTSTR)tagName, (LPCTSTR)mapping);

	// 즉시 INI 파일에 저장
	WritePrivateProfileString(_T("TagMapping"), tagName, mapping, m_iniFilePath);
}

void CConfigManager::SetTagMapping(const CString& tagName, const CString& mapping)
{
	m_tagMappings[tagName] = mapping;
	TRACE("태그 매핑 설정: %s -> %s\n", (LPCTSTR)tagName, (LPCTSTR)mapping);

	// 즉시 INI 파일에 저장
	WritePrivateProfileString(_T("TagMapping"), tagName, mapping, m_iniFilePath);
}

void CConfigManager::RemoveTagMapping(const CString& tagName)
{
	auto it = m_tagMappings.find(tagName);
	if (it != m_tagMappings.end()) {
		m_tagMappings.erase(it);
		TRACE("태그 매핑 삭제: %s\n", (LPCTSTR)tagName);

		// 즉시 INI 파일에서 삭제
		WritePrivateProfileString(_T("TagMapping"), tagName, NULL, m_iniFilePath);
	}
}

bool CConfigManager::HasTagMapping(const CString& tagName) const
{
	return m_tagMappings.find(tagName) != m_tagMappings.end();
}

CString CConfigManager::GetTagMapping(const CString& tagName) const
{
	auto it = m_tagMappings.find(tagName);
	if (it != m_tagMappings.end()) {
		return it->second;
	}
	return _T("");
}

void CConfigManager::SetMqttIp(const CString& ip)
{
	m_mqttIp = ip;
}

CString CConfigManager::GetMqttIp() const
{
	return m_mqttIp;
}

void CConfigManager::SetMqttPort(int port)
{
	m_mqttPort = port;
}

int CConfigManager::GetMqttPort() const
{
	return m_mqttPort;
}

void CConfigManager::SetMqttKeepAlive(int keepAlive)
{
	m_mqttKeepAlive = keepAlive;
}

int CConfigManager::GetMqttKeepAlive() const
{
	return m_mqttKeepAlive;
}

// Device 관련 메서드
void CConfigManager::SetDevice(const CString& deviceType)
{
	m_device = deviceType;
	TRACE("Device 설정: %s\n", (LPCTSTR)deviceType);
}

CString CConfigManager::GetDevice() const
{
	return m_device;
}
