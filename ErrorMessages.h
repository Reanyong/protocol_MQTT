// ErrorMessages.h
#pragma once

// JSON 구문 오류 메시지
#define ERROR_MSG_JSON_PARSE      _T("JSON 구문 분석 오류")
#define ERROR_MSG_INVALID_FORMAT  _T("잘못된 JSON 형식")

// 필수 필드 누락 메시지
#define ERROR_MSG_MISSING_CODE    _T("필수 필드 'code'가 없습니다")
#define ERROR_MSG_MISSING_CID     _T("필수 필드 'cid'가 없습니다")
#define ERROR_MSG_MISSING_ADR     _T("필수 필드 'adr'이 없습니다")
#define ERROR_MSG_MISSING_DATA    _T("필수 필드 'data'가 없습니다")
#define ERROR_MSG_MISSING_EVENTNO _T("필수 필드 'eventno'가 없습니다")
#define ERROR_MSG_MISSING_SRCURL  _T("필수 필드 'srcurl'이 없습니다")

// 타입 오류 메시지
#define ERROR_MSG_CID_TYPE        _T("'cid' 필드가 숫자 형식이어야 합니다")
#define ERROR_MSG_DATA_TYPE       _T("'data' 필드가 객체 형식이어야 합니다")
#define ERROR_MSG_PAYLOAD_TYPE    _T("'payload' 필드가 객체 형식이어야 합니다")

// 구조체 필드 오류 메시지
#define ERROR_MSG_TIMER_FIELDS    _T("타이머 카운터에 필수 필드가 없습니다")
#define ERROR_MSG_TEMP_FIELDS     _T("온도 데이터에 필수 필드가 없습니다")
#define ERROR_MSG_IOLINK_FIELDS   _T("IOLink 데이터에 필수 필드가 없습니다")

// 필드 타입 오류 메시지
#define ERROR_MSG_TIMER_TYPE      _T("타이머 카운터 필드가 숫자 형식이 아닙니다")
#define ERROR_MSG_TEMP_TYPE       _T("온도 데이터 필드가 숫자 형식이 아닙니다")
#define ERROR_MSG_IOLINK_CODE_TYPE _T("IOLink 'code' 필드가 숫자 형식이 아닙니다")
#define ERROR_MSG_IOLINK_DATA_TYPE _T("IOLink 'data' 필드가 올바른 형식이 아닙니다")
