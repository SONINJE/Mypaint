/**
 * @file Dib.cpp
 * @brief CDib 구현
 */
#include "pch.h"
#include "Dib.h"

CDib::~CDib()
{
    Destroy();
}

bool CDib::Create(int width, int height)
{
    Destroy();
    if (width <= 0 || height <= 0 || width > kMaxSide || height > kMaxSide ||
        static_cast<long long>(width) * height > kMaxPixels)
    {
        return false;
    }

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hbm = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbm || !bits)
    {
        if (hbm)
            ::DeleteObject(hbm);
        return false;
    }

    HDC dc = ::CreateCompatibleDC(nullptr);
    if (!dc)
    {
        ::DeleteObject(hbm);
        return false;
    }

    m_hOldBitmap = ::SelectObject(dc, hbm);
    m_hBitmap = hbm;
    m_hdc = dc;
    m_bits = static_cast<uint32_t*>(bits);
    m_width = width;
    m_height = height;
    return true;
}

bool CDib::Create(int width, int height, COLORREF fill)
{
    if (!Create(width, height))
        return false;
    Fill(fill);
    return true;
}

bool CDib::CreateFromRegion(const CDib& src, CRect region)
{
    CRect clip;
    if (!src.IsValid() || !clip.IntersectRect(region, CRect(0, 0, src.Width(), src.Height())))
        return false;
    if (!Create(clip.Width(), clip.Height()))
        return false;

    const uint32_t* s = src.Bits();
    uint32_t* d = Bits();
    for (int y = 0; y < clip.Height(); ++y)
    {
        const uint32_t* row = s + size_t(clip.top + y) * src.Width() + clip.left;
        std::copy(row, row + clip.Width(), d + size_t(y) * m_width);
    }
    return true;
}

void CDib::Destroy()
{
    if (m_hdc)
    {
        ::SelectObject(m_hdc, m_hOldBitmap);
        ::DeleteDC(m_hdc);
    }
    if (m_hBitmap)
        ::DeleteObject(m_hBitmap);

    m_hBitmap = nullptr;
    m_hOldBitmap = nullptr;
    m_hdc = nullptr;
    m_bits = nullptr;
    m_width = 0;
    m_height = 0;
}

void CDib::Swap(CDib& other) noexcept
{
    std::swap(m_hBitmap, other.m_hBitmap);
    std::swap(m_hOldBitmap, other.m_hOldBitmap);
    std::swap(m_hdc, other.m_hdc);
    std::swap(m_bits, other.m_bits);
    std::swap(m_width, other.m_width);
    std::swap(m_height, other.m_height);
}

uint32_t* CDib::Bits()
{
    ::GdiFlush();
    return m_bits;
}

const uint32_t* CDib::Bits() const
{
    ::GdiFlush();
    return m_bits;
}

void CDib::Fill(COLORREF color)
{
    if (IsValid())
        std::fill_n(Bits(), PixelCount(), px::FromColor(color));
}

void CDib::FillRect(CRect rect, COLORREF color)
{
    CRect clip;
    if (!IsValid() || !clip.IntersectRect(rect, CRect(0, 0, m_width, m_height)))
        return;

    const uint32_t value = px::FromColor(color);
    uint32_t* bits = Bits();
    for (int y = clip.top; y < clip.bottom; ++y)
        std::fill_n(bits + size_t(y) * m_width + clip.left, clip.Width(), value);
}

void CDib::ForceOpaque()
{
    if (!IsValid())
        return;
    uint32_t* bits = Bits();
    const size_t count = PixelCount();
    for (size_t i = 0; i < count; ++i)
        bits[i] |= 0xFF000000u;
}

void CDib::Blit(const CDib& src, CPoint dest)
{
    if (!IsValid() || !src.IsValid())
        return;

    const int x0 = (std::max<int>)(0, -dest.x);
    const int x1 = (std::min<int>)(src.Width(), m_width - dest.x);
    if (x1 <= x0)
        return;

    const uint32_t* s = src.Bits();
    uint32_t* d = Bits();
    for (int y = 0; y < src.Height(); ++y)
    {
        const int dy = dest.y + y;
        if (dy < 0 || dy >= m_height)
            continue;
        const uint32_t* row = s + size_t(y) * src.Width();
        std::copy(row + x0, row + x1, d + size_t(dy) * m_width + dest.x + x0);
    }
}
