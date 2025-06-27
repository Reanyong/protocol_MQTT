#include "pch.h"
#include "LogManager.h"
#include <io.h>

CLogManager::CLogManager()
{
	m_logFilePath = CreateLogFilePath();
	TRACE("로그 파일 경로: %s\n", (LPCTSTR)m_logFilePath);
}

CLogManager::~CLogManager()
{
}

CLogManager& CLogManager::GetInstance()
{
	static CLogManager instance;
	return instance;
}

CString CLogManager::CreateLogFilePath() const
{
	TCHAR szPath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, szPath, MAX_PATH);

	// 실행 파일 디렉토리에 로그 파일 생성
	CString strPath(szPath);
	int nPos = strPath.ReverseFind('\\');
	if (nPos > 0) {
		return strPath.Left(nPos + 1) + _T("EVMQTT.log");
	}
	else {
		return _T("EVMQTT.log");
	}
}

void CLogManager::WriteErrorLog(const CString& logType, const CString& source,
	const CString& errorMessage, const CString& additionalInfo)
{
	std::lock_guard<std::mutex> lock(m_logMutex);

	try {
		// UTF-8로 파일에 기록
		std::ofstream logFile;

		// 멀티바이트 환경에서 UTF-8 파일 열기
		CT2A pathA(m_logFilePath, CP_UTF8);
		logFile.open(pathA, std::ios::app | std::ios::out);

		if (logFile.is_open()) {
			// 로그 라인 포맷팅
			CString timeStr = GetCurrentTimeString();
			CString logLine;

			if (additionalInfo.IsEmpty()) {
				logLine.Format(_T("[%s] [%s] [%s] %s"),
					timeStr, logType, source, errorMessage);
			}
			else {
				logLine.Format(_T("[%s] [%s] [%s] %s | %s"),
					timeStr, logType, source, errorMessage, additionalInfo);
			}

			// UTF-8로 변환하여 기록
			CT2A logLineA(logLine, CP_UTF8);
			logFile << logLineA.m_psz << std::endl;

			logFile.close();

			// 로그 파일 크기 관리
			ManageLogFileSize();
		}
		else {
			TRACE("로그 파일 열기 실패: %s\n", (LPCTSTR)m_logFilePath);
		}

		// 디버그 출력
		TRACE("=== ERROR LOG WRITTEN ===\n");
		TRACE("타입: %s\n", (LPCTSTR)logType);
		TRACE("소스: %s\n", (LPCTSTR)source);
		TRACE("메시지: %s\n", (LPCTSTR)errorMessage);
		if (!additionalInfo.IsEmpty()) {
			TRACE("추가정보: %s\n", (LPCTSTR)additionalInfo);
		}
		TRACE("=========================\n");
	}
	catch (const std::exception& e) {
		TRACE("로그 기록 중 예외 발생: %s\n", e.what());
	}
	catch (...) {
		TRACE("로그 기록 중 알 수 없는 예외 발생\n");
	}
}

CString CLogManager::GetLogFilePath() const
{
	return m_logFilePath;
}

bool CLogManager::LogFileExists() const
{
	try {
		CT2A pathA(m_logFilePath, CP_UTF8);
		return (_access(pathA, 0) == 0);
	}
	catch (...) {
		return false;
	}
}

int CLogManager::GetLogFileSize() const
{
	try {
		CT2A pathA(m_logFilePath, CP_UTF8);
		struct _stat fileInfo;
		if (_stat(pathA, &fileInfo) == 0) {
			return static_cast<int>(fileInfo.st_size / 1024); // KB 단위
		}
	}
	catch (...) {
	}

	return 0;
}

CString CLogManager::GetCurrentTimeString() const
{
	CTime currentTime = CTime::GetCurrentTime();
	return currentTime.Format(_T("%Y-%m-%d %H:%M:%S"));
}

void CLogManager::ManageLogFileSize()
{
	try {
		// 10MB 이상이면 파일 절반으로 줄이기
		if (GetLogFileSize() > 10240) { // 10MB = 10240KB
			TRACE("로그 파일 크기가 10MB를 초과하여 정리합니다.\n");

			CT2A pathA(m_logFilePath, CP_UTF8);

			// 임시 파일명
			CString tempPath = m_logFilePath + _T(".tmp");
			CT2A tempPathA(tempPath, CP_UTF8);

			std::ifstream oldFile(pathA, std::ios::in);
			std::ofstream newFile(tempPathA, std::ios::out);

			if (oldFile.is_open() && newFile.is_open()) {
				std::vector<std::string> lines;
				std::string line;

				// 모든 라인 읽기
				while (std::getline(oldFile, line)) {
					lines.push_back(line);
				}

				// 최근 절반만 새 파일에 쓰기
				size_t startIndex = lines.size() / 2;
				for (size_t i = startIndex; i < lines.size(); i++) {
					newFile << lines[i] << std::endl;
				}

				oldFile.close();
				newFile.close();

				// 원본 파일 삭제하고 임시 파일을 원본으로 변경
				remove(pathA);
				rename(tempPathA, pathA);

				TRACE("로그 파일 정리 완료: %d개 라인 유지\n", lines.size() - startIndex);
			}
		}
	}
	catch (const std::exception& e) {
		TRACE("로그 파일 정리 중 예외: %s\n", e.what());
	}
}
