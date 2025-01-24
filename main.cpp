#include "rawcopy.h"
#include <iostream>
#include <windows.h>  
#include <string>

std::wstring GetFileName(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/"); 
    return (pos != std::wstring::npos) ? path.substr(pos + 1) : path;
}


int main()
{
    std::wstring src = L""; // fill with your path to origin
    wchar_t tempPathBuffer[MAX_PATH];
    DWORD tempPathLength = GetEnvironmentVariableW(L"TEMP", tempPathBuffer, MAX_PATH);
    if (tempPathLength == 0 || tempPathLength > MAX_PATH) {
        std::wcerr << L"Error: Unable to retrieve the TEMP directory path.\n";
        return 1;
    }
    std::wstring tempPath(tempPathBuffer);
    std::wstring fileName = GetFileName(src);
    std::wstring dst = tempPath + L"\\" + fileName;
    unsigned long result = RawCopy(src, dst);
    if (result == ERROR_SUCCESS)
    {
        std::wcout << L"File moved successfully from " << src << L" to " << dst << L".\n";
    }
    else
    {
        std::wcout << L"Failed to move file. Error code: " << result << L"\n";
    }
    return (result == ERROR_SUCCESS) ? 0 : 1;
}