
#pragma once

#ifndef __AFXWIN_H__
	#error "PCH에 대해 이 파일을 포함하기 전에 'pch.h'를 포함합니다."
#endif

#include "resource.h"		// 주 기호입니다.

class CEVMQTTApp : public CWinApp
{
public:
	CEVMQTTApp();

// 재정의입니다.
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// 구현입니다.
private:
	HANDLE m_hMutex;

	DECLARE_MESSAGE_MAP()
};

extern CEVMQTTApp theApp;

// ===== EasyView 프로젝트 이름 전역 변수 =====
extern CString g_szProjectName;
