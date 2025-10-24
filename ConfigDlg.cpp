#include "pch.h"
#include "EVMQTT.h"
#include "ConfigDlg.h"
#include "ConfigManager.h"
#include "afxdialogex.h"

// CConfigDlg 대화 상자

IMPLEMENT_DYNAMIC(CConfigDlg, CDialogEx)

CConfigDlg::CConfigDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_CONFIG_DIALOG, pParent)
	, m_strMqttIP(_T("127.0.0.1"))
	, m_nMqttPort(1883)
	, m_nMqttKeepAlive(60)
	, m_nParsingInterval(50)
	, m_pEditCtrl(nullptr)
	, m_pInlineEdit(nullptr)
	, m_editItem(-1)
	, m_editSubItem(-1)
	, m_bListModified(false)
	, m_lastSelectedItem(-1)
{
}

CConfigDlg::~CConfigDlg()
{
	if (m_pEditCtrl)
	{
		delete m_pEditCtrl;
		m_pEditCtrl = nullptr;
	}

	if (m_pInlineEdit)
	{
		delete m_pInlineEdit;
		m_pInlineEdit = nullptr;
	}
}

void CConfigDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_MQTT_IP, m_strMqttIP);
	DDX_Text(pDX, IDC_EDIT_MQTT_PORT, m_nMqttPort);
	DDX_Text(pDX, IDC_EDIT_MQTT_KEEPALIVE, m_nMqttKeepAlive);
	DDX_Text(pDX, IDC_EDIT_PARSING_INTERVAL, m_nParsingInterval);
	DDX_Control(pDX, IDC_LIST_TAG_CONFIG, m_listTagMapping);
	DDX_Control(pDX, IDC_STC_TAG_COUNT, m_staticTagCount);
}

BEGIN_MESSAGE_MAP(CConfigDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_ADD_TAG, &CConfigDlg::OnBnClickedBtnAddTag)
	ON_BN_CLICKED(IDC_BTN_DELETE_TAG, &CConfigDlg::OnBnClickedBtnDeleteTag)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_TAG_CONFIG, &CConfigDlg::OnNMDblclkListTagMapping)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST_TAG_CONFIG, &CConfigDlg::OnLvnItemchangedListTagMapping)
	ON_NOTIFY(NM_CLICK, IDC_LIST_TAG_CONFIG, &CConfigDlg::OnNMClickListTagMapping)
	ON_NOTIFY(LVN_ENDLABELEDIT, IDC_LIST_TAG_CONFIG, &CConfigDlg::OnLvnEndlabeleditListTagMapping)
END_MESSAGE_MAP()

// CConfigDlg 메시지 처리기

BOOL CConfigDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	SetWindowText(_T("EVMQTT 설정"));

	// 태그 매핑 리스트 초기화
	InitTagMappingList();

	// 설정 데이터 로드
	LoadConfigData();

	// 태그 매핑 데이터 표시
	UpdateTagMappingList();

	// INI의 태그 개수 표시
	DisplayTagCount();

	return TRUE;
}

BOOL CConfigDlg::PreTranslateMessage(MSG* pMsg)
{
	// 키보드 단축키 처리
	if (pMsg->message == WM_KEYDOWN)
	{
		// 인라인 편집 중이면 별도 처리하지 않음 (CInlineEdit에서 처리)
		if (m_pInlineEdit && m_pInlineEdit->IsWindowVisible())
		{
			return CDialogEx::PreTranslateMessage(pMsg);
		}

		// 리스트 컨트롤에 포커스가 있을 때만 처리
		if (GetFocus() == &m_listTagMapping)
		{
			switch (pMsg->wParam)
			{
			case VK_RETURN:  // 엔터키 - 추가
			{
				// 빈 행이나 완성된 행이 있으면 추가
				AddNewTagRow();
				return TRUE;
			}

			case VK_DELETE:  // Delete키 - 삭제
			{
				DeleteSelectedTag();
				return TRUE;
			}

			case VK_ESCAPE:  // ESC키 - 편집 취소
			{
				if (m_pInlineEdit && m_pInlineEdit->IsWindowVisible())
				{
					EndEditing(false);
					return TRUE;
				}
			}
			break;
			}
		}
	}

	return CDialogEx::PreTranslateMessage(pMsg);
}

