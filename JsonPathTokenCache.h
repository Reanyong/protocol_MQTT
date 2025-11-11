// JsonPathTokenCache.h
// JSONPath 토큰 캐싱 시스템

#pragma once

#include <map>
#include <vector>
#include <string>
#include <mutex>

// ============================================================================
// CJsonPathTokenCache - JSONPath 파싱 결과 캐싱 클래스
// ============================================================================

class CJsonPathTokenCache
{
public:
	// 캐시 통계 구조체
	struct CacheStats {
		int hitCount;      // 캐시 히트 횟수
		int missCount;     // 캐시 미스 횟수
		int totalSize;     // 캐시 크기 (JSONPath 개수)
		double hitRate;    // 히트율 (%)

		CacheStats()
			: hitCount(0), missCount(0), totalSize(0), hitRate(0.0)
		{
		}
	};

public:
	CJsonPathTokenCache();
	~CJsonPathTokenCache();

	// ========================================================================
	// 초기화 및 사전 로딩
	// ========================================================================

	// ConfigManager의 태그 매핑을 기반으로 모든 JSONPath 사전 파싱
	// - 실행 시점: CThreadSub::Run() 시작 시 (TagInfoCache 이후)
	// - Thread-safe: 내부에서 mutex 사용
	void PreloadAllJsonPaths(const std::map<CString, CString>& tagMappings);

	// 특정 JSONPath 수동 로드 (필요 시)
	bool LoadSingleJsonPath(const CString& jsonPath);

	// ========================================================================
	// 조회 함수
	// ========================================================================

	// 캐시에서 JSONPath 토큰 조회 (Thread-safe)
	// 반환값: true = 캐시 히트, false = 캐시 미스
	bool GetCachedTokens(const CString& jsonPath, std::vector<std::string>& outTokens);

	// JSONPath가 캐시에 존재하는지만 확인
	bool IsPathCached(const CString& jsonPath) const;

	// ========================================================================
	// 관리 함수
	// ========================================================================

	// 캐시 전체 삭제 (ConfigManager 설정 변경 시 호출)
	void Clear();

	// 특정 JSONPath만 캐시에서 제거
	void RemovePath(const CString& jsonPath);

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

	// JSONPath 문자열을 파싱하여 토큰 배열 생성
	bool ParseJsonPathToTokens(const CString& jsonPath, std::vector<std::string>& outTokens);

	// JSONPath에서 topic 부분 제거 (topic,jsonpath 형식 처리)
	CString ExtractJsonPathOnly(const CString& fullMapping);

private:
	// ========================================================================
	// 멤버 변수
	// ========================================================================

	// JSONPath 토큰 캐시 (JSONPath 문자열 → 토큰 배열)
	// std::map 사용 이유: C++98 호환 + 정렬 보장
	std::map<CString, std::vector<std::string>> m_pathCache;

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
//   g_jsonPathCache.PreloadAllJsonPaths(tagMappings);
//   g_jsonPathCache.GetCachedTokens(jsonPath, tokens);
// ============================================================================
extern CJsonPathTokenCache g_jsonPathCache;
