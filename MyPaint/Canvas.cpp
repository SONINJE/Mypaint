/**
 * @file Canvas.cpp
 * @brief CCanvas 구현
 */
#include "pch.h"
#include "Canvas.h"

namespace
{
constexpr size_t kMaxUndoSteps = 50;
constexpr size_t kMaxUndoBytes = size_t(768) * 1024 * 1024;

bool GetEncoderClsid(const wchar_t* mime, CLSID* clsid)
{
    UINT count = 0;
    UINT size = 0;
    if (Gdiplus::GetImageEncodersSize(&count, &size) != Gdiplus::Ok || size == 0)
        return false;

    std::vector<BYTE> buffer(size);
    auto* codecs = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    if (Gdiplus::GetImageEncoders(count, size, codecs) != Gdiplus::Ok)
        return false;

    for (UINT i = 0; i < count; ++i)
    {
        if (wcscmp(codecs[i].MimeType, mime) == 0)
        {
            *clsid = codecs[i].Clsid;
            return true;
        }
    }
    return false;
}

/** @brief CDib 픽셀 버퍼를 그대로 공유하는 GDI+ 비트맵을 만듭니다 (복사 없음). */
Gdiplus::Bitmap* WrapDib(CDib& dib)
{
    return new Gdiplus::Bitmap(dib.Width(), dib.Height(), dib.Width() * 4, PixelFormat32bppRGB,
                               reinterpret_cast<BYTE*>(dib.Bits()));
}
} // namespace

const wchar_t* MimeTypeForPath(const CString& path)
{
    const int dot = path.ReverseFind(L'.');
    const int slash = (std::max)(path.ReverseFind(L'\\'), path.ReverseFind(L'/'));
    CString ext = (dot > slash) ? path.Mid(dot + 1) : path;
    ext.MakeLower();

    if (ext == L"png")
        return L"image/png";
    if (ext == L"jpg" || ext == L"jpeg" || ext == L"jpe")
        return L"image/jpeg";
    if (ext == L"bmp" || ext == L"dib")
        return L"image/bmp";
    if (ext == L"gif")
        return L"image/gif";
    if (ext == L"tif" || ext == L"tiff")
        return L"image/tiff";
    return nullptr;
}

bool CCanvas::New(int width, int height, COLORREF background)
{
    CDib image;
    if (!image.Create(width, height, background))
        return false;
    m_image.Swap(image);
    ClearHistory();
    m_modified = false;
    return true;
}

bool CCanvas::Load(const CString& path, CString& error)
{
    std::unique_ptr<Gdiplus::Bitmap> source(Gdiplus::Bitmap::FromFile(path));
    if (!source || source->GetLastStatus() != Gdiplus::Ok)
    {
        error = L"이미지를 열 수 없습니다.\n지원하지 않는 형식이거나 손상된 파일입니다.";
        return false;
    }

    const int width = static_cast<int>(source->GetWidth());
    const int height = static_cast<int>(source->GetHeight());
    CDib image;
    if (!image.Create(width, height, RGB(255, 255, 255)))
    {
        error.Format(L"이미지가 너무 크거나 메모리가 부족합니다. (%d × %d)", width, height);
        return false;
    }

    {
        std::unique_ptr<Gdiplus::Bitmap> target(WrapDib(image));
        Gdiplus::Graphics g(target.get());
        g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        // 투명한 부분은 흰 배경 위에 합성됩니다.
        if (g.DrawImage(source.get(), Gdiplus::Rect(0, 0, width, height), 0, 0, width, height,
                        Gdiplus::UnitPixel) != Gdiplus::Ok)
        {
            error = L"이미지를 읽는 중 오류가 발생했습니다.";
            return false;
        }
    }
    image.ForceOpaque();

    m_image.Swap(image);
    ClearHistory();
    m_modified = false;
    return true;
}

