#include "framework.h"
#include "BitmapBuffer.h"

BitmapBuffer::BitmapBuffer(HWND hWnd, int width, double aspectRatio) :
	m_hWnd(hWnd),
	m_Width(width),
	m_AspectRatio(aspectRatio)
{
	if (!hWnd || hWnd == INVALID_HANDLE_VALUE)
	{
		__debugbreak();
	}

	m_Height = (UINT)(width / aspectRatio);
	m_Height = (m_Height < 1 ? 1 : m_Height);

	m_ScanlineCount = width * 3 + ((width * 3) & 3);

	m_hDC = GetDC(hWnd);
	if (!m_hDC || m_hDC == INVALID_HANDLE_VALUE)
	{
		__debugbreak();
	}

	m_hMemDC = CreateCompatibleDC(m_hDC);
	if (!m_hMemDC || m_hMemDC == INVALID_HANDLE_VALUE)
	{
		__debugbreak();
	}

	BITMAPINFO bmi = { 0, };
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = m_Width;
	bmi.bmiHeader.biHeight = m_Height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = BITS_PER_PIXEL;
	bmi.bmiHeader.biCompression = BI_RGB;

	m_hBitmap = CreateDIBSection(m_hMemDC, &bmi, DIB_RGB_COLORS, (LPVOID*)&m_pBitsData, NULL, NULL);
	if (!m_hBitmap || m_hBitmap == INVALID_HANDLE_VALUE)
	{
		__debugbreak();
	}
}

BitmapBuffer::~BitmapBuffer()
{
	if (m_hBitmap)
	{
		DeleteObject(m_hBitmap);
		m_hBitmap = nullptr;
	}
	if (m_hMemDC)
	{
		ReleaseDC(m_hWnd, m_hMemDC);
		m_hMemDC = nullptr;
	}
	if (m_hDC)
	{
		ReleaseDC(m_hWnd, m_hDC);
		m_hDC = nullptr;
	}
}

void BitmapBuffer::Draw()
{
	_ASSERT(m_hDC);
	_ASSERT(m_hMemDC);

	BitBlt(m_hDC, 0, 0, m_Width, m_Height, m_hMemDC, 0, 0, SRCCOPY);
}

void BitmapBuffer::SetColor(int x, int y, DWORD color)
{
	if (x < 0 || x >= m_Width || y < 0 || y >= m_Height)
	{
		return;
	}
	*(DWORD*)(m_pBitsData + x * BYTES_PER_PIXEL + y * m_ScanlineCount) = color;
}
