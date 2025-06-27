#pragma once
#include <fstream>
#include <mutex>

// 간단한 실패 로그 관리 클래스
class CLogManager
{
public:
	CLogManager();
	virtual ~CLogManager();

	// 싱글톤 패턴
	static CLogManager& GetInstance();

	// 에러 로그 기록 (간단한 형태)
	void WriteErrorLog(const CString& logType, const CString& source,
		const CString& errorMessage, const CString& additionalInfo = _T(""));

	// 로그 파일 경로 반환
	CString GetLogFilePath() const;

	// 로그 파일이 존재하는지 확인
	bool LogFileExists() const;

	// 로그 파일 크기 확인 (KB 단위)
	int GetLogFileSize() const;

private:
	CString m_logFilePath;
	std::mutex m_logMutex;

	// 로그 파일 경로 생성
	CString CreateLogFilePath() const;

	// 로그 파일 크기 관리 (최대 10MB로 제한)
	void ManageLogFileSize();

	// 현재 시간을 문자열로 반환
	CString GetCurrentTimeString() const;
};
