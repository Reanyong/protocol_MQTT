#include "pch.h"
#include "ConfigManager.h"
#include "LogManager.h"

CConfigManager::CConfigManager()
	: m_parsingInterval(1000)
	, m_mqttIp(_T("127.0.0.1"))
	, m_mqttPort(1883)
	, m_mqttKeepAlive(60)
	, m_subscribeTopic(_T("+"))
	, m_deviceType(_T("IFM"))
	, m_autorun(0)
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

		// TCHAR szTagGroup[64] = { 0 };
		// GetPrivateProfileString(_T("TagInfo"), _T("TagGroup"), _T("MQTT"),
		//	 szTagGroup, 64, m_iniFilePath);
		// m_tagGroup = szTagGroup;

		// MQTT 설정
		TCHAR szMqttIp[64] = { 0 };
		GetPrivateProfileString(_T("General"), _T("Ip"), _T("127.0.0.1"),
			szMqttIp, 64, m_iniFilePath);
		m_mqttIp = szMqttIp;

		// Device 타입 설정 로드
		TCHAR szDeviceType[64] = { 0 };
		GetPrivateProfileString(_T("General"), _T("Device"), _T("IFM"),
			szDeviceType, 64, m_iniFilePath);
		m_deviceType = szDeviceType;

		// MQTT 구독 토픽 설정
		TCHAR szSubscribeTopic[128] = { 0 };
		GetPrivateProfileString(_T("General"), _T("SubscribeTopic"), _T("+"),
			szSubscribeTopic, 128, m_iniFilePath);
		m_subscribeTopic = szSubscribeTopic;

		m_mqttPort = GetPrivateProfileInt(_T("General"), _T("Port"), 1883, m_iniFilePath);
		m_mqttKeepAlive = GetPrivateProfileInt(_T("General"), _T("KeepAlive"), 60, m_iniFilePath);

		// Autorun 설정 로드
		m_autorun = GetPrivateProfileInt(_T("General"), _T("Autorun"), 0, m_iniFilePath);

		// 항상 두 섹션 모두 로드 (UI에서 Device Type 전환 가능하도록)
		result = result && LoadSubTagMappings();
		result = result && LoadPubTagMappings();

		// ===== HashMap 구축 (성능 최적화) =====
		if (result) {
			BuildTopicHashMap();
		}

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

		// WritePrivateProfileString(_T("TagInfo"), _T("TagGroup"), m_tagGroup, m_iniFilePath);

		// Device 타입 저장
		WritePrivateProfileString(_T("General"), _T("Device"), m_deviceType, m_iniFilePath);

		// MQTT 구독 토픽 저장
		WritePrivateProfileString(_T("General"), _T("SubscribeTopic"), m_subscribeTopic, m_iniFilePath);

		// Autorun 설정 저장
		CString strAutorun;
		strAutorun.Format(_T("%d"), m_autorun);
		WritePrivateProfileString(_T("General"), _T("Autorun"), strAutorun, m_iniFilePath);

		// Device Type에 따라 태그 매핑 저장 (대소문자 구분 없음)
		if (m_deviceType.CompareNoCase(_T("IFM")) == 0) {
			result = result && SaveSubTagMappings();
		}
		else if (m_deviceType.CompareNoCase(_T("Navifra")) == 0) {
			result = result && SavePubTagMappings();
		}
		else {
			// 기본적으로 둘 다 저장
			result = result && SaveSubTagMappings();
			result = result && SavePubTagMappings();
		}

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

// CString CConfigManager::GetTagGroup() const
// {
//	return m_tagGroup;
// }

// void CConfigManager::SetTagGroup(const CString& tagGroup)
// {
//	m_tagGroup = tagGroup;
// }

// ===== Subscribe 태그 매핑 =====
std::map<CString, CString> CConfigManager::GetAllSubTagMappings() const
{
	return m_subTagMappings;
}

// ===== Subscribe 태그 순서 =====
const std::vector<CString>& CConfigManager::GetSubTagOrder() const
{
	return m_subTagOrder;
}

