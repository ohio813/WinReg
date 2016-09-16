////////////////////////////////////////////////////////////////////////////////
//
// WinReg -- C++ Wrappers around Windows Registry APIs
//
// by Giovanni Dicanio <giovanni.dicanio@gmail.com>
//
// FILE: WinReg.cpp
// DESC: Module implementation. 
//
////////////////////////////////////////////////////////////////////////////////


//------------------------------------------------------------------------------
//                              Includes
//------------------------------------------------------------------------------

#include "WinReg.hpp"   // Module header

// C library
#include <string.h>     // wcslen()

// C++ library
#include <limits>       // numeric_limits
#include <stdexcept>    // overflow_error



//------------------------------------------------------------------------------
//                      Private Helper Functions
//------------------------------------------------------------------------------
namespace
{


// MSVC emits a warning in 64-bit builds when assigning size_t to DWORD.
// So, only in 64-bit builds, check proper size limits before conversion
// and throw std::overflow_error if the size_t value is too big.
inline DWORD SafeSizeToDwordCast(size_t size)
{
#ifdef _WIN64
    if (size > static_cast<size_t>((std::numeric_limits<DWORD>::max)()))
    {
        throw std::overflow_error(
            "SafeSizeToDwordCast(): Input size_t too long, it doesn't fit in a DWORD.");
    }

    // This cast is now safe
    return static_cast<DWORD>(size);
#else
    // Just fine in 32-bit builds
    return size;
#endif
}



//
// Helpers called by QueryValue() to read actual data from the registry.
//
// NOTE: The "valueSize" parameter contains the size of the value to be read in *BYTES*.
// This is important for example to helper functions reading strings (REG_SZ, etc.), 
// as usually std::wstring methods consider sizes in wchar_ts.
//

// Reads a REG_DWORD value.
winreg::RegValue ReadValueDwordInternal(HKEY hKey, const std::wstring& valueName)
{
    GD_WINREG_ASSERT(hKey != nullptr);

    DWORD valueData = 0;
    DWORD valueSize = sizeof(valueData);

    LONG result = ::RegQueryValueEx(
        hKey, 
        valueName.c_str(),
        nullptr, // reserved
        nullptr, // type not required in this helper: we're called as dispatching by QueryValue()
        reinterpret_cast<BYTE*>(&valueData),   // where data will be read
        &valueSize
    );
    GD_WINREG_ASSERT(valueSize == sizeof(DWORD)); // we read a DWORD

    if (result != ERROR_SUCCESS)
    {
        throw winreg::RegException("RegQueryValueEx() failed in returning REG_DWORD value.", result);
    }

    winreg::RegValue value(REG_DWORD);
    value.Dword() = valueData;
    return value;
}


// Reads a REG_SZ value.
winreg::RegValue ReadValueStringInternal(HKEY hKey, const std::wstring& valueName, DWORD valueSize)
{
    GD_WINREG_ASSERT(hKey != nullptr);

    // valueSize is in bytes, we need string length in wchar_ts
    const DWORD stringBufferLenInWchars = valueSize / sizeof(wchar_t);
    
    // Make room for result string
    std::wstring str;
    str.resize(stringBufferLenInWchars);

    DWORD sizeInBytes = valueSize;

    LONG result = ::RegQueryValueEx(
        hKey, 
        valueName.c_str(),
        nullptr, // reserved
        nullptr, // not interested in type (we know it's REG_SZ)
        reinterpret_cast<BYTE*>(&str[0]),   // where data will be read
        &sizeInBytes 
    );
    if (result != ERROR_SUCCESS)
    {
        throw winreg::RegException("RegQueryValueEx() failed in returning REG_SZ value.", result);
    }

    //
    // In the remarks section of RegQueryValueEx()
    //
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms724911(v=vs.85).aspx
    //
    // they specify that we should check if the string is NUL-terminated, and if it isn't,
    // we must add a NUL-terminator.
    //
    if (str[stringBufferLenInWchars - 1] == L'\0')
    {
        // Strip off the NUL-terminator written by the API
        str.resize(stringBufferLenInWchars - 1);
    }
    // The API didn't write a NUL terminator, at the end of the string, which is just fine,
    // as wstrings are automatically NUL-terminated.

    winreg::RegValue value(REG_SZ);
    value.String() = str;
    return value;
}


// Reads a REG_EXPAND_SZ value.
winreg::RegValue ReadValueExpandStringInternal(HKEY hKey, const std::wstring& valueName, DWORD valueSize)
{
    // Almost copy-and-paste from ReadValueStringInternal()
    GD_WINREG_ASSERT(hKey != nullptr);

    // valueSize is in bytes, we need string length in wchar_ts
    const DWORD stringBufferLenInWchars = valueSize / sizeof(wchar_t);

    // Make room for result string
    std::wstring str;
    str.resize(stringBufferLenInWchars);
    DWORD sizeInBytes = valueSize;

    LONG result = ::RegQueryValueEx(
        hKey, 
        valueName.c_str(),
        nullptr, // reserved
        nullptr, // not interested in type (we know it's REG_EXPAND_SZ)
        reinterpret_cast<BYTE*>(&str[0]),   // where data will be read
        &sizeInBytes
    );
    if (result != ERROR_SUCCESS)
    {
        throw winreg::RegException("RegQueryValueEx() failed in returning REG_EXPAND_SZ value.", 
            result);
    }

    //
    // In the remarks section of RegQueryValueEx()
    //
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms724911(v=vs.85).aspx
    //
    // they specify that we should check if the string is NUL-terminated, and if it isn't,
    // we must add a NUL-terminator.
    //
    if (str[stringBufferLenInWchars - 1] == L'\0')
    {
        // Strip off the NUL-terminator written by the API
        str.resize(stringBufferLenInWchars - 1);
    }
    // The API didn't write a NUL terminator, at the end of the string, which is just fine,
    // as wstrings are automatically NUL-terminated.

    winreg::RegValue value(REG_EXPAND_SZ);
    value.ExpandString() = str;
    return value;
}


// Reads a REG_BINARY value.
winreg::RegValue ReadValueBinaryInternal(HKEY hKey, const std::wstring& valueName, DWORD valueSize)
{
    GD_WINREG_ASSERT(hKey != nullptr);

    // Data to be read from the registry
    std::vector<BYTE> binaryData(valueSize);
    DWORD sizeInBytes = valueSize;

    LONG result = ::RegQueryValueEx(
        hKey, 
        valueName.c_str(),
        nullptr, // reserved
        nullptr, // not interested in type (we know it's REG_BINARY)
        binaryData.data(),   // where data will be read
        &sizeInBytes
    );
    if (result != ERROR_SUCCESS)
    {
        throw winreg::RegException("RegQueryValueEx() failed in returning REG_BINARY value.", result);
    }

    winreg::RegValue value(REG_BINARY);
    value.Binary() = binaryData;
    return value;
}


// Reads a REG_MULTI_SZ value.
winreg::RegValue ReadValueMultiStringInternal(HKEY hKey, const std::wstring& valueName, DWORD valueSize)
{
    GD_WINREG_ASSERT(hKey != nullptr);

    // Multi-string parsed into a vector of strings
    std::vector<std::wstring> multiStrings;

    // Buffer containing the multi-string
    std::vector<wchar_t> buffer(valueSize);
    
    DWORD sizeInBytes = valueSize;
    LONG result = ::RegQueryValueEx(
        hKey, 
        valueName.c_str(),
        nullptr, // reserved
        nullptr, // not interested in type (we know it's REG_MULTI_SZ)
        reinterpret_cast<BYTE*>(buffer.data()),   // where data will be read
        &sizeInBytes
    );
    if (result != ERROR_SUCCESS)
    {
        throw winreg::RegException("RegQueryValueEx() failed in returning REG_MULTI_SZ value.", 
            result);
    }

    // Scan the read multi-string buffer, and parse the single various strings,
    // adding them to the result vector<wstring>.
    const wchar_t* pszz = buffer.data();
    while (*pszz != L'\0')
    {
        // Get current string length
        const size_t len = wcslen(pszz);

        // Add this string to the resulting vector
        multiStrings.push_back(std::wstring(pszz, len));
        
        // Point to next string (or end: \0)
        pszz += len + 1;
    }

    winreg::RegValue value(REG_MULTI_SZ);
    value.MultiString() = multiStrings;
    return value;
}



//
// Helpers for SetValue()
//

void WriteValueBinaryInternal(HKEY hKey, const std::wstring& valueName, const winreg::RegValue& value)
{
    GD_WINREG_ASSERT(hKey != nullptr);
    GD_WINREG_ASSERT(value.GetType() == REG_BINARY);

    const std::vector<BYTE> & data = value.Binary();
    const DWORD dataSize = SafeSizeToDwordCast(data.size());
    LONG result = ::RegSetValueEx(
        hKey, 
        valueName.c_str(),
        0, // reserved
        REG_BINARY,
        &data[0],
        dataSize);
    if (result != ERROR_SUCCESS)
    {
        throw winreg::RegException("RegSetValueEx() failed in writing REG_BINARY value.", result);
    }
}


void WriteValueDwordInternal(HKEY hKey, const std::wstring& valueName, const winreg::RegValue& value)
{
    GD_WINREG_ASSERT(hKey != nullptr);
    GD_WINREG_ASSERT(value.GetType() == REG_DWORD);

    const DWORD data = value.Dword();
    const DWORD dataSize = sizeof(data);
    LONG result = ::RegSetValueEx(
        hKey, 
        valueName.c_str(),
        0, // reserved
        REG_DWORD,
        reinterpret_cast<const BYTE*>(&data),
        dataSize);
    if (result != ERROR_SUCCESS)
    {
        throw winreg::RegException("RegSetValueEx() failed in writing REG_DWORD value.", result);
    }
}


void WriteValueStringInternal(HKEY hKey, const std::wstring& valueName, const winreg::RegValue& value)
{
    GD_WINREG_ASSERT(hKey != nullptr);
    GD_WINREG_ASSERT(value.GetType() == REG_SZ);

    const std::wstring& str = value.String();

    // According to MSDN doc, this size must include the terminating NUL
    // Note that size is in *BYTES*, so we must scale by wchar_t.
    const DWORD dataSize = SafeSizeToDwordCast( (str.size() + 1) * sizeof(wchar_t) );

    LONG result = ::RegSetValueEx(
        hKey, 
        valueName.c_str(),
        0, // reserved
        REG_SZ,
        reinterpret_cast<const BYTE*>(str.c_str()),
        dataSize);
    if (result != ERROR_SUCCESS)
    {
        throw winreg::RegException("RegSetValueEx() failed in writing REG_SZ value.", result);
    }
}


void WriteValueExpandStringInternal(HKEY hKey, const std::wstring& valueName, const winreg::RegValue& value)
{
    GD_WINREG_ASSERT(hKey != nullptr);
    GD_WINREG_ASSERT(value.GetType() == REG_EXPAND_SZ);

    const std::wstring & str = value.ExpandString();

    // According to MSDN doc, this size must include the terminating NUL.
    // Note that size is in *BYTES*, so we must scale by wchar_t.
    const DWORD dataSize = SafeSizeToDwordCast((str.size() + 1) * sizeof(wchar_t));

    LONG result = ::RegSetValueEx(
        hKey, 
        valueName.c_str(),
        0, // reserved
        REG_EXPAND_SZ,
        reinterpret_cast<const BYTE*>(str.c_str()),
        dataSize);
    if (result != ERROR_SUCCESS)
    {
        throw winreg::RegException("RegSetValueEx() failed in writing REG_EXPAND_SZ value.", result);
    }
}


void WriteValueMultiStringInternal(HKEY hKey, const std::wstring& valueName, const winreg::RegValue& value)
{
    GD_WINREG_ASSERT(hKey != nullptr);
    GD_WINREG_ASSERT(value.GetType() == REG_MULTI_SZ);

    const std::vector<std::wstring> & multiString = value.MultiString();

    // We need to build a whole array containing the multi-strings, with double-NUL termination
    std::vector<wchar_t> buffer;

    // Get total buffer size, in wchar_ts
    size_t totalLen = 0;
    for (const std::wstring& s : multiString)
    {
        // +1 to include the terminating NUL for current string
        totalLen += (s.size() + 1);
    }
    // Consider another terminating NUL (double-NUL-termination)
    totalLen++;

    // Optimization: reserve room in the multi-string buffer
    buffer.resize(totalLen);

    // Deep copy the single strings in the buffer
    if (!multiString.empty())
    {
        wchar_t* dest = &buffer[0];
        for (const std::wstring& s : multiString)
        {
            // Copy the whole string to destination buffer, including the terminating NUL
            wmemcpy(dest, s.c_str(), s.size() + 1);

            // Skip to the next string slot
            dest += s.size() + 1;
        }

        // Add another NUL terminator
        *dest = L'\0';
    }
    else
    {
        // Just write two NULs in the buffer
        buffer.resize(2);
        buffer[0] = L'\0';
        buffer[1] = L'\0';
    }

    // Size is in *BYTES*
    const DWORD dataSize = SafeSizeToDwordCast( buffer.size() * sizeof(wchar_t) );

    LONG result = ::RegSetValueEx(
        hKey, 
        valueName.c_str(),
        0, // reserved
        REG_MULTI_SZ,
        reinterpret_cast<const BYTE*>(buffer.data()),
        dataSize);
    if (result != ERROR_SUCCESS)
    {
        throw winreg::RegException("RegSetValueEx() failed in writing REG_MULTI_SZ value.", result);
    }
}


} // namespace



