#include "pch.h"
#include "XlsxConfigManager.h"
#include <atlstr.h>

// 전역 인스턴스 정의
CXlsxConfigManager g_xlsxConfig;

CXlsxConfigManager::CXlsxConfigManager()
{
}

CXlsxConfigManager::~CXlsxConfigManager()
{
    Clear();
}

// XLSX 파일 로드 (메인 진입점)
bool CXlsxConfigManager::LoadFromXlsx(const CString& xlsxPath)
{
    Clear();

    bool loadSuccess = false;
    
    try {
        TRACE(_T("=== XLSX 로드 시작 ===\n"));
        TRACE(_T("파일 경로: %s\n"), (LPCTSTR)xlsxPath);
        
        // xlnt::workbook을 별도 scope에서 생성/소멸 (메모리 릭 방지)
        {
            // xlnt는 std::string을 사용하므로 변환 필요
            std::string pathStr = CT2A(xlsxPath, CP_UTF8);
            xlnt::workbook wb;
            wb.load(pathStr);

            TRACE(_T("시트 개수: %d\n"), (int)wb.sheet_count());

            // Sheet1: SubTagMapping
            if (!LoadSubTagSheet(wb)) {
                TRACE(_T("ERROR: SubTagMapping 시트 로드 실패\n"));
                return false;
            }

            // Sheet2: PubTagMapping
            if (!LoadPubTagSheet(wb)) {
                TRACE(_T("ERROR: PubTagMapping 시트 로드 실패\n"));
                return false;
            }
            
            loadSuccess = true;
            
            // wb는 여기서 자동 소멸됨 (scope 종료)
            TRACE(_T("xlnt::workbook 소멸 완료\n"));
        }

        if (loadSuccess) {
            // vector 메모리 재할당 방지 (중요!)
            m_subConfigs.shrink_to_fit();
            m_pubConfigs.shrink_to_fit();

            // 인덱스 구축 (빠른 검색을 위해)
            BuildIndexes();

            TRACE(_T("=== XLSX 로드 완료 ===\n"));
            TRACE(_T("Subscribe 설정: %d개\n"), GetSubConfigCount());
            TRACE(_T("Publish 설정: %d개\n"), GetPubConfigCount());
        }

        return loadSuccess;
    }
    catch (const std::exception& e) {
        CString msg = CA2T(e.what());
        TRACE(_T("XLSX 로드 예외: %s\n"), (LPCTSTR)msg);
        Clear();  // 예외 발생 시 명시적 정리
        return false;
    }
    catch (...) {
        TRACE(_T("XLSX 로드 알 수 없는 예외\n"));
        Clear();  // 예외 발생 시 명시적 정리
        return false;
    }
}

// Sheet1: SubTagMapping 로드
bool CXlsxConfigManager::LoadSubTagSheet(xlnt::workbook& wb)
{
    try {
        // 시트 이름으로 찾기 (Sheet1 또는 SubTagMapping)
        xlnt::worksheet ws;
        if (wb.contains("SubTagMapping")) {
            ws = wb.sheet_by_title("SubTagMapping");
        }
        else if (wb.contains("Sheet1")) {
            ws = wb.sheet_by_title("Sheet1");
        }
        else {
            TRACE(_T("ERROR: SubTagMapping 또는 Sheet1 시트를 찾을 수 없습니다\n"));
            return false;
        }

        // 헤더 행(1행) 건너뛰고 2행부터 읽기
        int row = 2;
        while (ws.cell(xlnt::column_t(1), row).has_value()) {
            TagConfigEntry entry;

            // 각 열 읽기 (1부터 시작)
            // | 기능명 | 태그명 | 토픽명 | JSONPath | 배율 | 비고 |
            //    1       2        3        4          5      6
            entry.function = GetCellString(ws, row, 1);   // 기능명
            entry.tagName = GetCellString(ws, row, 2);    // 태그명
            entry.topic = GetCellString(ws, row, 3);      // 토픽명
            entry.jsonPath = GetCellString(ws, row, 4);   // JSONPath
            entry.scale = GetCellDouble(ws, row, 5);      // 배율
            entry.comment = GetCellString(ws, row, 6);    // 비고

            // 유효성 검사
            if (entry.tagName.IsEmpty() || entry.topic.IsEmpty()) {
                TRACE(_T("WARNING: SubTagMapping %d행 스킵 (태그명 또는 토픽명 없음)\n"), row);
                row++;
                continue;
            }

            m_subConfigs.push_back(entry);
            row++;
        }

        TRACE(_T("SubTagMapping: %d개 항목 로드\n"), (int)m_subConfigs.size());
        return true;
    }
    catch (const std::exception& e) {
        CString msg = CA2T(e.what());
        TRACE(_T("SubTagMapping 로드 예외: %s\n"), (LPCTSTR)msg);
        return false;
    }
}

