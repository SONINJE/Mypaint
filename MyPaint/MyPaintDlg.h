#pragma once

/**
 * @file MyPaintDlg.h
 * @brief 주 대화 상자 클래스 선언
 */

#include <array>
#include "Canvas.h"
#include "CanvasView.h"
#include "ColorBar.h"
#include "Tools.h"

/**
 * @brief 주 대화 상자. 메뉴, 도구 모음, 캔버스 뷰, 상태 표시줄을 배치하고 명령을 처리합니다.
 */
class CMyPaintDlg : public CDialog
{
public:
    /**
     * @brief 생성자
     * @param pParent 부모 창 (없으면 nullptr)
     */
    explicit CMyPaintDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_MYPAINT_DIALOG };
#endif

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    BOOL PreTranslateMessage(MSG* pMsg) override;
    /** @brief Enter로 대화 상자가 닫히지 않게 막음 */
    void OnOK() override {}
    /** @brief 닫기(X, Alt+F4): 저장 여부를 물은 뒤 닫음 */
    void OnCancel() override;

    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);
    afx_msg void OnDropFiles(HDROP hDropInfo);
    afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnRestoreTool(WPARAM wParam, LPARAM lParam);

    afx_msg void OnToolButton(UINT id);
    afx_msg void OnWidthChange();
    afx_msg void OnStyleChange();
    afx_msg void OnFontButton();

    afx_msg void OnFileNew();
    afx_msg void OnFileOpen();
    afx_msg void OnFileSave();
    afx_msg void OnFileSaveAs();
    afx_msg void OnAppExit();
    afx_msg void OnEditUndo();
    afx_msg void OnEditRedo();
    afx_msg void OnEditCut();
    afx_msg void OnEditCopy();
    afx_msg void OnEditPaste();
    afx_msg void OnEditClear();
    afx_msg void OnEditSelectAll();
    afx_msg void OnImageResize();
    afx_msg void OnImageCanvasSize();
    afx_msg void OnImageRotateRight();
    afx_msg void OnImageRotateLeft();
    afx_msg void OnImageRotate180();
    afx_msg void OnImageFlipH();
    afx_msg void OnImageFlipV();
    afx_msg void OnImageClear();
    afx_msg void OnViewZoomIn();
    afx_msg void OnViewZoomOut();
    afx_msg void OnViewZoom100();
    afx_msg void OnViewZoomFit();

    DECLARE_MESSAGE_MAP()

private:
    int S(int value) const { return MulDiv(value, static_cast<int>(m_dpi), 96); }
    void CreateControls();
    void CreateMenuBar();
    void CreateUiFont();
    void Layout();

    void SelectTool(ToolKind kind);
    void SyncToolOptions();
    void UpdateTitle();
    void UpdateStatus();
    void UpdateFontButton();
    CString DocumentName() const;

    bool ConfirmDiscard();
    bool OpenFile(const CString& path);
    bool Save();
    bool SaveAs();
    /** @brief 그림 전체를 바꾸는 작업을 실행 취소 기록과 함께 적용 */
    template <class Operation>
    void ApplyImageOperation(Operation operation);

    HICON m_hIcon = nullptr; ///< 창 아이콘
    UINT m_dpi = 96;

    CCanvas m_canvas;
    ToolSettings m_settings;
    CCanvasView m_view;
    CColorBar m_colorBar;

    std::array<CButton, kToolCount> m_toolButtons;
    CStatic m_widthLabel;
    CComboBox m_widthCombo;
    CStatic m_styleLabel;
    CComboBox m_styleCombo;
    CButton m_fontButton;
    CStatic m_statusPos;
    CStatic m_statusSelection;
    CStatic m_statusSize;
    CStatic m_statusZoom;
    CFont m_uiFont;
    CMenu m_menu;

    CString m_path;
    ToolKind m_tool = ToolKind::Pencil;
    ToolKind m_previousTool = ToolKind::Pencil;
    std::array<int, kToolCount> m_toolWidths{};
};