// ===== Publish 태그 매핑 =====
std::map<CString, CString> CConfigManager::GetAllPubTagMappings() const
{
	return m_pubTagMappings;
}

// ===== Publish 태그 순서 =====
const std::vector<CString>& CConfigManager::GetPubTagOrder() const
{
	return m_pubTagOrder;
}

// ===== 하위 호환성 (기존 코드용) =====
std::map<CString, CString> CConfigManager::GetAllTagMappings() const
{
	// Device Type에 따라 적절한 매핑 반환
	if (m_deviceType.CompareNoCase(_T("IFM")) == 0) {
		return m_subTagMappings;
	}
	else if (m_deviceType.CompareNoCase(_T("Navifra")) == 0) {
		return m_pubTagMappings;
	}
	return m_subTagMappings;  // 기본값
}


// ===== Subscribe 태그 매핑 로드 (IFM 모드) =====
bool CConfigManager::LoadSubTagMappings()
{
	//try {
	//	m_subTagMappings.clear();

	//	// INI 파일에서 [SubTagMapping] 섹션 읽기
	//	TCHAR szBuffer[8192] = { 0 };
	//	DWORD dwRead = GetPrivateProfileSection(_T("SubTagMapping"), szBuffer, 8192, m_iniFilePath);

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
		m_subTagMappings.clear();
		m_subTagOrder.clear();  // 순서 초기화

		TRACE("=== SubTagMapping (IFM 모드) 로드 시작 ===\n");
		TRACE("INI 파일 경로: %s\n", (LPCTSTR)m_iniFilePath);
		
		// INI 파일 캐시 강제 새로고침
		WritePrivateProfileString(NULL, NULL, NULL, m_iniFilePath);
		TRACE("INI 파일 캐시 새로고침 완료\n");

		// 스마트한 INI 섹션 읽기
		std::vector<CString> tagLines;
		if (!ReadIniSectionSmart(_T("SubTagMapping"), tagLines)) {
			TRACE("SubTagMapping 섹션 읽기 실패\n");
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
				if (m_subTagMappings.find(tagName) != m_subTagMappings.end()) {
					TRACE("경고: 중복 태그 발견, 덮어씀 - %s\n", (LPCTSTR)tagName);
				}

				m_subTagMappings[tagName] = mapping;
				m_subTagOrder.push_back(tagName);  // INI 파일 순서대로 저장
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

		TRACE("=== SubTagMapping 로드 완료 ===\n");
		TRACE("성공: %d개, 건너뜀: %d개, 전체: %d개\n",
			successCount, skipCount, tagLines.size());

		// 메모리 사용량 추정
		size_t estimatedMemory = 0;
		for (const auto& pair : m_subTagMappings) {
			estimatedMemory += (pair.first.GetLength() + pair.second.GetLength()) * sizeof(TCHAR);
		}
		TRACE("추정 메모리 사용량: %.2f KB\n", estimatedMemory / 1024.0);

		return successCount > 0;
	}
	catch (const std::exception& e) {
		TRACE("SubTagMapping 로드 중 예외: %s\n", e.what());
		return false;
	}
	catch (...) {
		TRACE("SubTagMapping 로드 중 알 수 없는 예외\n");
		return false;
	}
}

