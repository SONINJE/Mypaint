/**
 * @file MyPaint.cpp
 * @brief 애플리케이션 클래스 구현
 */
#include "pch.h"
#include "MyPaint.h"
#include "MyPaintDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CMyPaintApp, CWinApp)
END_MESSAGE_MAP()

/** @brief 유일한 애플리케이션 개체 */
CMyPaintApp theApp;

CMyPaintApp::CMyPaintApp()
{
}

BOOL CMyPaintApp::InitInstance()
{
    // 공용 컨트롤 6.0(비주얼 스타일)을 쓰려면 InitCommonControlsEx가 필요합니다.
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    CWinApp::InitInstance();

    // 이미지 파일 입출력과 부드러운 선 그리기에 GDI+를 씁니다.
    Gdiplus::GdiplusStartupInput gdiplusInput;
    ULONG_PTR gdiplusToken = 0;
    if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusInput, nullptr) != Gdiplus::Ok)
    {
        AfxMessageBox(L"GDI+를 초기화하지 못했습니다.", MB_ICONERROR);
        return FALSE;
    }

    {
        CMyPaintDlg dlg;
        m_pMainWnd = &dlg;
        dlg.DoModal();
        m_pMainWnd = nullptr;
    } // GDI+ 종료 전에 대화 상자와 그 안의 GDI+ 개체를 먼저 정리

    Gdiplus::GdiplusShutdown(gdiplusToken);

    // 대화 상자가 닫혔으므로 메시지 펌프를 시작하지 않고 종료합니다.
    return FALSE;
}