bool CCanvas::Save(const CString& path, CString& error)
{
    const wchar_t* mime = MimeTypeForPath(path);
    CLSID clsid{};
    if (!mime || !GetEncoderClsid(mime, &clsid))
    {
        error = L"지원하지 않는 파일 형식입니다.\nPNG, JPEG, BMP, GIF, TIFF 중 하나로 저장하세요.";
        return false;
    }

    m_image.ForceOpaque();
    std::unique_ptr<Gdiplus::Bitmap> bitmap(WrapDib(m_image));

    ULONG quality = 95;
    Gdiplus::EncoderParameters params{};
    Gdiplus::EncoderParameters* paramsPtr = nullptr;
    if (wcscmp(mime, L"image/jpeg") == 0)
    {
        params.Count = 1;
        params.Parameter[0].Guid = Gdiplus::EncoderQuality;
        params.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
        params.Parameter[0].NumberOfValues = 1;
        params.Parameter[0].Value = &quality;
        paramsPtr = &params;
    }

    if (bitmap->Save(path, &clsid, paramsPtr) != Gdiplus::Ok)
    {
        error = L"파일을 저장하지 못했습니다.\n경로와 쓰기 권한을 확인하세요.";
        return false;
    }
    m_modified = false;
    return true;
}

COLORREF CCanvas::GetPixel(CPoint pt) const
{
    if (!Contains(pt))
        return RGB(255, 255, 255);
    return px::ToColor(m_image.Bits()[size_t(pt.y) * Width() + pt.x]);
}

// ---------------------------------------------------------------------------
// 실행 취소
// ---------------------------------------------------------------------------

CCanvas::Snapshot CCanvas::Capture() const
{
    Snapshot s;
    s.width = Width();
    s.height = Height();
    const uint32_t* bits = m_image.Bits();
    s.pixels.assign(bits, bits + m_image.PixelCount());
    return s;
}

bool CCanvas::Restore(const Snapshot& snapshot)
{
    if (snapshot.width != Width() || snapshot.height != Height())
    {
        CDib image;
        if (!image.Create(snapshot.width, snapshot.height))
            return false;
        m_image.Swap(image);
    }
    std::copy(snapshot.pixels.begin(), snapshot.pixels.end(), m_image.Bits());
    return true;
}

void CCanvas::PushUndo()
{
    m_undo.push_back(Capture());
    m_redo.clear();
    TrimUndo();
}

void CCanvas::RevertLastUndo()
{
    if (m_undo.empty())
        return;
    Restore(m_undo.back());
    m_undo.pop_back();
}

bool CCanvas::Undo()
{
    if (m_undo.empty())
        return false;
    Snapshot current = Capture();
    if (!Restore(m_undo.back()))
        return false;
    m_undo.pop_back();
    m_redo.push_back(std::move(current));
    m_modified = true;
    return true;
}

bool CCanvas::Redo()
{
    if (m_redo.empty())
        return false;
    Snapshot current = Capture();
    if (!Restore(m_redo.back()))
        return false;
    m_redo.pop_back();
    m_undo.push_back(std::move(current));
    m_modified = true;
    return true;
}

void CCanvas::ClearHistory()
{
    m_undo.clear();
    m_redo.clear();
}

void CCanvas::TrimUndo()
{
    auto totalBytes = [this]() {
        size_t bytes = 0;
        for (const auto& s : m_undo)
            bytes += s.pixels.size() * sizeof(uint32_t);
        return bytes;
    };
    while (m_undo.size() > 1 && (m_undo.size() > kMaxUndoSteps || totalBytes() > kMaxUndoBytes))
        m_undo.pop_front();
}

// ---------------------------------------------------------------------------
// 이미지 작업
// ---------------------------------------------------------------------------

