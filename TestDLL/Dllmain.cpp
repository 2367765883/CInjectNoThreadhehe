#include <Windows.h>
#include <string>



extern "C" __declspec(dllexport) void  __cdecl function() {
    std::wstring text = std::wstring(L"function ���� ");
    LPWSTR szBuffer = new WCHAR[100];
    wsprintf(szBuffer, L"%p", function);
    text += szBuffer;

    MessageBox(NULL, text.c_str(), L"��ʾ", NULL);
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH: {

        //std::wstring text = std::wstring(L"dll load main: ");
        LPWSTR szBuffer = new WCHAR[100];
        wsprintf(szBuffer, L"dll load main: %p moduleBase: %p", DllMain, hModule);
        //text += szBuffer;

        MessageBox(NULL, szBuffer, L"��ʾ", NULL);
        function();
        break;
    }  
    default:
        break;
    }
   

    return TRUE;
}