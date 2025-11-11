#pragma once

#include <vector>
#include <map>
#include <xlnt/xlnt.hpp>

// ===== TagConfigEntry 구조체 =====
// XLSX 파일의 각 행을 표현하는 구조체
struct TagConfigEntry {
    // ===== XLSX에서 읽는 필드 =====
    CString function;      // "sub" 또는 "pub"
    CString tagName;       // EasyView 태그명 (예: AI_Flow1)
    CString topic;         // MQTT 토픽 (예: device/sensor1)
    CString jsonPath;      // JSON 경로 (예: /data)
    double scale;          // 배율 (0.1, 1.0, 10.0 등)
    CString comment;       // 비고

    // ===== 런타임 캐싱 필드 (자동 채워짐) =====
    int nStnPos;           // EasyView Station 번호
    int nTagPos;           // 태그 Position
    int nSBOffset;         // Scanbuffer 오프셋
    int nTagType;          // 태그 타입 (AI, DI 등)
    bool bCached;          // 캐시 완료 여부

    TagConfigEntry()
        : scale(1.0),
          nStnPos(-1), nTagPos(-1),
          nSBOffset(-1), nTagType(-1),
          bCached(false)
    {}
};

// ===== XLSX 설정 관리 클래스 =====
class CXlsxConfigManager {
public:
    CXlsxConfigManager();
    ~CXlsxConfigManager();

    // XLSX 파일 로드
    bool LoadFromXlsx(const CString& xlsxPath);

    // 조회 함수
    std::vector<TagConfigEntry*> GetConfigsByTopic(const CString& topic);
    TagConfigEntry* GetConfigByTagName(const CString& tagName);

    // 통계 함수
    int GetSubConfigCount() const { return (int)m_subConfigs.size(); }
    int GetPubConfigCount() const { return (int)m_pubConfigs.size(); }
    const std::vector<TagConfigEntry>& GetSubConfigs() const { return m_subConfigs; }
    const std::vector<TagConfigEntry>& GetPubConfigs() const { return m_pubConfigs; }

    // 초기화
    void Clear();

private:
    // XLSX 시트별 로드 함수
    bool LoadSubTagSheet(xlnt::workbook& wb);
    bool LoadPubTagSheet(xlnt::workbook& wb);

    // 인덱스 구축
    void BuildIndexes();

    // 유틸리티 함수
    CString GetCellString(xlnt::worksheet& ws, int row, int col) const;
    double GetCellDouble(xlnt::worksheet& ws, int row, int col) const;

private:
    // 설정 데이터
    std::vector<TagConfigEntry> m_subConfigs;   // Subscribe 설정
    std::vector<TagConfigEntry> m_pubConfigs;   // Publish 설정

    // 빠른 검색용 인덱스
    std::multimap<CString, TagConfigEntry*> m_topicIndex;    // topic → configs (1:N 매핑)
    std::map<CString, TagConfigEntry*> m_tagNameIndex;       // tagName → config (1:1 매핑)
};

// 전역 인스턴스 선언
extern CXlsxConfigManager g_xlsxConfig;
