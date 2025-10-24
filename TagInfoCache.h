// TagInfoCache.h
// EasyView Tag 정보 캐싱 시스템
// Phase 1: 태그 조회 성능 최적화 (5-10배 개선 목표)

#pragma once

#include <map>
#include <mutex>

// ============================================================================
// CTagInfoCache - EasyView 태그 정보 캐싱 클래스
// ============================================================================
// 목적: EV_GetTagInfo, EV_GetAiTagInfo 등의 반복 호출 제거
// 성능: 매 메시지마다 API 호출 (10-50ms) → 캐시 조회 (<1ms)
// ============================================================================

class CTagInfoCache
{
public:
	// 캐시 엔트리 구조체
	struct TagCacheEntry {
		int nStnPos;      // Station Position
		int nTagPos;      // Tag Position
		int nSBOffset;    // ScanBuffer Offset (가장 중요!)
		int nTagType;     // Tag Type (AI/DI/SI 등)
		bool isValid;     // 캐시 유효성 플래그

		// Phase 3 (Safe): 태그 구조체 포인터 캐싱
		ST_EV_TAG_ANALOG_INPUT*  pAiTag;   // AI 태그 포인터
		ST_EV_TAG_DIGITAL_INPUT* pDiTag;   // DI 태그 포인터
		ST_EV_TAG_STRING_INPUT*  pSiTag;   // SI 태그 포인터

		TagCacheEntry()
			: nStnPos(0), nTagPos(0), nSBOffset(0),
			nTagType(0), isValid(false),
			pAiTag(nullptr), pDiTag(nullptr), pSiTag(nullptr)
		{
		}

		TagCacheEntry(int stnPos, int tagPos, int sbOffset, int tagType)
			: nStnPos(stnPos), nTagPos(tagPos), nSBOffset(sbOffset),
			nTagType(tagType), isValid(true),
			pAiTag(nullptr), pDiTag(nullptr), pSiTag(nullptr)
		{
		}
	};

	// 캐시 통계 구조체 (성능 측정용)
	struct CacheStats {
		int hitCount;      // 캐시 히트 횟수
		int missCount;     // 캐시 미스 횟수
		int totalSize;     // 캐시 크기 (태그 개수)
		double hitRate;    // 히트율 (%)

		CacheStats()
			: hitCount(0), missCount(0), totalSize(0), hitRate(0.0)
		{
		}
	};

public:
	CTagInfoCache();
	~CTagInfoCache();

	// ========================================================================
	// 초기화 및 사전 로딩
	// ========================================================================

	// ConfigManager의 태그 매핑을 기반으로 모든 태그 사전 로드
	// - 실행 시점: CThreadSub::Run() 시작 시
	// - Thread-safe: 내부에서 mutex 사용
	void PreloadAllTags(const std::map<CString, CString>& tagMappings);

	// 특정 태그 수동 로드 (필요 시)
	bool LoadSingleTag(const CString& tagName);

	// ========================================================================
	// 조회 함수
	// ========================================================================

	// 캐시에서 태그 정보 조회 (Thread-safe)
	// 반환값: true = 캐시 히트, false = 캐시 미스
	bool GetCachedTagInfo(const CString& tagName, TagCacheEntry& outInfo);

	// 태그가 캐시에 존재하는지만 확인
	bool IsTagCached(const CString& tagName) const;

	// ========================================================================
	// 관리 함수
	// ========================================================================

	// 캐시 전체 삭제 (ConfigManager 설정 변경 시 호출)
	void Clear();

	// 특정 태그만 캐시에서 제거
	void RemoveTag(const CString& tagName);

	// ========================================================================
	// 통계 및 디버깅
	// ========================================================================

	// 캐시 통계 조회
	void GetCacheStats(CacheStats& outStats) const;

	// 캐시 상태 출력 (디버깅용)
	void PrintCacheStatus() const;

	// 성능 측정용: 캐시 히트율 반환
	double GetHitRate() const;

private:
	// ========================================================================
	// 내부 함수
	// ========================================================================

	// EasyView API 호출하여 태그 정보 로드
	bool LoadTagInfoFromAPI(const CString& tagName, TagCacheEntry& outEntry);

	// AI 태그 SBOffset 조회
	bool GetAiTagSBOffset(int nStnPos, int nTagPos, int& outSBOffset);

	// DI 태그 SBOffset 조회
	bool GetDiTagSBOffset(int nStnPos, int nTagPos, int& outSBOffset);

	// SI 태그 SBOffset 조회
	bool GetSiTagSBOffset(int nStnPos, int nTagPos, int& outSBOffset);

private:
	// ========================================================================
	// 멤버 변수
	// ========================================================================

	// 태그 캐시 (태그명 → 태그정보)
	// std::map 사용 이유: C++98 호환 + 정렬 보장
	std::map<CString, TagCacheEntry> m_tagCache;

	// Thread-safe를 위한 뮤텍스
	mutable std::mutex m_mutex;

	// 초기화 플래그
	bool m_initialized;

	// 통계 카운터 (성능 측정용)
	mutable int m_hitCount;
	mutable int m_missCount;

	// 디버깅 플래그 (TRACE 출력 제어)
	bool m_enableDebugTrace;
};

// ============================================================================
// 전역 싱글톤 인스턴스
// ============================================================================
// 사용 예:
//   g_tagCache.PreloadAllTags(tagMappings);
//   g_tagCache.GetCachedTagInfo(tagName, entry);
// ============================================================================
extern CTagInfoCache g_tagCache;