void CConfigDlg::OnOK()
{
	// 편집 중이면 편집 종료
	if (m_pInlineEdit && m_pInlineEdit->IsWindowVisible())
	{
		EndEditing(true);
	}

	UpdateData(TRUE);

	if (!ValidateConfig())
	{
		return; // 유효성 검사 실패 시 다이얼로그를 닫지 않음
	}

	// 설정 저장
	SaveConfigData();

	CDialogEx::OnOK();
}

void CConfigDlg::OnCancel()
{
	// 편집 중이면 편집 취소
	if (m_pInlineEdit && m_pInlineEdit->IsWindowVisible())
	{
		EndEditing(false);
	}

	CDialogEx::OnCancel();
}

void CConfigDlg::LoadConfigData()
{
	CConfigManager& configManager = CConfigManager::GetInstance();
	configManager.LoadConfig();

	m_strMqttIP = configManager.GetMqttIp();
	m_nMqttPort = configManager.GetMqttPort();
	m_nMqttKeepAlive = configManager.GetMqttKeepAlive();
	m_nParsingInterval = configManager.GetParsingInterval();

	UpdateData(FALSE);
}

void CConfigDlg::SaveConfigData()
{
	CConfigManager& configManager = CConfigManager::GetInstance();

	configManager.SetMqttIp(m_strMqttIP);
	configManager.SetMqttPort(m_nMqttPort);
	configManager.SetMqttKeepAlive(m_nMqttKeepAlive);
	configManager.SetParsingInterval(m_nParsingInterval);

	configManager.SaveConfig();
}

void CConfigDlg::InitTagMappingList()
{
	// 리스트 컨트롤 스타일 설정
	DWORD dwStyle = m_listTagMapping.GetExtendedStyle();
	m_listTagMapping.SetExtendedStyle(dwStyle | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_LABELTIP);

	// 컬럼 추가
	m_listTagMapping.InsertColumn(0, _T("태그명"), LVCFMT_LEFT, 120);
	m_listTagMapping.InsertColumn(1, _T("토픽"), LVCFMT_LEFT, 100);
	m_listTagMapping.InsertColumn(2, _T("JSONPath"), LVCFMT_LEFT, 150);

	TRACE("태그 매핑 리스트 초기화 완료\n");
}

void CConfigDlg::UpdateTagMappingList()
{
	m_listTagMapping.DeleteAllItems();

	CConfigManager& configManager = CConfigManager::GetInstance();
	std::map<CString, CString> tagMappings = configManager.GetAllTagMappings();

	int index = 0;
	for (const auto& mapping : tagMappings)
	{
		const CString& tagName = mapping.first;
		const CString& tagMapping = mapping.second;

		// 토픽과 JSONPath 분리
		CString topic, jsonPath;
		int commaPos = tagMapping.Find(_T(","));
		if (commaPos > 0)
		{
			topic = tagMapping.Left(commaPos);
			jsonPath = tagMapping.Mid(commaPos + 1);
			topic.Trim();
			jsonPath.Trim();
		}
		else
		{
			topic = _T("+");
			jsonPath = tagMapping;
			jsonPath.Trim();
		}

		AddTagToList(tagName, topic, jsonPath);
		index++;
	}

	// 빈 행 하나 추가 (새 태그 입력용)
	int emptyIndex = m_listTagMapping.InsertItem(m_listTagMapping.GetItemCount(), _T(""));
	m_listTagMapping.SetItemText(emptyIndex, 1, _T(""));
	m_listTagMapping.SetItemText(emptyIndex, 2, _T(""));

	TRACE("태그 매핑 리스트 업데이트 완료: %d개 항목\n", m_listTagMapping.GetItemCount());
}

void CConfigDlg::AddTagToList(const CString& tagName, const CString& topic, const CString& jsonPath)
{
	int index = m_listTagMapping.GetItemCount();
	m_listTagMapping.InsertItem(index, tagName);
	m_listTagMapping.SetItemText(index, 1, topic);
	m_listTagMapping.SetItemText(index, 2, jsonPath);
}

