#include "pch.h"
#include "EVMQTT.h"
#include "ConfigDlg.h"
#include "ConfigManager.h"
#include "afxdialogex.h"

// CConfigDlg 대화 상자

IMPLEMENT_DYNAMIC(CConfigDlg, CDialogEx)

CConfigDlg::CConfigDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_CONFIG_DIALOG, pParent)
	, m_strMqttIP(_T(""))
	, m_nMqttPort(1883)
	, m_nMqttKeepAlive(60)
	, m_nParsingInterval(50)
{
}

CConfigDlg::~CConfigDlg()
{
}

void CConfigDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_MQTT_IP, m_strMqttIP);
	DDX_Text(pDX, IDC_EDIT_MQTT_PORT, m_nMqttPort);
	DDX_Text(pDX, IDC_EDIT_MQTT_KEEPALIVE, m_nMqttKeepAlive);
	DDX_Text(pDX, IDC_EDIT_PARSING_INTERVAL, m_nParsingInterval);
	DDX_Control(pDX, IDC_LIST_TAG_MAPPING, m_listTagMapping);
}

BEGIN_MESSAGE_MAP(CConfigDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_ADD_TAG, &CConfigDlg::OnBnClickedBtnAddTag)
	ON_BN_CLICKED(IDC_BTN_EDIT_TAG, &CConfigDlg::OnBnClickedBtnEditTag)
	ON_BN_CLICKED(IDC_BTN_DELETE_TAG, &CConfigDlg::OnBnClickedBtnDeleteTag)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_TAG_MAPPING, &CConfigDlg::OnNMDblclkListTagMapping)
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

	return TRUE;
}

void CConfigDlg::OnOK()
{
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
	m_listTagMapping.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	// 컬럼 추가
	m_listTagMapping.InsertColumn(0, _T("태그명"), LVCFMT_LEFT, 120);
	m_listTagMapping.InsertColumn(1, _T("토픽"), LVCFMT_LEFT, 100);
	m_listTagMapping.InsertColumn(2, _T("JSONPath"), LVCFMT_LEFT, 300);
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
	ShowTagEditDialog();
}

void CConfigDlg::OnBnClickedBtnEditTag()
{
	int selectedItem = m_listTagMapping.GetNextItem(-1, LVNI_SELECTED);
	if (selectedItem >= 0)
	{
		CString tagName = m_listTagMapping.GetItemText(selectedItem, 0);
		CString topic = m_listTagMapping.GetItemText(selectedItem, 1);
		CString jsonPath = m_listTagMapping.GetItemText(selectedItem, 2);
		
		CString mapping;
		if (topic != _T("+"))
		{
			mapping.Format(_T("%s,%s"), topic, jsonPath);
		}
		else
		{
			mapping = jsonPath;
		}
		
		ShowTagEditDialog(tagName, mapping);
	}
	else
	{
		AfxMessageBox(_T("편집할 태그를 선택해주세요."));
	}
}

void CConfigDlg::OnBnClickedBtnDeleteTag()
{
	int selectedItem = m_listTagMapping.GetNextItem(-1, LVNI_SELECTED);
	if (selectedItem >= 0)
	{
		CString tagName = m_listTagMapping.GetItemText(selectedItem, 0);
		
		CString message;
		message.Format(_T("태그 '%s'를 삭제하시겠습니까?"), tagName);
		
		if (AfxMessageBox(message, MB_YESNO | MB_ICONQUESTION) == IDYES)
		{
			CConfigManager& configManager = CConfigManager::GetInstance();
			configManager.RemoveTagMapping(tagName);
			configManager.SaveTagMappings();
			
			UpdateTagMappingList();
		}
	}
	else
	{
		AfxMessageBox(_T("삭제할 태그를 선택해주세요."));
	}
}

void CConfigDlg::OnNMDblclkListTagMapping(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	
	if (pNMItemActivate->iItem >= 0)
	{
		OnBnClickedBtnEditTag();
	}
	
	*pResult = 0;
} 
