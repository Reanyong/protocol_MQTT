#include "pch.h"
#include "JsonResultManager.h"

CJsonResultManager::CJsonResultManager()
{
}

CJsonResultManager::~CJsonResultManager()
{
}

CJsonResultManager& CJsonResultManager::GetInstance()
{
    static CJsonResultManager instance;
    return instance;
}

void CJsonResultManager::StoreResult(const CString& filePath, const CJsonParser::EventData& eventData)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_results[filePath] = eventData;
}

bool CJsonResultManager::GetResult(const CString& filePath, CJsonParser::EventData& eventData)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_results.find(filePath);
    if (it != m_results.end()) {
        eventData = it->second;
        return true;
    }
    return false;
}

std::vector<CString> CJsonResultManager::GetAvailableResults() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CString> filePaths;
    for (const auto& pair : m_results) {
        filePaths.push_back(pair.first);
    }
    return filePaths;
}

void CJsonResultManager::ClearResult(const CString& filePath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_results.erase(filePath);
}

void CJsonResultManager::ClearAllResults()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_results.clear();
}