// ===== Publish 태그 매핑 로드 =====
bool CConfigManager::LoadPubTagMappings()
{
	try {
		m_pubTagMappings.clear();
		m_pubTagOrder.clear();  // 순서 초기화

		TRACE("=== PubTagMapping (Navifra 모드) 로드 시작 ===\n");
		TRACE("INI 파일 경로: %s\n", (LPCTSTR)m_iniFilePath);
		
		// INI 파일 캐시 강제 새로고침
		WritePrivateProfileString(NULL, NULL, NULL, m_iniFilePath);
		TRACE("INI 파일 캐시 새로고침 완료\n");

		// 디버그 로그 파일 생성 (다른 PC에서 확인용)
		CString logPath = m_iniFilePath;
		logPath.Replace(_T("_Config.ini"), _T("_LoadDebug.txt"));
		FILE* debugLog = nullptr;
		_tfopen_s(&debugLog, logPath, _T("w"));
		if (debugLog) {
			_ftprintf(debugLog, _T("=== PubTagMapping 로드 디버그 ===\n"));
			_ftprintf(debugLog, _T("INI 경로: %s\n"), (LPCTSTR)m_iniFilePath);
		}

		// 파일 존재 확인
		DWORD fileAttr = GetFileAttributes(m_iniFilePath);
		if (fileAttr == INVALID_FILE_ATTRIBUTES) {
			TRACE("!!! INI 파일이 존재하지 않음 !!!\n");
			TRACE("에러 코드: %d\n", GetLastError());
			if (debugLog) {
				_ftprintf(debugLog, _T("ERROR: INI 파일이 존재하지 않음 (에러: %d)\n"), GetLastError());
				fclose(debugLog);
			}
			return false;
		}
		TRACE("INI 파일 존재 확인 OK (속성: 0x%X)\n", fileAttr);
		if (debugLog) {
			_ftprintf(debugLog, _T("INI 파일 존재 OK (속성: 0x%X)\n"), fileAttr);
		}

		// INI 파일 내용 전체 덤프 (디버깅용)
		if (debugLog) {
			_ftprintf(debugLog, _T("\n=== INI 파일 내용 확인 ===\n"));
			
			// 모든 섹션 이름 가져오기
			TCHAR sectionBuffer[8192] = { 0 };
			DWORD sectionRead = GetPrivateProfileSectionNames(sectionBuffer, 8192, m_iniFilePath);
			
			if (sectionRead > 0) {
				_ftprintf(debugLog, _T("발견된 섹션들:\n"));
				TCHAR* pSection = sectionBuffer;
				while (*pSection) {
					_ftprintf(debugLog, _T("  [%s]\n"), pSection);
					pSection += _tcslen(pSection) + 1;
				}
			}
			else {
				_ftprintf(debugLog, _T("섹션 이름 읽기 실패 (에러: %d)\n"), GetLastError());
			}
			
			// INI 파일 직접 읽기 (인코딩 확인용)
			_ftprintf(debugLog, _T("\n=== INI 파일 직접 읽기 (Raw) ===\n"));
			FILE* iniFile = nullptr;
			_tfopen_s(&iniFile, m_iniFilePath, _T("rb"));
			if (iniFile) {
				// 파일 크기 확인
				fseek(iniFile, 0, SEEK_END);
				long fileSize = ftell(iniFile);
				fseek(iniFile, 0, SEEK_SET);
				
				_ftprintf(debugLog, _T("파일 크기: %ld bytes\n"), fileSize);
				
				// BOM 확인
				unsigned char bom[3] = { 0 };
				fread(bom, 1, 3, iniFile);
				fseek(iniFile, 0, SEEK_SET);
				
				if (bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF) {
					_ftprintf(debugLog, _T("인코딩: UTF-8 with BOM\n"));
				}
				else if (bom[0] == 0xFF && bom[1] == 0xFE) {
					_ftprintf(debugLog, _T("인코딩: UTF-16 LE\n"));
				}
				else if (bom[0] == 0xFE && bom[1] == 0xFF) {
					_ftprintf(debugLog, _T("인코딩: UTF-16 BE\n"));
				}
				else {
					_ftprintf(debugLog, _T("인코딩: ANSI 또는 UTF-8 without BOM\n"));
				}
				
				// 처음 1000바이트 읽기
				char rawBuffer[1001] = { 0 };
				size_t readBytes = fread(rawBuffer, 1, 1000, iniFile);
				fclose(iniFile);
				
				_ftprintf(debugLog, _T("\n처음 1000 바이트 내용:\n"));
				_ftprintf(debugLog, _T("---BEGIN---\n"));
				
				// ANSI로 변환하여 출력
				#ifdef UNICODE
				wchar_t wideBuffer[1001] = { 0 };
				MultiByteToWideChar(CP_ACP, 0, rawBuffer, -1, wideBuffer, 1001);
				_ftprintf(debugLog, _T("%s"), wideBuffer);
				#else
				_ftprintf(debugLog, _T("%s"), rawBuffer);
				#endif
				
				_ftprintf(debugLog, _T("\n---END---\n"));
				
				// PubTagMapping 섹션 찾기
				if (strstr(rawBuffer, "[PubTagMapping]") || strstr(rawBuffer, "[PUBTAGMAPPING]")) {
					_ftprintf(debugLog, _T("\n✓ [PubTagMapping] 섹션이 파일에 존재함!\n"));
				}
				else {
					_ftprintf(debugLog, _T("\n✗ [PubTagMapping] 섹션을 파일에서 찾을 수 없음\n"));
					_ftprintf(debugLog, _T("   → INI 파일이 너무 크거나 섹션이 1000바이트 이후에 있을 수 있음\n"));
				}
			}
			else {
				_ftprintf(debugLog, _T("INI 파일 직접 열기 실패\n"));
			}
		}

		// 스마트한 INI 섹션 읽기
		std::vector<CString> tagLines;
		if (!ReadIniSectionSmart(_T("PubTagMapping"), tagLines)) {
			TRACE("!!! PubTagMapping 섹션 읽기 실패 !!!\n");
			TRACE("힌트: INI 파일에 [PubTagMapping] 섹션이 있는지 확인하세요.\n");
			TRACE("현재 Device 설정: %s\n", (LPCTSTR)m_deviceType);
			
			if (debugLog) {
				_ftprintf(debugLog, _T("\nERROR: PubTagMapping 섹션 읽기 실패\n"));
				_ftprintf(debugLog, _T("원인:\n"));
				_ftprintf(debugLog, _T("  1. INI 파일에 [PubTagMapping] 섹션이 없음\n"));
				_ftprintf(debugLog, _T("  2. 섹션이 비어있음\n"));
				_ftprintf(debugLog, _T("  3. INI 파일 인코딩 문제 (UTF-8 BOM 권장)\n"));
				_ftprintf(debugLog, _T("\n해결 방법:\n"));
				_ftprintf(debugLog, _T("  1. INI 파일을 열어 [PubTagMapping] 섹션 추가\n"));
				_ftprintf(debugLog, _T("  2. 예시:\n"));
				_ftprintf(debugLog, _T("     [PubTagMapping]\n"));
				_ftprintf(debugLog, _T("     tag1=topic1,/path/to/data\n"));
				_ftprintf(debugLog, _T("     tag2=topic2,/path/to/value\n"));
				fclose(debugLog);
			}
			return false;
		}

		TRACE("읽어온 라인 수: %d\n", tagLines.size());
		if (debugLog) {
			_ftprintf(debugLog, _T("\n읽어온 라인 수: %d\n"), tagLines.size());
		}

		// 각 라인 파싱
		int successCount = 0;
		int skipCount = 0;

		for (const auto& line : tagLines) {
			CString tagName, mapping;

			if (ParseTagMappingLine(line, tagName, mapping)) {
				// 중복 태그 체크
				if (m_pubTagMappings.find(tagName) != m_pubTagMappings.end()) {
					TRACE("경고: 중복 태그 발견, 덮어씀 - %s\n", (LPCTSTR)tagName);
				}

				m_pubTagMappings[tagName] = mapping;
				m_pubTagOrder.push_back(tagName);  // INI 파일 순서대로 저장
				successCount++;

				// 처음 10개와 마지막 10개만 상세 로그
				if (successCount <= 10 || successCount > (int)tagLines.size() - 10) {
					TRACE("태그[%03d]: %s -> %s\n", successCount,
						(LPCTSTR)tagName, (LPCTSTR)mapping);
					if (debugLog) {
						_ftprintf(debugLog, _T("태그[%03d]: %s -> %s\n"), successCount,
							(LPCTSTR)tagName, (LPCTSTR)mapping);
					}
				}
			}
			else {
				skipCount++;
				if (debugLog && skipCount <= 10) {
					_ftprintf(debugLog, _T("파싱 실패 라인: %s\n"), (LPCTSTR)line);
				}
			}
		}

		TRACE("=== PubTagMapping 로드 완료 ===\n");
		TRACE("성공: %d개, 건너뜀: %d개, 전체: %d개\n",
			successCount, skipCount, tagLines.size());

		if (debugLog) {
			_ftprintf(debugLog, _T("\n=== 로드 완료 ===\n"));
			_ftprintf(debugLog, _T("성공: %d개, 건너뜀: %d개, 전체: %d개\n"),
				successCount, skipCount, tagLines.size());
			_ftprintf(debugLog, _T("반환값: %s\n"), (successCount > 0) ? _T("true") : _T("false"));
			fclose(debugLog);
		}

		// 메모리 사용량 추정
		size_t estimatedMemory = 0;
		for (const auto& pair : m_pubTagMappings) {
			estimatedMemory += (pair.first.GetLength() + pair.second.GetLength()) * sizeof(TCHAR);
		}
		TRACE("추정 메모리 사용량: %.2f KB\n", estimatedMemory / 1024.0);

		return successCount > 0;
	}
	catch (const std::exception& e) {
		TRACE("PubTagMapping 로드 중 예외: %s\n", e.what());
		return false;
	}
	catch (...) {
		TRACE("PubTagMapping 로드 중 알 수 없는 예외\n");
		return false;
	}
}

