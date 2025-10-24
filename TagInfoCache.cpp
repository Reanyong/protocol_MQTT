// TagInfoCache.cpp
// EasyView Tag 정보 캐싱 시스템 구현

#include "pch.h"
#include "TagInfoCache.h"
#include "ConfigManager.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// ============================================================================
// 전역 싱글톤 인스턴스
// ============================================================================
CTagInfoCache g_tagCache;

// ============================================================================
// 생성자/소멸자
// ============================================================================

CTagInfoCache::CTagInfoCache()
	: m_initialized(false)
	, m_hitCount(0)
	, m_missCount(0)
	, m_enableDebugTrace(true)  // 초기에는 디버깅 활성화
{
	TRACE("CTagInfoCache: Constructor called\n");
}

CTagInfoCache::~CTagInfoCache()
{
	TRACE("CTagInfoCache: Destructor called\n");
	Clear();
}

// ============================================================================
// 초기화 및 사전 로딩
// ============================================================================

void CTagInfoCache::PreloadAllTags(const std::map<CString, CString>& tagMappings)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	TRACE("=== TagInfoCache: PreloadAllTags Started ===\n");
	TRACE("Total tags to preload: %d\n", tagMappings.size());

	DWORD startTime = GetTickCount();
	int successCount = 0;
	int failCount = 0;

	// 기존 캐시 초기화
	m_tagCache.clear();
	m_hitCount = 0;
	m_missCount = 0;

	// 모든 태그 사전 로드
	for (const auto& mapping : tagMappings)
	{
		const CString& tagName = mapping.first;
		const CString& tagMapping = mapping.second;

		TRACE("[PRELOAD] 태그명='%S', 매핑='%S'\n", (LPCTSTR)tagName, (LPCTSTR)tagMapping);

		TagCacheEntry entry;
		if (LoadTagInfoFromAPI(tagName, entry))
		{
			m_tagCache[tagName] = entry;
			successCount++;

			TRACE("  [SUCCESS %d] 캐시 저장: 태그='%S' → StnPos=%d, TagPos=%d, SBOffset=%d, Type=%d\n",
				successCount, (LPCTSTR)tagName,
				entry.nStnPos, entry.nTagPos, entry.nSBOffset, entry.nTagType);
		}
		else
		{
			failCount++;
			TRACE("  [FAILED %d] 태그 로드 실패: '%S'\n", failCount, (LPCTSTR)tagName);
		}
	}

	DWORD elapsed = GetTickCount() - startTime;

	m_initialized = true;

	TRACE("=== TagInfoCache: PreloadAllTags Completed ===\n");
	TRACE("Success: %d, Failed: %d, Total: %d\n", successCount, failCount, tagMappings.size());
	TRACE("Loading time: %d ms (avg: %.2f ms/tag)\n", elapsed,
		tagMappings.size() > 0 ? (double)elapsed / tagMappings.size() : 0.0);
	TRACE("Cache size: %d entries\n", m_tagCache.size());
	TRACE("Estimated memory: ~%d bytes\n", m_tagCache.size() * sizeof(TagCacheEntry));
}

bool CTagInfoCache::LoadSingleTag(const CString& tagName)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	// 이미 캐시에 있으면 재로드 안 함
	if (m_tagCache.find(tagName) != m_tagCache.end()) {
		TRACE("TagInfoCache: Tag already cached: %S\n", (LPCTSTR)tagName);
		return true;
	}

	TagCacheEntry entry;
	if (LoadTagInfoFromAPI(tagName, entry))
	{
		m_tagCache[tagName] = entry;
		TRACE("TagInfoCache: Tag loaded: %S → SBOffset=%d\n",
			(LPCTSTR)tagName, entry.nSBOffset);
		return true;
	}

	TRACE("TagInfoCache: Failed to load tag: %S\n", (LPCTSTR)tagName);
	return false;
}

// ============================================================================
// 조회 함수
// ============================================================================