bool CConfigDlg::ValidateConfig()
{
	// MQTT IP 주소 검증
	if (m_strMqttIP.IsEmpty())
	{
		AfxMessageBox(_T("MQTT IP 주소를 입력해주세요."));
		GetDlgItem(IDC_EDIT_MQTT_IP)->SetFocus();
		return false;
	}

	// MQTT 포트 검증
	if (m_nMqttPort < 1 || m_nMqttPort > 65535)
	{
		AfxMessageBox(_T("MQTT 포트는 1~65535 범위여야 합니다."));
		GetDlgItem(IDC_EDIT_MQTT_PORT)->SetFocus();
		return false;
	}

	// Keep Alive 검증
	if (m_nMqttKeepAlive < 10 || m_nMqttKeepAlive > 300)
	{
		AfxMessageBox(_T("Keep Alive는 10~300초 범위여야 합니다."));
		GetDlgItem(IDC_EDIT_MQTT_KEEPALIVE)->SetFocus();
		return false;
	}

	// 파싱 주기 검증
	if (m_nParsingInterval < 10 || m_nParsingInterval > 10000)
	{
		AfxMessageBox(_T("파싱 주기는 10~10000ms 범위여야 합니다."));
		GetDlgItem(IDC_EDIT_PARSING_INTERVAL)->SetFocus();
		return false;
	}

	return true;
}

void CConfigDlg::ShowTagEditDialog(const CString& tagName, const CString& mapping)
{
	// 태그 편집 다이얼로그 표시 (나중에 구현)
	AfxMessageBox(_T("태그 편집 기능은 추후 구현 예정입니다."));
}

void CConfigDlg::OnBnClickedBtnAddTag()
{
	AddNewTagRow();
}

void CConfigDlg::OnBnClickedBtnDeleteTag()
{
	DeleteSelectedTag();
}

void CConfigDlg::OnNMDblclkListTagMapping(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

	if (pNMItemActivate->iItem >= 0 && pNMItemActivate->iSubItem >= 0)
	{
		StartEditingCell(pNMItemActivate->iItem, pNMItemActivate->iSubItem);
	}

	*pResult = 0;
}

void CConfigDlg::OnLvnItemchangedListTagMapping(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);

	// 선택된 항목 추적
	if (pNMLV->uNewState & LVIS_SELECTED)
	{
		m_lastSelectedItem = pNMLV->iItem;
	}

	*pResult = 0;
}

void CConfigDlg::OnNMClickListTagMapping(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

	// 편집 중이면 편집 종료
	if (m_pInlineEdit && m_pInlineEdit->IsWindowVisible())
	{
		EndEditing(true);
	}

	*pResult = 0;
}

void CConfigDlg::OnLvnEndlabeleditListTagMapping(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);

	// 편집 완료 처리는 별도 함수에서 처리
	*pResult = FALSE;  // 기본 처리 방지
}

void CConfigDlg::AddNewTagRow()
{
	TRACE("AddNewTagRow 호출\n");

	// 마지막 빈 행의 데이터 검사
	int itemCount = m_listTagMapping.GetItemCount();
	if (itemCount > 0)
	{
		int lastIndex = itemCount - 1;
		CString tagName, topic, jsonPath;

		if (ValidateTagRow(lastIndex, tagName, topic, jsonPath))
		{
			// 유효한 데이터가 있으면 저장
			SaveTagToConfig(tagName, topic, jsonPath);

			// 새로운 빈 행 추가
			int newIndex = m_listTagMapping.InsertItem(itemCount, _T(""));
			m_listTagMapping.SetItemText(newIndex, 1, _T(""));
			m_listTagMapping.SetItemText(newIndex, 2, _T(""));

			// 새 행 선택
			m_listTagMapping.SetItemState(newIndex, LVIS_SELECTED | LVIS_FOCUSED,
				LVIS_SELECTED | LVIS_FOCUSED);
			m_listTagMapping.EnsureVisible(newIndex, FALSE);

			// 첫 번째 컬럼 편집 시작
			StartEditingCell(newIndex, 0);

			m_bListModified = true;

			TRACE("새 태그 추가됨: %s\n", (LPCTSTR)tagName);
		}
		else
		{
			// 데이터가 불완전하면 첫 번째 빈 셀로 이동하여 편집 시작
			for (int col = 0; col < 3; col++)
			{
				CString cellText = m_listTagMapping.GetItemText(lastIndex, col);
				if (cellText.IsEmpty())
				{
					StartEditingCell(lastIndex, col);
					break;
				}
			}
		}
	}
	else
	{
		// 아무 항목도 없으면 첫 번째 행 추가
		int newIndex = m_listTagMapping.InsertItem(0, _T(""));
		m_listTagMapping.SetItemText(newIndex, 1, _T(""));
		m_listTagMapping.SetItemText(newIndex, 2, _T(""));

		StartEditingCell(newIndex, 0);
	}
}