//------------------------------------------------------------------------------
//                      Public Functions Implementations
//------------------------------------------------------------------------------


namespace winreg
{


std::vector<std::wstring> EnumerateSubKeyNames(HKEY hKey)
{
    GD_WINREG_ASSERT(hKey != nullptr);

    // Get sub-keys count and max sub-key name length
    DWORD subkeyCount = 0;
    DWORD maxSubkeyNameLength = 0;
    LONG result = ::RegQueryInfoKey(
        hKey, 
        nullptr, nullptr,           // not interested in user-defined class of the key 
        nullptr,                    // reserved
        &subkeyCount,               // how many sub-keys here? 
        &maxSubkeyNameLength,       // useful to preallocate a buffer for all keys
        nullptr, nullptr, nullptr,  // not interested in all this stuff
        nullptr, nullptr, nullptr   // (see MSDN doc)
    );
    if (result != ERROR_SUCCESS)
    {
        throw RegException("RegQueryInfoKey() failed while trying to get sub-keys info.", result);
    }

    // Result of the function
    std::vector<std::wstring> subkeyNames;

    // Temporary buffer to read sub-key names into
    std::vector<wchar_t> subkeyNameBuffer(maxSubkeyNameLength + 1); // +1 for terminating NUL

    // For each sub-key:
    for (DWORD subkeyIndex = 0; subkeyIndex < subkeyCount; subkeyIndex++)
    {
        DWORD subkeyNameLength = SafeSizeToDwordCast( subkeyNameBuffer.size() ); // including NUL
        
        result = ::RegEnumKeyEx(
            hKey, 
            subkeyIndex, 
            &subkeyNameBuffer[0], 
            &subkeyNameLength, 
            nullptr, nullptr, nullptr, nullptr);
        if (result != ERROR_SUCCESS)
        {
            throw RegException("RegEnumKeyEx() failed trying to get sub-key name.", result);
        }

        // When the RegEnumKeyEx() function returns, subkeyNameBufferSize
        // contains the number of characters read, *NOT* including the terminating NUL
        subkeyNames.push_back(std::wstring(subkeyNameBuffer.data(), subkeyNameLength));
    }

    return subkeyNames;
}


RegKey OpenKey(HKEY hKey, const std::wstring& subKeyName, REGSAM accessRights)
{
    GD_WINREG_ASSERT(hKey != nullptr);

    HKEY hKeyResult = nullptr;
    LONG result = ::RegOpenKeyEx(
        hKey,
        subKeyName.c_str(),
        0, // no special option of symbolic link
        accessRights,
        &hKeyResult
    );
    if (result != ERROR_SUCCESS)
    {
        throw RegException("RegOpenKeyEx() failed trying opening a key.", result);
    }

    return RegKey(hKeyResult);
}


RegKey CreateKey(HKEY hKey, const std::wstring& subKeyName,
    DWORD options, REGSAM accessRights,
    LPSECURITY_ATTRIBUTES securityAttributes,
    LPDWORD disposition)
{
    GD_WINREG_ASSERT(hKey != nullptr);

    HKEY hKeyResult = nullptr;
    LONG result = ::RegCreateKeyEx(
        hKey,
        subKeyName.c_str(),
        0,          // reserved
        nullptr,    // no user defined class
        options,
        accessRights,
        securityAttributes,
        &hKeyResult,
        disposition
    );
    if (result != ERROR_SUCCESS)
    {
        throw RegException("RegCreateKeyEx() failed.", result);
    }

    return RegKey(hKeyResult);
}


std::vector<std::wstring> EnumerateValueNames(HKEY hKey)
{
    GD_WINREG_ASSERT(hKey != nullptr);

    // Get values count and max value name length
    DWORD valueCount = 0;
    DWORD maxValueNameLength = 0;
    LONG result = ::RegQueryInfoKey(
        hKey,
        nullptr, nullptr,
        nullptr,
        nullptr, nullptr,
        nullptr,
        &valueCount, &maxValueNameLength,
        nullptr, nullptr, nullptr);
    if (result != ERROR_SUCCESS)
    {
        throw RegException("RegQueryInfoKey() failed while trying to get value info.", result);
    }

    std::vector<std::wstring> valueNames;

    // Temporary buffer to read value names into
    std::vector<wchar_t> valueNameBuffer(maxValueNameLength + 1); // +1 for including NUL

    // For each value in this key:
    for (DWORD valueIndex = 0; valueIndex < valueCount; valueIndex++)
    {
        DWORD valueNameLength = SafeSizeToDwordCast(valueNameBuffer.size()); // including NUL
        
        // We are just interested in the value's name
        result = ::RegEnumValue(
            hKey, 
            valueIndex, 
            &valueNameBuffer[0], 
            &valueNameLength, 
            nullptr,    // reserved
            nullptr,    // not interested in type
            nullptr,    // not interested in data
            nullptr     // not interested in data size
        );
        if (result != ERROR_SUCCESS)
        {
            throw RegException("RegEnumValue() failed to get value name.", result);
        }

        // When the RegEnumValue() function returns, valueNameLength
        // contains the number of characters read, not including the terminating NUL
        valueNames.push_back(std::wstring(valueNameBuffer.data(), valueNameLength));
    }

    return valueNames;
}


RegValue QueryValue(HKEY hKey, const std::wstring& valueName)
{
    GD_WINREG_ASSERT(hKey != nullptr);

    DWORD valueType = 0;

    // According to this MSDN web page:
    //
    // "Registry Element Size Limits"
    //  https://msdn.microsoft.com/en-us/library/windows/desktop/ms724872(v=vs.85).aspx
    //
    // "Long values (more than 2,048 bytes) should be stored in a file, 
    // and the location of the file should be stored in the registry. 
    // This helps the registry perform efficiently."
    //

    DWORD dataSize = 0;

    // Query the value type and the data size for that value
    LONG result = ::RegQueryValueEx(
        hKey, 
        valueName.c_str(),
        nullptr,        // reserved
        &valueType,
        nullptr,        // not ready to pass buffer to write data into yet
        &dataSize       // ask this API the total bytes for the data
    );
    if (result != ERROR_SUCCESS)
    {
        throw RegException("RegQueryValueEx() failed in returning value info.", result);
    }

    // Dispatch to internal helper function based on the value's type
    switch (valueType)
    {
    case REG_BINARY:    return ReadValueBinaryInternal(hKey, valueName, dataSize);
    case REG_DWORD:     return ReadValueDwordInternal(hKey, valueName);
    case REG_SZ:        return ReadValueStringInternal(hKey, valueName, dataSize);
    case REG_EXPAND_SZ: return ReadValueExpandStringInternal(hKey, valueName, dataSize);
    case REG_MULTI_SZ:  return ReadValueMultiStringInternal(hKey, valueName, dataSize);

    default:
        throw std::invalid_argument("Unsupported Windows Registry value type.");
    }
}


void SetValue(HKEY hKey, const std::wstring& valueName, const RegValue& value)
{
    GD_WINREG_ASSERT(hKey != nullptr);

    switch (value.GetType())
    {
    case REG_BINARY:    return WriteValueBinaryInternal(hKey, valueName, value);
    case REG_DWORD:     return WriteValueDwordInternal(hKey, valueName, value);
    case REG_SZ:        return WriteValueStringInternal(hKey, valueName, value);
    case REG_EXPAND_SZ: return WriteValueExpandStringInternal(hKey, valueName, value);
    case REG_MULTI_SZ:  return WriteValueMultiStringInternal(hKey, valueName, value);

    default:
        throw std::invalid_argument("Unsupported Windows Registry value type.");
    }
}


void DeleteValue(HKEY hKey, const std::wstring& valueName)
{
    GD_WINREG_ASSERT(hKey != nullptr);

    LONG result = ::RegDeleteValue(hKey, valueName.c_str());
    if (result != ERROR_SUCCESS)
    {
        throw RegException("RegDeleteValue() failed.", result);
    }
}


void DeleteKey(HKEY hKey, const std::wstring& subKey, REGSAM view)
{
    GD_WINREG_ASSERT(hKey != nullptr);

    LONG result = ::RegDeleteKeyEx(hKey, subKey.c_str(), view, 0);
    if (result != ERROR_SUCCESS)
    {
        throw RegException("RegDeleteKeyEx() failed.", result);
    }
}


std::wstring ExpandEnvironmentStrings(const std::wstring& source)
{
    DWORD requiredLen = ::ExpandEnvironmentStrings(source.c_str(), nullptr, 0);
    if (requiredLen == 0)
    {
        return std::wstring(); // empty
    }

    std::wstring str;
    str.resize(requiredLen);
    DWORD len = ::ExpandEnvironmentStrings(source.c_str(), &str[0], requiredLen);
    if (len == 0)
    {
        // Probably error?
        // ...but just return an empty string if can't expand.
        return std::wstring();
    }

    // Size (len) returned by ExpandEnvironmentStrings() includes the terminating NUL,
    // so subtract - 1 to remove it.
    str.resize(len - 1);
    return str;
}


std::wstring ValueTypeIdToString(DWORD typeId)
{
    switch (typeId)
    {
    case REG_BINARY:    return L"REG_BINARY";       break;
    case REG_DWORD:     return L"REG_DWORD";        break;
    case REG_EXPAND_SZ: return L"REG_EXPAND_SZ";    break;
    case REG_MULTI_SZ:  return L"REG_MULTI_SZ";     break;
    case REG_SZ:        return L"REG_SZ";           break;

    default:
        // Should I throw?
        return L"Unsupported/Unknown registry value type";
        break;
    }
}


void LoadKey(HKEY hKey, const std::wstring& subKey, const std::wstring& filename)
{
    LONG result = ::RegLoadKey(hKey, subKey.c_str(), filename.c_str());
    if (result != ERROR_SUCCESS)
    {
        throw RegException("RegLoadKey failed.", result);
    }
}


void SaveKey(HKEY hKey, const std::wstring& filename, LPSECURITY_ATTRIBUTES security)
{
    LONG result = ::RegSaveKey(hKey, filename.c_str(), security);
    if (result != ERROR_SUCCESS)
    {
        throw RegException("RegSaveKey failed.", result);
    }
}


RegKey ConnectRegistry(const std::wstring& machineName, HKEY hKey)
{
    HKEY hKeyResult = nullptr;
    LONG result = ::RegConnectRegistry(machineName.c_str(), hKey, &hKeyResult);
    if (result != ERROR_SUCCESS)
    {
        throw RegException("RegConnectRegistry failed.", result);
    }

    return RegKey(hKeyResult);
}


} // namespace winreg

