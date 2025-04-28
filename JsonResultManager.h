#pragma once
#include <map>
#include <string>
#include <mutex>
#include "json.hpp"
#include "ParserJSON.h"

// 파싱 결과 저장 클래스
class CJsonResultManager
{
public:
    CJsonResultManager();
    virtual ~CJsonResultManager();

    // 결과 저장 및 조회
    void StoreResult(const CString& filePath, const CJsonParser::EventData& eventData);
    bool GetResult(const CString& filePath, CJsonParser::EventData& eventData);
    std::vector<CString> GetAvailableResults() const;
    void ClearResult(const CString& filePath);
    void ClearAllResults();

    // 싱글톤 패턴
    static CJsonResultManager& GetInstance();

private:
    std::map<CString, CJsonParser::EventData> m_results;
    mutable std::mutex m_mutex;
};
