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

	CString strPath(szPath);
	int nPos = strPath.ReverseFind('\\');
	if (nPos > 0) {
		CString exeName = strPath.Mid(nPos + 1);
		int dotPos = exeName.ReverseFind('.');
		if (dotPos > 0) {
			exeName = exeName.Left(dotPos);
		}
		return strPath.Left(nPos + 1) + exeName + _T("_Config.ini");
	}
	else {
		return _T("EVMQTT_Config.ini");
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
	//try {
	//	m_tagMappings.clear();

	//	// INI 파일에서 [TagMapping] 섹션 읽기
	//	TCHAR szBuffer[8192] = { 0 };
	//	DWORD dwRead = GetPrivateProfileSection(_T("TagMapping"), szBuffer, 8192, m_iniFilePath);

	//	if (dwRead > 0) {
	//		TCHAR* p = szBuffer;
	//		while (*p) {
	//			CString line(p);
	//			int equalPos = line.Find(_T("="));

	//			if (equalPos > 0) {
	//				CString tagName = line.Left(equalPos);
	//				CString value = line.Mid(equalPos + 1);

	//				tagName.Trim();
	//				value.Trim();

	//				// 주석 라인은 건너뛰기
	//				if (tagName.IsEmpty() || tagName[0] == _T(';')) {
	//					p += lstrlen(p) + 1;
	//					continue;
	//				}

	//				if (!tagName.IsEmpty() && !value.IsEmpty()) {
	//					m_tagMappings[tagName] = value;
	//					TRACE("태그 매핑 로드: %s -> %s\n", tagName, value);
	//				}
	//			}

	//			p += lstrlen(p) + 1;  // 다음 줄로 이동
	//		}
	//	}

	//	TRACE("총 %d개의 태그 매핑을 로드했습니다.\n", m_tagMappings.size());
	//	return true;
	//}
	//catch (const std::exception& e) {
	//	OutputDebugStringA("태그 매핑 로드 오류: ");
	//	OutputDebugStringA(e.what());
	//	OutputDebugStringA("\n");
	//	return false;
	//}

	try {
		m_tagMappings.clear();

		TRACE("=== 스마트 태그 매핑 로드 시작 ===\n");

		// 스마트한 INI 섹션 읽기
		std::vector<CString> tagLines;
		if (!ReadIniSectionSmart(_T("TagMapping"), tagLines)) {
			TRACE("태그 매핑 섹션 읽기 실패\n");
			return false;
		}

		TRACE("읽어온 라인 수: %d\n", tagLines.size());

		// 각 라인 파싱
		int successCount = 0;
		int skipCount = 0;

		for (const auto& line : tagLines) {
			CString tagName, mapping;

			if (ParseTagMappingLine(line, tagName, mapping)) {
				// 중복 태그 체크
				if (m_tagMappings.find(tagName) != m_tagMappings.end()) {
					TRACE("경고: 중복 태그 발견, 덮어씀 - %s\n", (LPCTSTR)tagName);
				}

				m_tagMappings[tagName] = mapping;
				successCount++;

				// 처음 10개와 마지막 10개만 상세 로그
				if (successCount <= 10 || successCount > (int)tagLines.size() - 10) {
					TRACE("태그[%03d]: %s -> %s\n", successCount,
						(LPCTSTR)tagName, (LPCTSTR)mapping);
				}
			}
			else {
				skipCount++;
			}
		}

		TRACE("=== 태그 매핑 로드 완료 ===\n");
		TRACE("성공: %d개, 건너뜀: %d개, 전체: %d개\n",
			successCount, skipCount, tagLines.size());

		// 메모리 사용량 추정
		size_t estimatedMemory = 0;
		for (const auto& pair : m_tagMappings) {
			estimatedMemory += (pair.first.GetLength() + pair.second.GetLength()) * sizeof(TCHAR);
		}
		TRACE("추정 메모리 사용량: %.2f KB\n", estimatedMemory / 1024.0);

		return successCount > 0;
	}
	catch (const std::exception& e) {
		TRACE("태그 매핑 로드 중 예외: %s\n", e.what());
		return false;
	}
	catch (...) {
		TRACE("태그 매핑 로드 중 알 수 없는 예외\n");
		return false;
	}

}

bool CConfigManager::ReadIniSectionSmart(const CString& sectionName, std::vector<CString>& lines)
{
	lines.clear();

	DWORD initialSize = GetOptimalBufferSize(sectionName);
	TRACE("초기 버퍼 크기: %d KB\n", initialSize / 1024);

	// 2단계: 동적 버퍼로 읽기 시도
	std::vector<TCHAR> buffer;
	DWORD actualRead = 0;
	bool success = false;
	int attempts = 0;
	const int MAX_ATTEMPTS = 5;

	DWORD currentSize = initialSize;

	while (!success && attempts < MAX_ATTEMPTS) {
		attempts++;
		buffer.resize(currentSize);

		TRACE("시도 %d: 버퍼 크기 %d KB로 읽기...\n",
			attempts, currentSize / 1024);

		actualRead = GetPrivateProfileSection(
			sectionName,
			buffer.data(),
			currentSize,
			m_iniFilePath
		);

		if (actualRead < currentSize - 2) {
			success = true;
			TRACE("성공! 실제 읽은 크기: %d bytes\n", actualRead);
		}
		else {
			// 버퍼가 부족한 경우 크기를 2배로 늘림
			currentSize *= 2;
			TRACE("버퍼 부족, 크기를 %d KB로 증가\n", currentSize / 1024);

			// 너무 큰 크기가 되면 중단 (1MB 제한)
			if (currentSize > 1024 * 1024) {
				TRACE("오류: 버퍼 크기가 1MB를 초과함. INI 파일이 너무 큼.\n");
				break;
			}
		}
	}

	if (!success) {
		TRACE("섹션 읽기 실패: %d번 시도 후 포기\n", attempts);
		return false;
	}

	if (actualRead > 0) {
		TCHAR* p = buffer.data();
		int lineCount = 0;

		while (*p && p < buffer.data() + actualRead) {
			CString line(p);
			if (!line.IsEmpty()) {
				lines.push_back(line);
				lineCount++;
			}
			p += lstrlen(p) + 1;  // 다음 라인으로
		}

		TRACE("파싱된 라인 수: %d\n", lineCount);
	}

	return true;
}

