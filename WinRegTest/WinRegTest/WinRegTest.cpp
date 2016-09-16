////////////////////////////////////////////////////////////////////////////////
//
// Testing WinReg
//
// by Giovanni Dicanio <giovanni.dicanio@gmail.com>
//
////////////////////////////////////////////////////////////////////////////////

#include "WinReg.hpp"   // WinReg public header

#include <Windows.h>

#include <iostream>
#include <string>
#include <vector>

using std::wcout;
using std::wstring;
using std::vector;

// Helpers
wstring ToHexString(BYTE b);
wstring ToHexString(DWORD dw);
void PrintRegValue(const winreg::RegValue& value);


int main()
{
    wcout << "*** Testing WinReg -- by Giovanni Dicanio ***\n\n";

    const wstring testKeyName = L"SOFTWARE\\GioRegTests";

    //
    // Create a test key and write some values into it
    //
    {
        wcout << L"Creating some test key and writing some values into it...\n";

        winreg::RegKey key = winreg::CreateKey(HKEY_CURRENT_USER, testKeyName);

        winreg::RegValue v(REG_DWORD);
        v.Dword() = 0x64;
        SetValue(key.Get(), L"TestValue_DWORD", v);

        v.Reset(REG_SZ);
        v.String() = L"Hello World";
        SetValue(key.Get(), L"TestValue_SZ", v);

        v.Reset(REG_EXPAND_SZ);
        v.ExpandString() = L"%WinDir%";
        SetValue(key.Get(), L"TestValue_EXPAND_SZ", v);

        v.Reset(REG_MULTI_SZ);
        v.MultiString().push_back(L"Ciao");
        v.MultiString().push_back(L"Hi");
        v.MultiString().push_back(L"Connie");
        SetValue(key.Get(), L"TestValue_MULTI_SZ", v);

        v.Reset(REG_BINARY);
        v.Binary().push_back(0x22);
        v.Binary().push_back(0x33);
        v.Binary().push_back(0x44);
        SetValue(key.Get(), L"TestValue_BINARY", v);

        // Key automatically closed
    }


    //
    // Enum Values
    //
    {
        wcout << L"\nEnumerating values:\n";

        winreg::RegKey key = winreg::OpenKey(HKEY_CURRENT_USER, testKeyName, KEY_READ);

        const vector<wstring> valueNames = winreg::EnumerateValueNames(key.Get());

        for (const auto& valueName : valueNames)
        {
            winreg::RegValue value = winreg::QueryValue(key.Get(), valueName);
            wcout << valueName
                  << L" is of type: " << winreg::ValueTypeIdToString(value.GetType()) 
                  << L"\n";       
        
            PrintRegValue(value);
            wcout << "-----------------------------------------------------------------\n";
        }
    }


    //
    // Test Delete
    //
    {
        wcout << L"\nDeleting a value...\n";
        
        winreg::RegKey key = winreg::OpenKey(HKEY_CURRENT_USER, testKeyName, KEY_WRITE|KEY_READ);

        // Delete a value
        const wstring valueName = L"TestValue_DWORD";
        winreg::DeleteValue(key.Get(), valueName);
    
        // Try accessing a non-existent value
        try
        {
            wcout << "Trying accessing value just deleted...\n";
            winreg::RegValue value = winreg::QueryValue(key.Get(), valueName);
        }
        catch (const winreg::RegException& ex)
        {
            wcout << L"winreg::RegException correctly caught.\n";
            wcout << L"Error code: " << ex.ErrorCode() << L'\n';
            // Error code should be 2: ERROR_FILE_NOT_FOUND
            if (ex.ErrorCode() == ERROR_FILE_NOT_FOUND)
            {
                wcout << L"All right, I expected ERROR_FILE_NOT_FOUND (== 2).\n\n";
            }
        }
        key.Close();

        // Delete the whole key
        winreg::DeleteKey(HKEY_CURRENT_USER, testKeyName);
    }
}


wstring ToHexString(BYTE b)
{
    wchar_t buf[10];
    swprintf_s(buf, L"0x%02X", b);
    return wstring(buf);
}


wstring ToHexString(DWORD dw)
{
    wchar_t buf[20];
    swprintf_s(buf, L"0x%08X", dw);
    return wstring(buf);
}


void PrintRegValue(const winreg::RegValue& value)
{
    switch (value.GetType())
    {
    case REG_NONE:
    {
        wcout << L"None\n";
    }
    break;

    case REG_BINARY:
    {
        const vector<BYTE> & data = value.Binary();
        for (BYTE x : data)
        {
            wcout << ToHexString(x) << L" ";
        }
        wcout << L"\n";
    }
    break;

    case REG_DWORD:
    {
        DWORD dw = value.Dword();
        wcout << ToHexString(dw) << L'\n';
    }
    break;

    case REG_EXPAND_SZ:
    {
        wcout << L"[" << value.ExpandString() << L"]\n";

        wcout << L"Expanded: [" << winreg::ExpandEnvironmentStrings(value.ExpandString()) << "]\n";
    }
    break;

    case REG_MULTI_SZ:
    {
        const vector<wstring>& multiString = value.MultiString();
        for (const auto& s : multiString)
        {
            wcout << L"[" << s << L"]\n";
        }
    }
    break;

    case REG_SZ:
    {
        wcout << L"[" << value.String() << L"]\n";
    }
    break;

    default:
        wcout << L"Unsupported/Unknown registry value type\n";
        break;
    }
}

