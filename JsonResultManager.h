#pragma once
#include <map>
#include <vector>
#include <mutex>

// 간단한 결과 저장을 위한 구조체
struct SimpleEventData
{
	CString eventType;
	CString deviceId;
	CString timestamp;
	bool isValid;

	SimpleEventData() : isValid(false) {}
};

// 파싱 결과 저장 클래스
class CJsonResultManager
{
public:
	CJsonResultManager();
	virtual ~CJsonResultManager();

	// 결과 저장 및 조회
	void StoreResult(const CString& filePath, const SimpleEventData& eventData);
	bool GetResult(const CString& filePath, SimpleEventData& eventData);
	std::vector<CString> GetAvailableResults() const;
	void ClearResult(const CString& filePath);
	void ClearAllResults();

	// 싱글톤 패턴
	static CJsonResultManager& GetInstance();

private:
	std::map<CString, SimpleEventData> m_results;
	mutable std::mutex m_mutex;
};
