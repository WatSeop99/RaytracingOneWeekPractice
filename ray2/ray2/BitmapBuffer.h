#pragma once

static const UINT BYTES_PER_PIXEL = 3;
static const UINT BITS_PER_PIXEL = 8 * BYTES_PER_PIXEL;

class BitmapBuffer
{
public:
	BitmapBuffer() = delete;
	BitmapBuffer(HWND hWnd, int width, double aspectRatio);
	~BitmapBuffer();

	void Draw();

	void SetColor(int x, int y, DWORD color);

private:
	HWND m_hWnd = nullptr;
	int m_Width = 100;
	int m_Height = 1;
	double m_AspectRatio = 0.0;
	DWORD m_ScanlineCount;

	HDC m_hDC = nullptr;
	HDC m_hMemDC = nullptr;
	HBITMAP m_hBitmap = nullptr;
	BYTE* m_pBitsData = nullptr;
};