void CConfigDlg::DeleteSelectedTag()
{
	int selectedItem = m_listTagMapping.GetNextItem(-1, LVNI_SELECTED);

	if (selectedItem >= 0)
	{
		DeleteTagAtIndex(selectedItem);
	}
	else if (m_lastSelectedItem >= 0 && m_lastSelectedItem < m_listTagMapping.GetItemCount())
	{
		// 선택된 항목이 없으면 마지막 선택된 항목 삭제
		DeleteTagAtIndex(m_lastSelectedItem);
	}
	else
	{
		AfxMessageBox(_T("삭제할 태그를 선택해주세요."));
	}
}

void CConfigDlg::DeleteTagAtIndex(int index)
{
	if (index < 0 || index >= m_listTagMapping.GetItemCount())
		return;

	CString tagName = m_listTagMapping.GetItemText(index, 0);

	// 빈 행이면 확인 없이 삭제
	if (tagName.IsEmpty())
	{
		m_listTagMapping.DeleteItem(index);
		TRACE("빈 행 삭제: 인덱스 %d\n", index);
		return;
	}

	// 데이터가 있는 행이면 확인 후 삭제
	CString message;
	message.Format(_T("태그 '%s'를 삭제하시겠습니까?"), tagName);

	if (AfxMessageBox(message, MB_YESNO | MB_ICONQUESTION) == IDYES)
	{
		// INI에서 삭제
		RemoveTagFromConfig(tagName);

		// 리스트에서 삭제
		m_listTagMapping.DeleteItem(index);

		// 빈 행이 없으면 새로 추가
		bool hasEmptyRow = false;
		int itemCount = m_listTagMapping.GetItemCount();
		for (int i = 0; i < itemCount; i++)
		{
			CString checkTag = m_listTagMapping.GetItemText(i, 0);
			CString checkTopic = m_listTagMapping.GetItemText(i, 1);
			CString checkPath = m_listTagMapping.GetItemText(i, 2);

			if (checkTag.IsEmpty() && checkTopic.IsEmpty() && checkPath.IsEmpty())
			{
				hasEmptyRow = true;
				break;
			}
		}

		if (!hasEmptyRow)
		{
			int emptyIndex = m_listTagMapping.InsertItem(itemCount, _T(""));
			m_listTagMapping.SetItemText(emptyIndex, 1, _T(""));
			m_listTagMapping.SetItemText(emptyIndex, 2, _T(""));
		}

		m_bListModified = true;

		TRACE("태그 삭제됨: %s\n", (LPCTSTR)tagName);
	}
}

bool CConfigDlg::ValidateTagRow(int index, CString& tagName, CString& topic, CString& jsonPath)
{
	if (index < 0 || index >= m_listTagMapping.GetItemCount())
		return false;

	tagName = m_listTagMapping.GetItemText(index, 0);
	topic = m_listTagMapping.GetItemText(index, 1);
	jsonPath = m_listTagMapping.GetItemText(index, 2);

	tagName.Trim();
	topic.Trim();
	jsonPath.Trim();

	// 모든 필드가 비어있으면 유효하지 않음
	if (tagName.IsEmpty() && topic.IsEmpty() && jsonPath.IsEmpty())
		return false;

	// 태그명이 비어있으면 유효하지 않음
	if (tagName.IsEmpty())
	{
		AfxMessageBox(_T("태그명을 입력해주세요."));
		return false;
	}

	// JSONPath가 비어있으면 유효하지 않음
	if (jsonPath.IsEmpty())
	{
		AfxMessageBox(_T("JSONPath를 입력해주세요."));
		return false;
	}

	// 토픽이 비어있으면 기본값 설정
	if (topic.IsEmpty())
	{
		topic = _T("+");
		m_listTagMapping.SetItemText(index, 1, topic);
	}

	// 태그명 유효성 검사
	if (!IsValidTagName(tagName))
	{
		AfxMessageBox(_T("유효하지 않은 태그명입니다."));
		return false;
	}

	// JSONPath 유효성 검사
	if (!IsValidJsonPath(jsonPath))
	{
		AfxMessageBox(_T("유효하지 않은 JSONPath입니다."));
		return false;
	}

	return true;
}