// 태그 매핑 라인 파싱
bool CConfigManager::ParseTagMappingLine(const CString& line, CString& tagName, CString& mapping)
{
	// 빈 라인이나 주석 라인 건너뛰기
	CString trimmedLine = line;
	trimmedLine.Trim();

	if (trimmedLine.IsEmpty() || trimmedLine[0] == _T(';') || trimmedLine[0] == _T('#')) {
		return false;
	}

	// "태그명=매핑값" 형식 파싱
	int equalPos = trimmedLine.Find(_T("="));
	if (equalPos <= 0) {
		TRACE("잘못된 형식 라인 건너뜀: %s\n", (LPCTSTR)trimmedLine);
		return false;
	}

	tagName = trimmedLine.Left(equalPos);
	mapping = trimmedLine.Mid(equalPos + 1);

	tagName.Trim();
	mapping.Trim();

	// 유효성 검사
	if (tagName.IsEmpty()) {
		TRACE("태그명이 비어있는 라인 건너뜀: %s\n", (LPCTSTR)trimmedLine);
		return false;
	}

	if (mapping.IsEmpty()) {
		TRACE("매핑값이 비어있는 라인 건너뜀: %s\n", (LPCTSTR)trimmedLine);
		return false;
	}

	if (tagName.GetLength() > 50) {
		TRACE("태그명이 너무 긴 라인 건너뜀 (%d자): %s\n",
			tagName.GetLength(), (LPCTSTR)tagName);
		return false;
	}

	if (mapping.GetLength() > 500) {
		TRACE("매핑값이 너무 긴 라인 건너뜀 (%d자): %s\n",
			mapping.GetLength(), (LPCTSTR)tagName);
		return false;
	}

	return true;
}

// 최적 버퍼 크기 계산
DWORD CConfigManager::GetOptimalBufferSize(const CString& sectionName)
{
	HANDLE hFile = CreateFile(m_iniFilePath, GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	DWORD fileSize = 0;
	if (hFile != INVALID_HANDLE_VALUE) {
		fileSize = GetFileSize(hFile, NULL);
		CloseHandle(hFile);
	}

	DWORD initialSize;

	if (fileSize == 0 || fileSize == INVALID_FILE_SIZE) {
		initialSize = 16 * 1024;  // 16KB
		TRACE("파일 크기 확인 실패, 기본 버퍼 크기 사용: 16KB\n");
	}
	else if (fileSize < 8 * 1024) {
		initialSize = 16 * 1024;  // 16KB
		TRACE("작은 파일 감지 (%d bytes), 16KB 버퍼 사용\n", fileSize);
	}
	else if (fileSize < 64 * 1024) {
		initialSize = fileSize * 2;  // 파일 크기의 2배
		TRACE("중간 파일 감지 (%d bytes), %dKB 버퍼 사용\n",
			fileSize, initialSize / 1024);
	}
	else {
		initialSize = 128 * 1024;  // 128KB
		TRACE("큰 파일 감지 (%d bytes), 128KB 버퍼 사용\n", fileSize);
	}

	const DWORD MIN_BUFFER_SIZE = 8 * 1024;    // 최소 8KB
	const DWORD MAX_BUFFER_SIZE = 512 * 1024;  // 최대 512KB

	if (initialSize < MIN_BUFFER_SIZE) {
		initialSize = MIN_BUFFER_SIZE;
	}
	else if (initialSize > MAX_BUFFER_SIZE) {
		initialSize = MAX_BUFFER_SIZE;
	}

	return initialSize;
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

	WritePrivateProfileString(_T("TagMapping"), tagName, mapping, m_iniFilePath);
}

void CConfigManager::SetTagMapping(const CString& tagName, const CString& mapping)
{
	m_tagMappings[tagName] = mapping;
	TRACE("태그 매핑 설정: %s -> %s\n", (LPCTSTR)tagName, (LPCTSTR)mapping);

	WritePrivateProfileString(_T("TagMapping"), tagName, mapping, m_iniFilePath);
}

void CConfigManager::RemoveTagMapping(const CString& tagName)
{
	auto it = m_tagMappings.find(tagName);
	if (it != m_tagMappings.end()) {
		m_tagMappings.erase(it);
		TRACE("태그 매핑 삭제: %s\n", (LPCTSTR)tagName);

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