bool CCanvas::FloodFill(CPoint pt, COLORREF color, int tolerance)
{
    if (!Contains(pt))
        return false;

    const int w = Width();
    const int h = Height();
    uint32_t* bits = m_image.Bits();
    const uint32_t target = bits[size_t(pt.y) * w + pt.x];
    const uint32_t fill = px::FromColor(color);
    const int tr = (target >> 16) & 0xFF, tg = (target >> 8) & 0xFF, tb = target & 0xFF;

    auto matches = [&](uint32_t p) {
        const int dr = std::abs(int((p >> 16) & 0xFF) - tr);
        const int dg = std::abs(int((p >> 8) & 0xFF) - tg);
        const int db = std::abs(int(p & 0xFF) - tb);
        return (std::max)({dr, dg, db}) <= tolerance;
    };

    std::vector<uint8_t> visited(m_image.PixelCount(), 0);
    std::vector<CPoint> stack;
    stack.push_back(pt);
    bool changed = false;

    // 스캔라인 채우기: 한 줄을 좌우로 끝까지 칠하고, 위아래 줄의 시작점만 스택에 넣습니다.
    while (!stack.empty())
    {
        const CPoint p = stack.back();
        stack.pop_back();
        const size_t row = size_t(p.y) * w;
        if (visited[row + p.x] || !matches(bits[row + p.x]))
            continue;

        int left = p.x;
        while (left > 0 && !visited[row + left - 1] && matches(bits[row + left - 1]))
            --left;
        int right = p.x;
        while (right < w - 1 && !visited[row + right + 1] && matches(bits[row + right + 1]))
            ++right;

        for (int x = left; x <= right; ++x)
        {
            if ((bits[row + x] & 0x00FFFFFF) != (fill & 0x00FFFFFF))
                changed = true;
            bits[row + x] = fill;
            visited[row + x] = 1;
        }

        for (const int ny : {p.y - 1, p.y + 1})
        {
            if (ny < 0 || ny >= h)
                continue;
            const size_t nrow = size_t(ny) * w;
            bool inRun = false;
            for (int x = left; x <= right; ++x)
            {
                const bool m = !visited[nrow + x] && matches(bits[nrow + x]);
                if (m && !inRun)
                    stack.emplace_back(x, ny);
                inRun = m;
            }
        }
    }
    return changed;
}

void CCanvas::FlipHorizontal()
{
    uint32_t* bits = m_image.Bits();
    const int w = Width();
    for (int y = 0; y < Height(); ++y)
        std::reverse(bits + size_t(y) * w, bits + size_t(y + 1) * w);
}

void CCanvas::FlipVertical()
{
    uint32_t* bits = m_image.Bits();
    const int w = Width();
    for (int top = 0, bottom = Height() - 1; top < bottom; ++top, --bottom)
        std::swap_ranges(bits + size_t(top) * w, bits + size_t(top + 1) * w, bits + size_t(bottom) * w);
}

bool CCanvas::Rotate90(bool clockwise)
{
    const int w = Width();
    const int h = Height();
    CDib rotated;
    if (!rotated.Create(h, w))
        return false;

    const uint32_t* src = m_image.Bits();
    uint32_t* dst = rotated.Bits();
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const int nx = clockwise ? (h - 1 - y) : y;
            const int ny = clockwise ? x : (w - 1 - x);
            dst[size_t(ny) * h + nx] = src[size_t(y) * w + x];
        }
    }
    m_image.Swap(rotated);
    return true;
}

bool CCanvas::ResizeCanvas(int width, int height, COLORREF background)
{
    CDib image;
    if (!image.Create(width, height, background))
        return false;
    image.Blit(m_image, CPoint(0, 0));
    m_image.Swap(image);
    return true;
}

bool CCanvas::ResizeImage(int width, int height)
{
    CDib image;
    if (!image.Create(width, height))
        return false;

    {
        std::unique_ptr<Gdiplus::Bitmap> src(WrapDib(m_image));
        std::unique_ptr<Gdiplus::Bitmap> dst(WrapDib(image));
        Gdiplus::Graphics g(dst.get());
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        Gdiplus::ImageAttributes attributes;
        attributes.SetWrapMode(Gdiplus::WrapModeTileFlipXY); // 가장자리가 어두워지지 않도록
        if (g.DrawImage(src.get(), Gdiplus::Rect(0, 0, width, height), 0, 0, Width(), Height(),
                        Gdiplus::UnitPixel, &attributes) != Gdiplus::Ok)
        {
            return false;
        }
    }
    image.ForceOpaque();
    m_image.Swap(image);
    return true;
}