bool CConfigDlg::IsValidTagName(const CString& tagName)
{
	if (tagName.IsEmpty() || tagName.GetLength() > 50)
		return false;

	// 특수문자 검사 (알파벳, 숫자, 언더스코어만 허용)
	for (int i = 0; i < tagName.GetLength(); i++)
	{
		TCHAR ch = tagName.GetAt(i);
		if (!(_istalnum(ch) || ch == _T('_')))
		{
			return false;
		}
	}

	return true;
}

bool CConfigDlg::IsValidJsonPath(const CString& jsonPath)
{
	if (jsonPath.IsEmpty())
		return false;

	// 기본적인 JSONPath 형식 검사
	TCHAR firstChar = jsonPath.GetAt(0);
	if (firstChar != _T('$') && firstChar != _T('/'))
		return false;

	return true;
}

void CConfigDlg::SaveTagToConfig(const CString& tagName, const CString& topic, const CString& jsonPath)
{
	CConfigManager& configManager = CConfigManager::GetInstance();

	CString mapping;
	if (topic != _T("+"))
	{
		mapping.Format(_T("%s,%s"), topic, jsonPath);
	}
	else
	{
		mapping = jsonPath;
	}

	// ConfigManager를 통해 태그 매핑 저장
	configManager.SetTagMapping(tagName, mapping);

	TRACE("태그 저장됨: %s -> %s\n", (LPCTSTR)tagName, (LPCTSTR)mapping);
}

void CConfigDlg::RemoveTagFromConfig(const CString& tagName)
{
	CConfigManager& configManager = CConfigManager::GetInstance();

	// ConfigManager를 통해 태그 매핑 삭제
	configManager.RemoveTagMapping(tagName);

	TRACE("태그 삭제됨: %s\n", (LPCTSTR)tagName);
}

void CConfigDlg::StartEditingCell(int item, int subItem)
{
	if (item < 0 || subItem < 0)
		return;

	// 기존 편집 종료
	if (m_pInlineEdit && m_pInlineEdit->IsWindowVisible())
	{
		EndEditing(true);
	}

	// 셀 위치 계산
	CRect rect;
	m_listTagMapping.GetSubItemRect(item, subItem, LVIR_BOUNDS, rect);

	// 컬럼 폭에 맞게 rect 조정
	CRect headerRect;
	CHeaderCtrl* pHeader = m_listTagMapping.GetHeaderCtrl();
	if (pHeader)
	{
		pHeader->GetItemRect(subItem, &headerRect);
		rect.right = rect.left + headerRect.Width() - 2;  // 여백 2픽셀
	}

	// InlineEdit 컨트롤 생성
	if (!m_pInlineEdit)
	{
		m_pInlineEdit = new CInlineEdit(this);
		m_pInlineEdit->Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
			rect, &m_listTagMapping, 1000);
		m_pInlineEdit->SetFont(m_listTagMapping.GetFont());
	}
	else
	{
		m_pInlineEdit->MoveWindow(&rect);
		m_pInlineEdit->ShowWindow(SW_SHOW);
	}

	// 현재 셀의 텍스트로 초기화
	CString cellText = m_listTagMapping.GetItemText(item, subItem);
	m_pInlineEdit->SetWindowText(cellText);
	m_pInlineEdit->SetSel(0, -1);  // 전체 선택
	m_pInlineEdit->SetFocus();

	// 편집 정보 저장
	m_editItem = item;
	m_editSubItem = subItem;

	TRACE("셀 편집 시작: 행=%d, 열=%d, 텍스트='%s'\n", item, subItem, (LPCTSTR)cellText);
}

