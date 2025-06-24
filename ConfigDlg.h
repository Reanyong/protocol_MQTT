#pragma once

#include "afxdialogex.h"
#include "resource.h"

// CConfigDlg 대화 상자
class CConfigDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CConfigDlg)

public:
	CConfigDlg(CWnd* pParent = nullptr);   // 표준 생성자입니다.
	virtual ~CConfigDlg();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CONFIG_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

	DECLARE_MESSAGE_MAP()

public:
	// MQTT 설정 변수
	CString m_strMqttIP;
	int m_nMqttPort;
	int m_nMqttKeepAlive;
	int m_nParsingInterval;

	// 태그 매핑 리스트
	CListCtrl m_listTagMapping;

	// 초기화 및 종료
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void OnCancel();

	// 버튼 핸들러
	afx_msg void OnBnClickedBtnAddTag();
	afx_msg void OnBnClickedBtnEditTag();
	afx_msg void OnBnClickedBtnDeleteTag();
	afx_msg void OnNMDblclkListTagMapping(NMHDR* pNMHDR, LRESULT* pResult);

private:
	// 내부 함수
	void LoadConfigData();
	void SaveConfigData();
	void InitTagMappingList();
	void UpdateTagMappingList();
	void AddTagToList(const CString& tagName, const CString& topic, const CString& jsonPath);
	bool ValidateConfig();
	void ShowTagEditDialog(const CString& tagName = _T(""), const CString& mapping = _T(""));
};

// 태그 편집 다이얼로그
class CTagEditDlg : public CDialogEx
{
public:
	CTagEditDlg(CWnd* pParent = nullptr);

	enum { IDD = IDD_ABOUTBOX }; // 임시로 사용, 나중에 별도 다이얼로그 생성

	CString m_strTagName;
	CString m_strTopic;
	CString m_strJsonPath;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();
	virtual void OnOK();

	DECLARE_MESSAGE_MAP()
}; 