// Sheet2: PubTagMapping 로드
bool CXlsxConfigManager::LoadPubTagSheet(xlnt::workbook& wb)
{
    try {
        // 시트 이름으로 찾기 (Sheet2 또는 PubTagMapping)
        xlnt::worksheet ws;
        if (wb.contains("PubTagMapping")) {
            ws = wb.sheet_by_title("PubTagMapping");
        }
        else if (wb.contains("Sheet2")) {
            ws = wb.sheet_by_title("Sheet2");
        }
        else {
            // Publish 시트는 선택사항 (없어도 됨)
            TRACE(_T("INFO: PubTagMapping 시트 없음 (선택사항)\n"));
            return true;
        }

        // 헤더 행(1행) 건너뛰고 2행부터 읽기
        int row = 2;
        while (ws.cell(xlnt::column_t(1), row).has_value()) {
            TagConfigEntry entry;

            // 각 열 읽기 (Publish는 배율 없음)
            // | 기능명 | 태그명 | 토픽명 | JSON구조 | 비고 |
            //    1       2        3        4         5
            entry.function = GetCellString(ws, row, 1);
            entry.tagName = GetCellString(ws, row, 2);
            entry.topic = GetCellString(ws, row, 3);
            entry.jsonPath = GetCellString(ws, row, 4); // Publish에서는 JSON 구조
            entry.scale = 1.0;                          // Publish는 배율 사용 안 함 (고정값)
            entry.comment = GetCellString(ws, row, 5);  // 5번 컬럼 (배율 컬럼 제거)

            // 유효성 검사
            if (entry.tagName.IsEmpty() || entry.topic.IsEmpty()) {
                TRACE(_T("WARNING: PubTagMapping %d행 스킵\n"), row);
                row++;
                continue;
            }

            m_pubConfigs.push_back(entry);
            row++;
        }

        TRACE(_T("PubTagMapping: %d개 항목 로드\n"), (int)m_pubConfigs.size());
        return true;
    }
    catch (const std::exception& e) {
        CString msg = CA2T(e.what());
        TRACE(_T("PubTagMapping 로드 예외: %s\n"), (LPCTSTR)msg);
        return false;
    }
}

// 인덱스 구축 (빠른 검색을 위해)
void CXlsxConfigManager::BuildIndexes()
{
    m_topicIndex.clear();
    m_tagNameIndex.clear();

    // Subscribe 설정 인덱싱
    for (auto& config : m_subConfigs) {
        // topic → config (1:N 매핑, multimap 사용)
        m_topicIndex.insert(std::make_pair(config.topic, &config));

        // tagName → config (1:1 매핑)
        m_tagNameIndex[config.tagName] = &config;
    }

    // Publish 설정 인덱싱
    for (auto& config : m_pubConfigs) {
        m_topicIndex.insert(std::make_pair(config.topic, &config));
        m_tagNameIndex[config.tagName] = &config;
    }

    TRACE(_T("인덱스 구축 완료: Topic=%d, TagName=%d\n"),
        (int)m_topicIndex.size(), (int)m_tagNameIndex.size());
}

