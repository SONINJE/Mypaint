/**
 * @file SizeDlg.cpp
 * @brief CSizeDlg 구현
 */
#include "pch.h"
#include "Resource.h"
#include "SizeDlg.h"
#include "Dib.h"

BEGIN_MESSAGE_MAP(CSizeDlg, CDialog)
    ON_EN_CHANGE(IDC_SIZE_WIDTH, &CSizeDlg::OnWidthChange)
    ON_EN_CHANGE(IDC_SIZE_HEIGHT, &CSizeDlg::OnHeightChange)
END_MESSAGE_MAP()

CSizeDlg::CSizeDlg(const CString& title, int width, int height, bool keepRatio, CWnd* parent)
    : CDialog(IDD_SIZE_DIALOG, parent), m_title(title), m_originalWidth(width), m_originalHeight(height),
      m_width(width), m_height(height), m_keepRatio(keepRatio)
{
}

BOOL CSizeDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // 리소스 파일은 ASCII로 유지하고, 한글 문구는 여기서 지정합니다.
    SetWindowText(m_title);
    SetDlgItemText(IDC_SIZE_WIDTH_LABEL, L"너비");
    SetDlgItemText(IDC_SIZE_HEIGHT_LABEL, L"높이");
    SetDlgItemText(IDC_SIZE_KEEP_RATIO, L"가로세로 비율 유지");
    SetDlgItemText(IDOK, L"확인");
    SetDlgItemText(IDCANCEL, L"취소");

    m_updating = true;
    SetDlgItemInt(IDC_SIZE_WIDTH, static_cast<UINT>(m_width), FALSE);
    SetDlgItemInt(IDC_SIZE_HEIGHT, static_cast<UINT>(m_height), FALSE);
    m_updating = false;
    CheckDlgButton(IDC_SIZE_KEEP_RATIO, m_keepRatio ? BST_CHECKED : BST_UNCHECKED);

    if (CEdit* edit = static_cast<CEdit*>(GetDlgItem(IDC_SIZE_WIDTH)))
    {
        edit->SetFocus();
        edit->SetSel(0, -1);
    }
    return FALSE; // 포커스를 직접 지정함
}

void CSizeDlg::OnWidthChange()
{
    if (m_updating || !KeepRatio())
        return;
    BOOL ok = FALSE;
    const int w = static_cast<int>(GetDlgItemInt(IDC_SIZE_WIDTH, &ok, FALSE));
    if (!ok || w <= 0)
        return;
    const int h = (std::max)(1, static_cast<int>(std::lround(double(w) * m_originalHeight / m_originalWidth)));
    m_updating = true;
    SetDlgItemInt(IDC_SIZE_HEIGHT, static_cast<UINT>(h), FALSE);
    m_updating = false;
}

void CSizeDlg::OnHeightChange()
{
    if (m_updating || !KeepRatio())
        return;
    BOOL ok = FALSE;
    const int h = static_cast<int>(GetDlgItemInt(IDC_SIZE_HEIGHT, &ok, FALSE));
    if (!ok || h <= 0)
        return;
    const int w = (std::max)(1, static_cast<int>(std::lround(double(h) * m_originalWidth / m_originalHeight)));
    m_updating = true;
    SetDlgItemInt(IDC_SIZE_WIDTH, static_cast<UINT>(w), FALSE);
    m_updating = false;
}

void CSizeDlg::OnOK()
{
    BOOL okW = FALSE;
    BOOL okH = FALSE;
    const int w = static_cast<int>(GetDlgItemInt(IDC_SIZE_WIDTH, &okW, FALSE));
    const int h = static_cast<int>(GetDlgItemInt(IDC_SIZE_HEIGHT, &okH, FALSE));
    if (!okW || !okH || w < 1 || h < 1 || w > CDib::kMaxSide || h > CDib::kMaxSide ||
        static_cast<long long>(w) * h > CDib::kMaxPixels)
    {
        CString message;
        message.Format(L"너비와 높이는 1~%d 사이여야 하고,\n전체 픽셀 수는 %lld 이하여야 합니다.", CDib::kMaxSide,
                       CDib::kMaxPixels);
        MessageBox(message, L"MyPaint", MB_ICONWARNING);
        return;
    }
    m_width = w;
    m_height = h;
    CDialog::OnOK();
}