void CConfigDlg::EndEditing(bool save)
{
	if (!m_pInlineEdit || !m_pInlineEdit->IsWindowVisible())
		return;

	if (save && m_editItem >= 0 && m_editSubItem >= 0)
	{
		CString newText;
		m_pInlineEdit->GetWindowText(newText);
		newText.Trim();

		// 리스트 컨트롤에 새 텍스트 설정
		m_listTagMapping.SetItemText(m_editItem, m_editSubItem, newText);

		TRACE("셀 편집 완료: 행=%d, 열=%d, 새텍스트='%s'\n",
			m_editItem, m_editSubItem, (LPCTSTR)newText);

		// 모든 필드가 채워진 경우에만 자동 저장 및 유효성 검사
		CString tagName = m_listTagMapping.GetItemText(m_editItem, 0).Trim();
		CString topic = m_listTagMapping.GetItemText(m_editItem, 1).Trim();
		CString jsonPath = m_listTagMapping.GetItemText(m_editItem, 2).Trim();

		if (!tagName.IsEmpty() && !jsonPath.IsEmpty())
		{
			// 토픽이 비어있으면 기본값 설정
			if (topic.IsEmpty())
			{
				topic = _T("+");
				m_listTagMapping.SetItemText(m_editItem, 1, topic);
			}

			// 유효성 검사
			if (IsValidTagName(tagName) && IsValidJsonPath(jsonPath))
			{
				// 기존 태그인지 확인 (첫 번째 컬럼 변경 시)
				if (m_editSubItem == 0)
				{
					CConfigManager& configManager = CConfigManager::GetInstance();
					std::map<CString, CString> existingMappings = configManager.GetAllTagMappings();

					if (existingMappings.find(tagName) != existingMappings.end())
					{
						CString message;
						message.Format(_T("태그 '%s'가 이미 존재합니다.\n덮어쓰시겠습니까?"), tagName);

						if (AfxMessageBox(message, MB_YESNO | MB_ICONQUESTION) != IDYES)
						{
							// 원래 값으로 복원
							m_listTagMapping.SetItemText(m_editItem, m_editSubItem, _T(""));
							m_pInlineEdit->ShowWindow(SW_HIDE);
							m_editItem = -1;
							m_editSubItem = -1;
							return;
						}
					}
				}

				// 유효한 태그이면 저장
				SaveTagToConfig(tagName, topic, jsonPath);
				m_bListModified = true;
			}
			else
			{
				// 유효성 검사 실패 시 경고 메시지만 표시 (다이얼로그 종료하지 않음)
				if (!IsValidTagName(tagName))
				{
					AfxMessageBox(_T("유효하지 않은 태그명입니다. (영문자, 숫자, 언더스코어만 허용)"));
				}
				else if (!IsValidJsonPath(jsonPath))
				{
					AfxMessageBox(_T("유효하지 않은 JSONPath입니다. ($나 /로 시작해야 합니다)"));
				}
			}
		}
	}

	// 편집 컨트롤 숨기기
	m_pInlineEdit->ShowWindow(SW_HIDE);
	m_editItem = -1;
	m_editSubItem = -1;

	// 포커스를 리스트로 복원
	m_listTagMapping.SetFocus();
}

// CInlineEdit 클래스의 PreTranslateMessage 구현
BOOL CConfigDlg::CInlineEdit::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		switch (pMsg->wParam)
		{
		case VK_RETURN:  // 엔터키 - 다음 셀로 이동 또는 편집 종료
		{
			// 현재 편집 저장
			m_pParent->EndEditing(true);

			// 다음 셀로 이동
			int nextSubItem = m_pParent->m_editSubItem + 1;
			if (nextSubItem <= 2)  // 컬럼 0, 1, 2만 있음
			{
				m_pParent->StartEditingCell(m_pParent->m_editItem, nextSubItem);
			}
			else
			{
				// 마지막 컬럼이면 다음 행의 첫 번째 컬럼으로
				int nextItem = m_pParent->m_editItem + 1;
				if (nextItem < m_pParent->m_listTagMapping.GetItemCount())
				{
					m_pParent->StartEditingCell(nextItem, 0);
				}
				else
				{
					// 새 행 추가
					m_pParent->AddNewTagRow();
				}
			}
			return TRUE;
		}

		case VK_ESCAPE:  // ESC키 - 편집 취소
		{
			m_pParent->EndEditing(false);
			return TRUE;
		}

		case VK_TAB:  // 탭키 - 다음/이전 셀로 이동
		{
			m_pParent->EndEditing(true);

			bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
			int nextSubItem = isShift ? (m_pParent->m_editSubItem - 1) : (m_pParent->m_editSubItem + 1);

			if (nextSubItem >= 0 && nextSubItem <= 2)
			{
				m_pParent->StartEditingCell(m_pParent->m_editItem, nextSubItem);
			}

			return TRUE;
		}
		}
	}

	return CEdit::PreTranslateMessage(pMsg);
}

void CConfigDlg::DisplayTagCount()
{
	CConfigManager& configManager = CConfigManager::GetInstance();
	std::map<CString, CString> tagMappings = configManager.GetAllTagMappings();
	
	int tagCount = static_cast<int>(tagMappings.size());
	
	CString countText;
	countText.Format(_T("총 등록된 태그: %d개"), tagCount);
	
	m_staticTagCount.SetWindowText(countText);
	
	TRACE("태그 개수 표시: %d개\n", tagCount);
}
