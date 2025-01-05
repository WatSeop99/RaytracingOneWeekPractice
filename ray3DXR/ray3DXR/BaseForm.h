#pragma once

#define ALIGN_TO(_alignment, _val) (((_val + _alignment - 1) / _alignment) * _alignment)

class BaseForm
{
public:
	BaseForm(HINSTANCE hInstance) : m_hInstance(hInstance) { }
	virtual ~BaseForm() = default;

	void Run();

	virtual void OnLoad() = 0;
	virtual void OnFrameRender() = 0;
	virtual void OnShutdown() = 0;

private:
	HWND CreateWindowHandle(const WCHAR* pszWINDOW_TITLE, UINT width, UINT height);

protected:
	HINSTANCE m_hInstance = nullptr;
	HWND m_hwnd = nullptr;
	UINT m_Width = 0;
	UINT m_Height = 0;
};
