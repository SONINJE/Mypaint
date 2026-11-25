#pragma once

/**
 * @file SizeDlg.h
 * @brief 이미지 크기 조정 / 캔버스 크기 입력 대화 상자
 */

/**
 * @brief 너비와 높이를 입력받습니다. 비율 유지 옵션이 있습니다.
 */
class CSizeDlg : public CDialog
{
public:
    CSizeDlg(const CString& title, int width, int height, bool keepRatio, CWnd* parent = nullptr);

    int Width() const { return m_width; }
    int Height() const { return m_height; }

protected:
    BOOL OnInitDialog() override;
    void OnOK() override;
    afx_msg void OnWidthChange();
    afx_msg void OnHeightChange();
    DECLARE_MESSAGE_MAP()

private:
    bool KeepRatio() const { return IsDlgButtonChecked(IDC_SIZE_KEEP_RATIO) == BST_CHECKED; }

    CString m_title;
    int m_originalWidth;
    int m_originalHeight;
    int m_width;
    int m_height;
    bool m_keepRatio;
    bool m_updating = false;
};
