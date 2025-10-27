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
	virtual BOOL PreTranslateMessage(MSG* pMsg);        // 키보드 단축키 처리

	DECLARE_MESSAGE_MAP()

public:
	// MQTT 설정 변수
	CString m_strMqttIP;
	int m_nMqttPort;
	int m_nMqttKeepAlive;
	int m_nParsingInterval;

	// Device 설정 변수
	CComboBox m_comboDeviceType;
	CString m_strDeviceType;

	// 태그 매핑 리스트
	CListCtrl m_listTagMapping;

	// 초기화 및 종료
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void OnCancel();

	// 버튼 핸들러
	afx_msg void OnBnClickedBtnAddTag();
	afx_msg void OnBnClickedBtnDeleteTag();
	afx_msg void OnNMDblclkListTagMapping(NMHDR* pNMHDR, LRESULT* pResult);

	// 리스트 컨트롤 이벤트 핸들러
	afx_msg void OnLvnItemchangedListTagMapping(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnNMClickListTagMapping(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLvnEndlabeleditListTagMapping(NMHDR* pNMHDR, LRESULT* pResult);
	
	// Device Type 콤보박스 이벤트 핸들러
	afx_msg void OnCbnSelchangeComboDeviceType();

private:
	// 내부 함수
	void LoadConfigData();
	void SaveConfigData();
	void InitTagMappingList();
	void UpdateTagMappingList();
	void AddTagToList(const CString& tagName, const CString& topic, const CString& jsonPath);
	bool ValidateConfig();
	void ShowTagEditDialog(const CString& tagName = _T(""), const CString& mapping = _T(""));

	CStatic m_staticTagCount;
	void DisplayTagCount();

	void AddNewTagRow();
	void DeleteSelectedTag();
	void DeleteTagAtIndex(int index);
	bool ValidateTagRow(int index, CString& tagName, CString& topic, CString& jsonPath);
	bool IsValidTagName(const CString& tagName);
	bool IsValidJsonPath(const CString& jsonPath);
	void SaveTagToConfig(const CString& tagName, const CString& topic, const CString& jsonPath);
	void RemoveTagFromConfig(const CString& tagName);
	void StartEditingCell(int item, int subItem);

	// 인라인 편집 관련
	CEdit* m_pEditCtrl;
	int m_editItem;
	int m_editSubItem;
	void EndEditing(bool save);

	// 커스텀 Edit 컨트롤 클래스
	class CInlineEdit : public CEdit
	{
	public:
		CInlineEdit(CConfigDlg* pParent) : m_pParent(pParent) {}

	protected:
		virtual BOOL PreTranslateMessage(MSG* pMsg);
		CConfigDlg* m_pParent;
	};

	CInlineEdit* m_pInlineEdit;

	// 상태 변수
	bool m_bListModified;  // 리스트가 수정되었는지 추적
	int m_lastSelectedItem; // 마지막 선택된 항목
};
