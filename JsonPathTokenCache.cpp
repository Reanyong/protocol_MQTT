// JsonPathTokenCache.cpp
// JSONPath 토큰 캐싱 시스템 구현 - Phase 2 최적화

#include "pch.h"
#include "JsonPathTokenCache.h"
#include "JsonPathUtil.h"
#include "ConfigManager.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// ============================================================================
// 전역 싱글톤 인스턴스
// ============================================================================
CJsonPathTokenCache g_jsonPathCache;

// ============================================================================
// 생성자/소멸자
// ============================================================================

CJsonPathTokenCache::CJsonPathTokenCache()
	: m_initialized(false)
	, m_hitCount(0)
	, m_missCount(0)
	, m_enableDebugTrace(true)  // 초기에는 디버깅 활성화
{
	TRACE("CJsonPathTokenCache: Constructor called\n");
}

CJsonPathTokenCache::~CJsonPathTokenCache()
{
	TRACE("CJsonPathTokenCache: Destructor called\n");
	Clear();
}

// ============================================================================
// 초기화 및 사전 로딩
// ============================================================================

void CJsonPathTokenCache::PreloadAllJsonPaths(const std::map<CString, CString>& tagMappings)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	TRACE("=== JsonPathTokenCache: PreloadAllJsonPaths Started ===\n");
	TRACE("Total mappings to preload: %d\n", tagMappings.size());

	DWORD startTime = GetTickCount();
	int successCount = 0;
	int failCount = 0;

	// 기존 캐시 초기화
	m_pathCache.clear();
	m_hitCount = 0;
	m_missCount = 0;

	// 모든 JSONPath 사전 파싱
	for (const auto& mapping : tagMappings)
	{
		const CString& tagName = mapping.first;
		const CString& tagMapping = mapping.second;

		// topic,jsonpath 형식에서 JSONPath만 추출
		CString jsonPath = ExtractJsonPathOnly(tagMapping);

		if (jsonPath.IsEmpty()) {
			TRACE("  [SKIP] 빈 JSONPath: 태그='%S'\n", (LPCTSTR)tagName);
			continue;
		}

		// 이미 캐시된 경우 스킵 (중복 제거)
		if (m_pathCache.find(jsonPath) != m_pathCache.end()) {
			TRACE("  [DUPLICATE] 이미 캐시됨: '%S'\n", (LPCTSTR)jsonPath);
			continue;
		}

		// JSONPath 파싱
		std::vector<std::string> tokens;
		if (ParseJsonPathToTokens(jsonPath, tokens))
		{
			m_pathCache[jsonPath] = tokens;
			successCount++;

			if (m_enableDebugTrace && successCount <= 5) {
				TRACE("  [SUCCESS %d] JSONPath 파싱: '%S' → %d개 토큰\n",
					successCount, (LPCTSTR)jsonPath, tokens.size());
			}
		}
		else
		{
			failCount++;
			if (failCount <= 3) {
				TRACE("  [FAILED %d] JSONPath 파싱 실패: '%S'\n", failCount, (LPCTSTR)jsonPath);
			}
		}
	}

	DWORD elapsed = GetTickCount() - startTime;

	m_initialized = true;

	TRACE("=== JsonPathTokenCache: PreloadAllJsonPaths Completed ===\n");
	TRACE("Success: %d, Failed: %d, Total mappings: %d\n", successCount, failCount, tagMappings.size());
	TRACE("Unique JSONPaths cached: %d\n", m_pathCache.size());
	TRACE("Loading time: %d ms (avg: %.2f ms/path)\n", elapsed,
		m_pathCache.size() > 0 ? (double)elapsed / m_pathCache.size() : 0.0);
	TRACE("Estimated memory: ~%d bytes\n", m_pathCache.size() * 100);  // 대략적 추정
}

bool CJsonPathTokenCache::LoadSingleJsonPath(const CString& jsonPath)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// 이미 캐시에 있으면 재로드 안 함
	if (m_pathCache.find(jsonPath) != m_pathCache.end()) {
		TRACE("JsonPathTokenCache: Path already cached: %S\n", (LPCTSTR)jsonPath);
		return true;
	}

	std::vector<std::string> tokens;
	if (ParseJsonPathToTokens(jsonPath, tokens))
	{
		m_pathCache[jsonPath] = tokens;
		TRACE("JsonPathTokenCache: Path loaded: %S → %d tokens\n",
			(LPCTSTR)jsonPath, tokens.size());
		return true;
	}

	TRACE("JsonPathTokenCache: Failed to load path: %S\n", (LPCTSTR)jsonPath);
	return false;
}

// ============================================================================
// 조회 함수
// ============================================================================