bool CTagInfoCache::GetCachedTagInfo(const CString& tagName, TagCacheEntry& outInfo)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_tagCache.find(tagName);
	if (it != m_tagCache.end())
	{
		// 캐시 히트
		outInfo = it->second;
		m_hitCount++;

		// 10000번마다 한 번만 로그 출력 (스팸 방지)
		if (m_enableDebugTrace && m_hitCount % 10000 == 0) {
			TRACE("TagInfoCache: Cache HIT #%d - %S (SBOffset=%d)\n",
				m_hitCount, (LPCTSTR)tagName, outInfo.nSBOffset);
		}

		return true;
	}

	// 캐시 미스
	m_missCount++;

	// 미스는 중요하므로 처음 10개는 로그 출력
	if (m_missCount <= 10) {
		TRACE("TagInfoCache: Cache MISS #%d - %S (attempting to load...)\n",
			m_missCount, (LPCTSTR)tagName);
	}

	// 캐시 미스 시 즉시 로드 시도
	if (LoadTagInfoFromAPI(tagName, outInfo))
	{
		m_tagCache[tagName] = outInfo;
		TRACE("TagInfoCache: Tag loaded on-demand: %S → SBOffset=%d\n",
			(LPCTSTR)tagName, outInfo.nSBOffset);
		return true;
	}

	TRACE("TagInfoCache: Failed to load tag on-demand: %S\n", (LPCTSTR)tagName);
	return false;
}

bool CTagInfoCache::IsTagCached(const CString& tagName) const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_tagCache.find(tagName) != m_tagCache.end();
}

// ============================================================================
// 관리 함수
// ============================================================================

void CTagInfoCache::Clear()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	TRACE("TagInfoCache: Clearing cache (%d entries)\n", m_tagCache.size());

	m_tagCache.clear();
	m_hitCount = 0;
	m_missCount = 0;
	m_initialized = false;
}

void CTagInfoCache::RemoveTag(const CString& tagName)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_tagCache.find(tagName);
	if (it != m_tagCache.end())
	{
		m_tagCache.erase(it);
		TRACE("TagInfoCache: Tag removed: %S\n", (LPCTSTR)tagName);
	}
}

// ============================================================================
// 통계 및 디버깅
// ============================================================================

void CTagInfoCache::GetCacheStats(CacheStats& outStats) const
{
	std::lock_guard<std::mutex> lock(m_mutex);

	outStats.hitCount = m_hitCount;
	outStats.missCount = m_missCount;
	outStats.totalSize = static_cast<int>(m_tagCache.size());

	int totalAccess = m_hitCount + m_missCount;
	outStats.hitRate = totalAccess > 0 ? (100.0 * m_hitCount / totalAccess) : 0.0;
}

void CTagInfoCache::PrintCacheStatus() const
{
	std::lock_guard<std::mutex> lock(m_mutex);

	int totalAccess = m_hitCount + m_missCount;
	double hitRate = totalAccess > 0 ? (100.0 * m_hitCount / totalAccess) : 0.0;

	TRACE("=== TagInfoCache Status ===\n");
	TRACE("Initialized: %s\n", m_initialized ? "YES" : "NO");
	TRACE("Cache size: %d entries\n", m_tagCache.size());
	TRACE("Total access: %d (Hit: %d, Miss: %d)\n",
		totalAccess, m_hitCount, m_missCount);
	TRACE("Hit rate: %.2f%%\n", hitRate);
	TRACE("Memory usage: ~%d bytes\n", m_tagCache.size() * sizeof(TagCacheEntry));
	TRACE("===========================\n");
}

double CTagInfoCache::GetHitRate() const
{
	std::lock_guard<std::mutex> lock(m_mutex);

	int totalAccess = m_hitCount + m_missCount;
	return totalAccess > 0 ? (100.0 * m_hitCount / totalAccess) : 0.0;
}

// ============================================================================
// 내부 함수 - EasyView API 호출
// ============================================================================