// ===== 하위 호환성 (기존 코드용) =====
bool CConfigManager::LoadTagMappings()
{
	// Device Type에 따라 적절한 매핑 로드
	if (m_deviceType.CompareNoCase(_T("IFM")) == 0) {
		return LoadSubTagMappings();
	}
	else if (m_deviceType.CompareNoCase(_T("Navifra")) == 0) {
		return LoadPubTagMappings();
	}
	return LoadSubTagMappings();  // 기본값
}

bool CConfigManager::ReadIniSectionSmart(const CString& sectionName, std::vector<CString>& lines)
{
	lines.clear();

	// 섹션 존재 여부 확인 (더미 키로 테스트)
	TCHAR testBuffer[4] = { 0 };
	DWORD testRead = GetPrivateProfileSection(sectionName, testBuffer, 4, m_iniFilePath);
	
	if (testRead == 0) {
		TRACE("경고: [%s] 섹션이 INI 파일에 존재하지 않거나 비어있음\n", (LPCTSTR)sectionName);
		TRACE("INI 경로: %s\n", (LPCTSTR)m_iniFilePath);
		
		// 섹션 존재 여부를 더 정확하게 확인
		// GetPrivateProfileString으로 섹션 내 임의 키 읽기 시도
		TCHAR dummyValue[16] = { 0 };
		GetPrivateProfileString(sectionName, _T("_dummy_"), _T("_not_found_"), dummyValue, 16, m_iniFilePath);
		
		if (_tcscmp(dummyValue, _T("_not_found_")) == 0) {
			TRACE("확인: [%s] 섹션이 실제로 존재하지 않음\n", (LPCTSTR)sectionName);
			return false;
		}
	}

	DWORD initialSize = GetOptimalBufferSize(sectionName);
	TRACE("초기 버퍼 크기: %d KB\n", initialSize / 1024);

	// 동적 버퍼로 읽기 시도
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

	// actualRead가 0이면 섹션이 비어있음
	if (actualRead == 0) {
		TRACE("경고: [%s] 섹션이 비어있음 (actualRead = 0)\n", (LPCTSTR)sectionName);
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
		
		// 라인이 하나도 없으면 실패로 처리
		if (lineCount == 0) {
			TRACE("경고: [%s] 섹션에서 유효한 라인을 찾지 못함\n", (LPCTSTR)sectionName);
			return false;
		}
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


// ===== Subscribe 태그 매핑 저장 (IFM 모드) =====
bool CConfigManager::SaveSubTagMappings()
{
	try {
		WritePrivateProfileSection(_T("SubTagMapping"), NULL, m_iniFilePath);

		// 새로운 태그 매핑들 저장
		for (const auto& mapping : m_subTagMappings) {
			WritePrivateProfileString(_T("SubTagMapping"), mapping.first, mapping.second, m_iniFilePath);
		}

		return true;
	}
	catch (const std::exception& e) {
		OutputDebugStringA("SubTagMapping 저장 오류: ");
		OutputDebugStringA(e.what());
		OutputDebugStringA("\n");
		return false;
	}
}

// ===== Publish 태그 매핑 저장 =====
bool CConfigManager::SavePubTagMappings()
{
	try {
		WritePrivateProfileSection(_T("PubTagMapping"), NULL, m_iniFilePath);

		// 새로운 태그 매핑들 저장
		for (const auto& mapping : m_pubTagMappings) {
			WritePrivateProfileString(_T("PubTagMapping"), mapping.first, mapping.second, m_iniFilePath);
		}

		return true;
	}
	catch (const std::exception& e) {
		OutputDebugStringA("PubTagMapping 저장 오류: ");
		OutputDebugStringA(e.what());
		OutputDebugStringA("\n");
		return false;
	}
}

// ===== 하위 호환성 (기존 코드용) =====
bool CConfigManager::SaveTagMappings()
{
	// Device Type에 따라 적절한 매핑 저장
	if (m_deviceType.CompareNoCase(_T("IFM")) == 0) {
		return SaveSubTagMappings();
	}
	else if (m_deviceType.CompareNoCase(_T("Navifra")) == 0) {
		return SavePubTagMappings();
	}
	return SaveSubTagMappings();  // 기본값
}

void CConfigManager::AddTagMapping(const CString& tagName, const CString& mapping)
{
	// Device Type에 따라 적절한 매핑에 추가
	if (m_deviceType.CompareNoCase(_T("IFM")) == 0) {
		m_subTagMappings[tagName] = mapping;
		WritePrivateProfileString(_T("SubTagMapping"), tagName, mapping, m_iniFilePath);
	}
	else if (m_deviceType.CompareNoCase(_T("Navifra")) == 0) {
		m_pubTagMappings[tagName] = mapping;
		WritePrivateProfileString(_T("PubTagMapping"), tagName, mapping, m_iniFilePath);
	}
	else {
		m_subTagMappings[tagName] = mapping;
		WritePrivateProfileString(_T("SubTagMapping"), tagName, mapping, m_iniFilePath);
	}
	TRACE("태그 매핑 추가: %s -> %s\n", (LPCTSTR)tagName, (LPCTSTR)mapping);
}

void CConfigManager::SetTagMapping(const CString& tagName, const CString& mapping)
{
	TRACE("=== SetTagMapping 호출 ===\n");
	TRACE("Device Type: %s\n", (LPCTSTR)m_deviceType);
	TRACE("INI 경로: %s\n", (LPCTSTR)m_iniFilePath);
	TRACE("태그: %s -> %s\n", (LPCTSTR)tagName, (LPCTSTR)mapping);

	// Device Type에 따라 적절한 매핑 설정
	BOOL writeResult = FALSE;
	if (m_deviceType.CompareNoCase(_T("IFM")) == 0) {
		m_subTagMappings[tagName] = mapping;
		writeResult = WritePrivateProfileString(_T("SubTagMapping"), tagName, mapping, m_iniFilePath);
		TRACE("SubTagMapping 저장 결과: %s\n", writeResult ? _T("성공") : _T("실패"));
	}
	else if (m_deviceType.CompareNoCase(_T("Navifra")) == 0) {
		m_pubTagMappings[tagName] = mapping;
		writeResult = WritePrivateProfileString(_T("PubTagMapping"), tagName, mapping, m_iniFilePath);
		TRACE("PubTagMapping 저장 결과: %s (에러코드: %d)\n",
			writeResult ? _T("성공") : _T("실패"), GetLastError());
	}
	else {
		m_subTagMappings[tagName] = mapping;
		writeResult = WritePrivateProfileString(_T("SubTagMapping"), tagName, mapping, m_iniFilePath);
		TRACE("SubTagMapping(기본) 저장 결과: %s\n", writeResult ? _T("성공") : _T("실패"));
	}
	TRACE("태그 매핑 설정: %s -> %s\n", (LPCTSTR)tagName, (LPCTSTR)mapping);
}

void CConfigManager::RemoveTagMapping(const CString& tagName)
{
	// Device Type에 따라 적절한 매핑에서 삭제
	if (m_deviceType.CompareNoCase(_T("IFM")) == 0) {
		auto it = m_subTagMappings.find(tagName);
		if (it != m_subTagMappings.end()) {
			m_subTagMappings.erase(it);
			WritePrivateProfileString(_T("SubTagMapping"), tagName, NULL, m_iniFilePath);
		}
	}
	else if (m_deviceType.CompareNoCase(_T("Navifra")) == 0) {
		auto it = m_pubTagMappings.find(tagName);
		if (it != m_pubTagMappings.end()) {
			m_pubTagMappings.erase(it);
			WritePrivateProfileString(_T("PubTagMapping"), tagName, NULL, m_iniFilePath);
		}
	}
	else {
		auto it = m_subTagMappings.find(tagName);
		if (it != m_subTagMappings.end()) {
			m_subTagMappings.erase(it);
			WritePrivateProfileString(_T("SubTagMapping"), tagName, NULL, m_iniFilePath);
		}
	}
	TRACE("태그 매핑 삭제: %s\n", (LPCTSTR)tagName);
}

bool CConfigManager::HasTagMapping(const CString& tagName) const
{
	// Device Type에 따라 적절한 매핑 검색
	if (m_deviceType.CompareNoCase(_T("IFM")) == 0) {
		return m_subTagMappings.find(tagName) != m_subTagMappings.end();
	}
	else if (m_deviceType.CompareNoCase(_T("Navifra")) == 0) {
		return m_pubTagMappings.find(tagName) != m_pubTagMappings.end();
	}
	return m_subTagMappings.find(tagName) != m_subTagMappings.end();
}

CString CConfigManager::GetTagMapping(const CString& tagName) const
{
	// Device Type에 따라 적절한 매핑 조회
	if (m_deviceType.CompareNoCase(_T("IFM")) == 0) {
		auto it = m_subTagMappings.find(tagName);
		if (it != m_subTagMappings.end()) {
			return it->second;
		}
	}
	else if (m_deviceType.CompareNoCase(_T("Navifra")) == 0) {
		auto it = m_pubTagMappings.find(tagName);
		if (it != m_pubTagMappings.end()) {
			return it->second;
		}
	}
	else {
		auto it = m_subTagMappings.find(tagName);
		if (it != m_subTagMappings.end()) {
			return it->second;
		}
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

// Device 타입 관련 메서드
void CConfigManager::SetDeviceType(const CString& deviceType)
{
	m_deviceType = deviceType;
	TRACE("Device 타입 설정: %s\n", (LPCTSTR)deviceType);
}

CString CConfigManager::GetDeviceType() const
{
	return m_deviceType;
}

// MQTT 구독 토픽 관련 메서드
void CConfigManager::SetSubscribeTopic(const CString& topic)
{
	m_subscribeTopic = topic;
	TRACE("구독 토픽 설정: %s\n", (LPCTSTR)topic);
}

CString CConfigManager::GetSubscribeTopic() const
{
	return m_subscribeTopic;
}

// Autorun 관련 메서드
void CConfigManager::SetAutorun(int autorun)
{
	m_autorun = autorun;
	TRACE("Autorun 설정: %d\n", autorun);
}

int CConfigManager::GetAutorun() const
{
	return m_autorun;
}

// Map 기반 빠른 조회 구현

void CConfigManager::BuildTopicHashMap()
{
	TRACE("=== Topic Map 구축 시작 ===\n");
	DWORD startTime = GetTickCount();

	m_topicToTagMap.clear();
	// std::map은 reserve 없음 (Red-Black Tree 구조)

	int successCount = 0;
	int wildcardCount = 0;

	// IFM 모드: SubTagMapping 사용
	for (const auto& mapping : m_subTagMappings) {
		const CString& tagName = mapping.first;
		const CString& tagMapping = mapping.second;

		// 토픽과 JSONPath 분리
		CString topic, jsonPath;
		int commaPos = tagMapping.Find(_T(","));
		
		if (commaPos > 0) {
			topic = tagMapping.Left(commaPos);
			jsonPath = tagMapping.Mid(commaPos + 1);
			topic.Trim();
			jsonPath.Trim();
		}
		else {
			// 콤마가 없으면 전체가 JSONPath (와일드카드 토픽)
			topic = _T("+");
			jsonPath = tagMapping;
			jsonPath.Trim();
			wildcardCount++;
		}

	// multimap에 추가 (중복 허용)
	if (topic != _T("+")) {
		m_topicToTagMap.insert({topic, TagMappingInfo(tagName, jsonPath)});
		successCount++;

		// 디버깅: 처음 5개만 출력
		if (successCount <= 5) {
			TRACE("  [%d] Map에 추가: Topic='%S' → Tag='%S', JSONPath='%S'\n",
				successCount, (LPCTSTR)topic, (LPCTSTR)tagName, (LPCTSTR)jsonPath);
		}
	}
	else {
		// 와일드카드 토픽은 Map에 추가하지 않음
		if (wildcardCount <= 3) {
			TRACE("  [와일드카드 %d] 건너뜀: Tag='%S', Mapping='%S'\n",
				wildcardCount, (LPCTSTR)tagName, (LPCTSTR)tagMapping);
		}
	}
	}

	DWORD elapsed = GetTickCount() - startTime;

	// 중복 토픽 통계 계산
	int duplicateTopics = successCount - (int)m_topicToTagMap.size();

	TRACE("=== Topic Map 구축 완료 ===\n");
	TRACE("성공: %d개, 와일드카드: %d개, 전체: %d개\n",
		successCount, wildcardCount, m_subTagMappings.size());
	TRACE("Map 크기: %d entries (고유 토픽 수)\n", m_topicToTagMap.size());

	if (duplicateTopics > 0) {
		TRACE("✅ 중복 토픽: %d개 (multimap으로 모두 처리됨)\n", duplicateTopics);

		// 중복 토픽 목록 출력
		std::map<CString, int> topicCount;
		for (const auto& mapping : m_subTagMappings) {
			const CString& tagMapping = mapping.second;
			int commaPos = tagMapping.Find(_T(","));
			if (commaPos > 0) {
				CString topic = tagMapping.Left(commaPos);
				topic.Trim();
				if (topic != _T("+")) {
					topicCount[topic]++;
				}
			}
		}

		int duplicateCountLimit = 0;
		for (const auto& tc : topicCount) {
			if (tc.second > 1 && duplicateCountLimit < 5) {
				TRACE("  토픽 '%S': %d개 태그 매핑됨\n", (LPCTSTR)tc.first, tc.second);
				duplicateCountLimit++;
			}
		}
		if (duplicateTopics > 5) {
			TRACE("  ... 외 %d개 더 있음\n", duplicateTopics - 5);
		}
	}

	TRACE("구축 시간: %d ms\n", elapsed);
	TRACE("검색 성능: O(log N) - multimap equal_range 사용\n");
}

std::vector<TagMappingInfo> CConfigManager::GetAllTagsByTopic(const CString& topic) const
{
	std::vector<TagMappingInfo> results;

	// multimap에서 같은 토픽의 모든 매핑 찾기 (O(log N + M), M = 중복 개수)
	auto range = m_topicToTagMap.equal_range(topic);

	for (auto it = range.first; it != range.second; ++it) {
		results.push_back(it->second);
	}

	return results;
}