bool CJsonPathTokenCache::GetCachedTokens(const CString& jsonPath, std::vector<std::string>& outTokens)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_pathCache.find(jsonPath);
	if (it != m_pathCache.end())
	{
		// 캐시 히트
		outTokens = it->second;
		m_hitCount++;

		// 10000번마다 한 번만 로그 출력 (스팸 방지)
		if (m_enableDebugTrace && m_hitCount % 10000 == 0) {
			TRACE("JsonPathTokenCache: Cache HIT #%d - %S (%d tokens)\n",
				m_hitCount, (LPCTSTR)jsonPath, outTokens.size());
		}

		return true;
	}

	// 캐시 미스
	m_missCount++;

	// 미스는 중요하므로 처음 10개는 로그 출력
	if (m_missCount <= 10) {
		TRACE("JsonPathTokenCache: Cache MISS #%d - %S (attempting to load...)\n",
			m_missCount, (LPCTSTR)jsonPath);
	}

	// 캐시 미스 시 즉시 파싱 시도
	if (ParseJsonPathToTokens(jsonPath, outTokens))
	{
		m_pathCache[jsonPath] = outTokens;
		TRACE("JsonPathTokenCache: Path parsed on-demand: %S → %d tokens\n",
			(LPCTSTR)jsonPath, outTokens.size());
		return true;
	}

	TRACE("JsonPathTokenCache: Failed to parse path on-demand: %S\n", (LPCTSTR)jsonPath);
	return false;
}

bool CJsonPathTokenCache::IsPathCached(const CString& jsonPath) const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_pathCache.find(jsonPath) != m_pathCache.end();
}

// ============================================================================
// 관리 함수
// ============================================================================

void CJsonPathTokenCache::Clear()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	TRACE("JsonPathTokenCache: Clearing cache (%d entries)\n", m_pathCache.size());

	m_pathCache.clear();
	m_hitCount = 0;
	m_missCount = 0;
	m_initialized = false;
}

void CJsonPathTokenCache::RemovePath(const CString& jsonPath)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_pathCache.find(jsonPath);
	if (it != m_pathCache.end())
	{
		m_pathCache.erase(it);
		TRACE("JsonPathTokenCache: Path removed: %S\n", (LPCTSTR)jsonPath);
	}
}

// ============================================================================
// 통계 및 디버깅
// ============================================================================

void CJsonPathTokenCache::GetCacheStats(CacheStats& outStats) const
{
	std::lock_guard<std::mutex> lock(m_mutex);

	outStats.hitCount = m_hitCount;
	outStats.missCount = m_missCount;
	outStats.totalSize = static_cast<int>(m_pathCache.size());

	int totalAccess = m_hitCount + m_missCount;
	outStats.hitRate = totalAccess > 0 ? (100.0 * m_hitCount / totalAccess) : 0.0;
}

void CJsonPathTokenCache::PrintCacheStatus() const
{
	std::lock_guard<std::mutex> lock(m_mutex);

	int totalAccess = m_hitCount + m_missCount;
	double hitRate = totalAccess > 0 ? (100.0 * m_hitCount / totalAccess) : 0.0;

	TRACE("=== JsonPathTokenCache Status ===\n");
	TRACE("Initialized: %s\n", m_initialized ? "YES" : "NO");
	TRACE("Cache size: %d entries\n", m_pathCache.size());
	TRACE("Total access: %d (Hit: %d, Miss: %d)\n",
		totalAccess, m_hitCount, m_missCount);
	TRACE("Hit rate: %.2f%%\n", hitRate);
	TRACE("Memory usage: ~%d bytes\n", m_pathCache.size() * 100);
	TRACE("=================================\n");
}

double CJsonPathTokenCache::GetHitRate() const
{
	std::lock_guard<std::mutex> lock(m_mutex);

	int totalAccess = m_hitCount + m_missCount;
	return totalAccess > 0 ? (100.0 * m_hitCount / totalAccess) : 0.0;
}

// ============================================================================
// 내부 함수
// ============================================================================

bool CJsonPathTokenCache::ParseJsonPathToTokens(const CString& jsonPath, std::vector<std::string>& outTokens)
{
	try {
		// CJsonPathUtil의 기존 파싱 함수 호출
		outTokens = CJsonPathUtil::ParseJsonPath(jsonPath);
		return !outTokens.empty();
	}
	catch (const std::exception& e) {
		TRACE("JsonPathTokenCache: Parse error: %s (Path: %S)\n", e.what(), (LPCTSTR)jsonPath);
		return false;
	}
	catch (...) {
		TRACE("JsonPathTokenCache: Unknown parse error (Path: %S)\n", (LPCTSTR)jsonPath);
		return false;
	}
}

CString CJsonPathTokenCache::ExtractJsonPathOnly(const CString& fullMapping)
{
	// "topic,jsonpath" 형식에서 JSONPath만 추출
	int commaPos = fullMapping.Find(_T(","));
	if (commaPos > 0) {
		CString jsonPath = fullMapping.Mid(commaPos + 1);
		jsonPath.Trim();
		return jsonPath;
	}

	// 쉼표가 없으면 전체가 JSONPath
	CString jsonPath = fullMapping;
	jsonPath.Trim();
	return jsonPath;
}