// 토픽으로 설정 조회 (여러 개 반환 가능)
std::vector<TagConfigEntry*> CXlsxConfigManager::GetConfigsByTopic(const CString& topic)
{
    std::vector<TagConfigEntry*> results;

    auto range = m_topicIndex.equal_range(topic);
    for (auto it = range.first; it != range.second; ++it) {
        results.push_back(it->second);
    }

    return results;
}

// 태그명으로 설정 조회 (1개 반환)
TagConfigEntry* CXlsxConfigManager::GetConfigByTagName(const CString& tagName)
{
    auto it = m_tagNameIndex.find(tagName);
    if (it != m_tagNameIndex.end()) {
        return it->second;
    }
    return nullptr;
}

// 초기화
void CXlsxConfigManager::Clear()
{
    TRACE(_T("=== XlsxConfigManager::Clear() 시작 ===\n"));
    
    // 1. 포인터 인덱스를 먼저 정리 (dangling pointer 방지)
    size_t topicIndexSize = m_topicIndex.size();
    size_t tagNameIndexSize = m_tagNameIndex.size();
    
    m_topicIndex.clear();
    m_tagNameIndex.clear();
    
    TRACE(_T("인덱스 정리: Topic=%d, TagName=%d\n"), 
        topicIndexSize, tagNameIndexSize);
    
    // 2. 그 다음 실제 데이터 정리
    size_t subSize = m_subConfigs.size();
    size_t pubSize = m_pubConfigs.size();
    
    m_subConfigs.clear();
    m_pubConfigs.clear();
    
    // 3. 메모리 확실히 해제 (vector capacity 축소)
    m_subConfigs.shrink_to_fit();
    m_pubConfigs.shrink_to_fit();
    
    TRACE(_T("데이터 정리: Sub=%d, Pub=%d\n"), subSize, pubSize);
    TRACE(_T("=== XlsxConfigManager::Clear() 완료 ===\n"));
}

// 유틸리티: 셀에서 문자열 읽기
CString CXlsxConfigManager::GetCellString(xlnt::worksheet& ws, int row, int col) const
{
    try {
        auto cell = ws.cell(xlnt::column_t(col), row);
        if (!cell.has_value()) {
            return _T("");
        }

        // xlnt에서 UTF-8 std::string 가져오기
        std::string str = cell.to_string();
        
        if (str.empty()) {
            return _T("");
        }

        // ===== 멀티바이트 프로젝트 대응: UTF-8 → 유니코드 → MBCS =====
        // 1단계: UTF-8 → 유니코드(wchar_t)
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
        if (wideLen <= 0) {
            return _T("");
        }

        wchar_t* wideBuf = new wchar_t[wideLen];
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wideBuf, wideLen);

        // 2단계: 유니코드 → CString (CString이 자동으로 MBCS 변환)
        CString result(wideBuf);

        // 메모리 정리
        delete[] wideBuf;
        str.clear();
        str.shrink_to_fit();

        return result;
    }
    catch (...) {
        return _T("");
    }
}

// 유틸리티: 셀에서 double 읽기
double CXlsxConfigManager::GetCellDouble(xlnt::worksheet& ws, int row, int col) const
{
    try {
        auto cell = ws.cell(xlnt::column_t(col), row);
        if (!cell.has_value()) {
            return 1.0;  // 기본값
        }

        // xlnt는 숫자를 double로 반환
        if (cell.data_type() == xlnt::cell::type::number) {
            return cell.value<double>();
        }
        else if (cell.data_type() == xlnt::cell::type::shared_string ||
                 cell.data_type() == xlnt::cell::type::inline_string) {
            // 문자열인 경우 파싱 시도
            std::string str = cell.to_string();
            return atof(str.c_str());
        }

        return 1.0;  // 기본값
    }
    catch (...) {
        return 1.0;  // 기본값
    }
}
