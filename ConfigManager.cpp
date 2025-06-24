#include "pch.h"
#include "ConfigManager.h"

CConfigManager::CConfigManager()
	: m_parsingInterval(1000) // 기본값 1초
	, m_tagGroup(_T(""))
	, m_mqttIp(_T("127.0.0.1"))   // 기본 IP
	, m_mqttPort(1883)            // 기본 포트
	, m_mqttKeepAlive(60)         // 기본 keepalive
{
	// INI 파일 경로 설정 (실행 파일과 같은 경로에 저장)
	TCHAR szPath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, szPath, MAX_PATH);

	// 실행 파일 이름 부분 제거하고 INI 파일 이름 추가
	CString strPath(szPath);
	int nPos = strPath.ReverseFind('\\');
	if (nPos > 0) {
		m_iniFilePath = strPath.Left(nPos + 1) + _T("EVMQTT_Config.ini");
	}
	else {
		m_iniFilePath = _T("EVMQTT_Config.ini"); // 현재 디렉토리에 저장
	}
}

CConfigManager::~CConfigManager()
{
}

CConfigManager& CConfigManager::GetInstance()
{
	static CConfigManager instance;
	return instance;
}

bool CConfigManager::LoadConfig()
{
	bool result = true;

	try {
		// 파싱 간격 읽기
		m_parsingInterval = GetPrivateProfileInt(_T("General"), _T("ParsingInterval"),
			1000, m_iniFilePath);

		TCHAR szTagGroup[64] = { 0 };
		GetPrivateProfileString(_T("TagInfo"), _T("TagGroup"), _T("MQTT"),
			szTagGroup, 64, m_iniFilePath);
		m_tagGroup = szTagGroup;

		// MQTT 설정 읽기
		TCHAR szMqttIp[64] = { 0 };
		GetPrivateProfileString(_T("General"), _T("Ip"), _T("127.0.0.1"),
			szMqttIp, 64, m_iniFilePath);
		m_mqttIp = szMqttIp;

		m_mqttPort = GetPrivateProfileInt(_T("General"), _T("Port"), 1883, m_iniFilePath);
		m_mqttKeepAlive = GetPrivateProfileInt(_T("General"), _T("KeepAlive"), 60, m_iniFilePath);

		result = result && LoadTagMappings();

		return result;
	}
	catch (const std::exception& e) {
		OutputDebugStringW(L"설정 로드 오류: ");
		OutputDebugStringA(e.what());
		OutputDebugStringW(L"\n");
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

		WritePrivateProfileString(_T("TagInfo"), _T("TagGroup"),
			m_tagGroup, m_iniFilePath);

		result = result && SaveTagMappings();

		return result;
	}
	catch (const std::exception& e) {
		OutputDebugStringW(L"설정 저장 오류: ");
		OutputDebugStringA(e.what());
		OutputDebugStringW(L"\n");
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
		OutputDebugStringW(L"태그 매핑 로드 오류: ");
		OutputDebugStringA(e.what());
		OutputDebugStringW(L"\n");
		return false;
	}
}

bool CConfigManager::SaveTagMappings()
{
	try {
		// 기존 [TagMapping] 섹션 삭제
		WritePrivateProfileSection(_T("TagMapping"), NULL, m_iniFilePath);

		// 새로운 태그 매핑들 저장
		for (const auto& mapping : m_tagMappings) {
			WritePrivateProfileString(_T("TagMapping"), mapping.first, mapping.second, m_iniFilePath);
		}

		return true;
	}
	catch (const std::exception& e) {
		OutputDebugStringW(L"태그 매핑 저장 오류: ");
		OutputDebugStringA(e.what());
		OutputDebugStringW(L"\n");
		return false;
	}
}

void CConfigManager::RemoveTagMapping(const CString& tagName)
{
	auto it = m_tagMappings.find(tagName);
	if (it != m_tagMappings.end()) {
		m_tagMappings.erase(it);
		TRACE("태그 매핑 삭제: %s\n", tagName);
	}
}

bool CConfigManager::HasTagMapping(const CString& tagName) const
{
	return m_tagMappings.find(tagName) != m_tagMappings.end();
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