bool CTagInfoCache::LoadTagInfoFromAPI(const CString& tagName, TagCacheEntry& outEntry)
{
	// 1. 기본 태그 정보 조회
	ST_EV_TAG_INFO tagInfo;
	int tagResult = EV_GetTagInfo(tagName, &tagInfo);
	if (tagResult <= 0) {
		return false;
	}

	// 2. 기본 정보 저장
	outEntry.nStnPos = tagInfo.nStnPos;
	outEntry.nTagPos = tagInfo.nTagPos;
	outEntry.nTagType = tagInfo.nTagType;
	outEntry.nSBOffset = tagInfo.nTagPos;  // 기본값 (fallback)

	// 3. 태그 타입별로 실제 SBOffset + 포인터 저장
	int sbOffset = 0;
	bool offsetFound = false;

	int errorCode = 0;
	switch (tagInfo.nTagType) {
	case TYPE_AI:
	case TYPE_AO:
	{
		ST_EV_TAG_ANALOG_INPUT* pAiTag = EV_GetAiTagInfo(tagInfo.nStnPos, tagInfo.nTagPos, &errorCode);
		if (pAiTag != nullptr && errorCode != 0) {
			outEntry.pAiTag = pAiTag;  // 포인터 저장!
			if (pAiTag->nSBOffset >= 0 && pAiTag->nSBOffset < 10000) {
				sbOffset = pAiTag->nSBOffset;
				offsetFound = true;
			}
		}
		break;
	}

	case TYPE_DI:
	case TYPE_DO:
	{
		ST_EV_TAG_DIGITAL_INPUT* pDiTag = EV_GetDiTagInfo(tagInfo.nStnPos, tagInfo.nTagPos, &errorCode);
		if (pDiTag != nullptr && errorCode == 0) {
			outEntry.pDiTag = pDiTag;  // 포인터 저장!
			if (pDiTag->nSBOffset >= 0 && pDiTag->nSBOffset < 10000) {
				sbOffset = pDiTag->nSBOffset;
				offsetFound = true;
			}
		}
		break;
	}

	case TYPE_SI:
	{
		ST_EV_TAG_STRING_INPUT* pSiTag = EV_GetSiTagInfo(tagInfo.nStnPos, tagInfo.nTagPos, &errorCode);
		if (pSiTag != nullptr && errorCode == 0) {
			outEntry.pSiTag = pSiTag;  // 포인터 저장!
			if (pSiTag->nSBOffset >= 0 && pSiTag->nSBOffset < 10000) {
				sbOffset = pSiTag->nSBOffset;
				offsetFound = true;
			}
		}
		break;
	}

	default:
		TRACE("TagInfoCache: Unsupported tag type %d for tag: %S\n",
			tagInfo.nTagType, (LPCTSTR)tagName);
		break;
	}

	// 4. SBOffset 업데이트
	if (offsetFound) {
		outEntry.nSBOffset = sbOffset;
	}

	outEntry.isValid = true;
	return true;
}

bool CTagInfoCache::GetAiTagSBOffset(int nStnPos, int nTagPos, int& outSBOffset)
{
	int errorCode = 0;
	ST_EV_TAG_ANALOG_INPUT* pAiTag = EV_GetAiTagInfo(nStnPos, nTagPos, &errorCode);

	if (pAiTag != nullptr && errorCode != 0) {
		// 공유 메모리 포인터이므로 즉시 복사
		int sbOffset = pAiTag->nSBOffset;

		// 유효성 검증
		if (sbOffset >= 0 && sbOffset < 10000) {
			outSBOffset = sbOffset;
			return true;
		}
	}

	return false;
}

bool CTagInfoCache::GetDiTagSBOffset(int nStnPos, int nTagPos, int& outSBOffset)
{
	int errorCode = 0;
	ST_EV_TAG_DIGITAL_INPUT* pDiTag = EV_GetDiTagInfo(nStnPos, nTagPos, &errorCode);

	if (pDiTag != nullptr && errorCode == 0) {
		int sbOffset = pDiTag->nSBOffset;

		if (sbOffset >= 0 && sbOffset < 10000) {
			outSBOffset = sbOffset;
			return true;
		}
	}

	return false;
}

bool CTagInfoCache::GetSiTagSBOffset(int nStnPos, int nTagPos, int& outSBOffset)
{
	int errorCode = 0;
	ST_EV_TAG_STRING_INPUT* pSiTag = EV_GetSiTagInfo(nStnPos, nTagPos, &errorCode);

	if (pSiTag != nullptr && errorCode == 0) {
		int sbOffset = pSiTag->nSBOffset;

		if (sbOffset >= 0 && sbOffset < 10000) {
			outSBOffset = sbOffset;
			return true;
		}
	}

	return false;
}
