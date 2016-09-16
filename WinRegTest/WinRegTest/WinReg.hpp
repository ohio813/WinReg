////////////////////////////////////////////////////////////////////////////////
//
// WinReg -- C++ Wrappers around Windows Registry APIs
//
// by Giovanni Dicanio <giovanni.dicanio@gmail.com>
//
// FILE: WinReg.hpp
// DESC: Module header, containing public interface and inline implementations. 
//
////////////////////////////////////////////////////////////////////////////////

#ifndef GIOVANNI_DICANIO_WINREG_HPP_INCLUDED
#define GIOVANNI_DICANIO_WINREG_HPP_INCLUDED

//==============================================================================
//
// *** NOTES ***
//
// Assume Unicode builds.
// TCHAR-model == OBSOLETE (sucks these days!)
//
//
// My Coding Style Rules for this module:
// --------------------------------------
// Max column limit = 100 columns
// Indent 4 spaces (more readable than 2)
// Aligned matching braces for better grouping and readability.
//
//==============================================================================


//------------------------------------------------------------------------------
//                              Includes
//------------------------------------------------------------------------------

#include <Windows.h>    // Windows Platform SDK
#include <crtdbg.h>     // _ASSERTE()

#include <stdexcept>    // std::invalid_argument, std::runtime_error
#include <string>       // std::wstring
#include <utility>      // std::swap()
#include <vector>       // std::vector


namespace winreg
{

//------------------------------------------------------------------------------
//                          Debugging Helpers
//------------------------------------------------------------------------------


// Debug-build-only assertion
#define GD_WINREG_ASSERT(expr) _ASSERTE(expr)

// Asserts on expression in debug builds; 
// just evaluates expression in release builds.
#ifdef _DEBUG
#define GD_WINREG_VERIFY(expr) _ASSERTE(expr)
#else
#define GD_WINREG_VERIFY(expr) (expr)
#endif // DEBUG



//------------------------------------------------------------------------------
// Convenient C++ wrapper on raw HKEY registry key handle.
//
// This class is movable but non-copyable. 
// Key is automatically closed by the destructor.
// *Don't* call ::RegCloseKey() on this key wrapper!
//------------------------------------------------------------------------------
class RegKey
{
public:

    // Creates an empty wrapper
    RegKey() noexcept;

    // Takes ownership of the input raw key handle
    explicit RegKey(HKEY hKey) noexcept;

    // Moves ownership from other to this
    RegKey(RegKey&& other) noexcept;

    // Releases any owned key handle, and moves ownership from other to this
    RegKey& operator=(RegKey&& other) noexcept;

    // Safely closes the wrapped raw handle (if any)
    ~RegKey() noexcept;

    // Ban copy
    RegKey(const RegKey&) = delete;
    RegKey& operator=(const RegKey&) = delete;


    // Access the wrapped handle
    HKEY Get() const noexcept;

    // Safely close if any key is open
    void Close() noexcept;

    // Does it wrap a valid handle?
    bool IsValid() const noexcept;


    // Transfers ownership from this to the caller
    HKEY Detach() noexcept;

    // Takes ownership of input key (any currently wrapped key is deleted)
    void Attach(HKEY hKey) noexcept;

    // Swaps with other
    void Swap(RegKey& other) noexcept;


private:
    // The raw key wrapped handle
    HKEY m_hKey;
};

// Follow STL's swap() pattern
void swap(RegKey& lhs, RegKey& rhs) noexcept;



//------------------------------------------------------------------------------
// Exception indicating errors from Windows Registry APIs.
//------------------------------------------------------------------------------
class RegException : public std::runtime_error
{
public:
    RegException(const char* msg, LONG errorCode);
    RegException(const std::string& msg, LONG errorCode);

    LONG ErrorCode() const noexcept;

private:
    LONG m_errorCode;
};



//------------------------------------------------------------------------------
//
// "Variant-style" Registry value.
//
// Currently supports the following value types:
//
//  Windows Registry Type       C++ higher-level type
// -----------------------------------------------------------
// REG_DWORD                    DWORD
// REG_SZ                       std::wstring
// REG_EXPAND_SZ                std::wstring
// REG_MULTI_SZ                 std::vector<std::wstring>
// REG_BINARY                   std::vector<BYTE>
//
//------------------------------------------------------------------------------
class RegValue
{
public:

    typedef DWORD TypeId; // REG_SZ, REG_DWORD, etc.

    // Initialize empty (type is REG_NONE)
    RegValue() = default;

    // Initialize with the given type.
    // Caller can use accessor corresponding to the given type (e.g. String() for REG_SZ)
    // to set the desired value.
    explicit RegValue(TypeId type);

    // Registry value type (e.g. REG_SZ) associated to current value.
    TypeId GetType() const;
    
    // Reset current "variant-style" value to the specified type.
    // Note that previous values are cleared.
    // Caller can use accessor corresponding to the given type (e.g. String() for REG_SZ)
    // to set the desired value.
    void Reset(TypeId type = REG_NONE);

    // Is it REG_NONE?
    // Note: "empty" (i.e. REG_NONE) can be used to indicate error conditions as well
    bool IsEmpty() const;


    // Those accessors asserts using GD_WINREG_ASSERT in debug builds,
    // and throw exception std::invalid_argument in release builds,
    // if the queried value doesn't match the registry value type.

    DWORD Dword() const;                                    // REG_DWORD
    const std::wstring & String() const;                    // REG_SZ
    const std::wstring & ExpandString() const;              // REG_EXPAND_SZ
    const std::vector<std::wstring> & MultiString() const;  // REG_MULTI_SZ
    const std::vector<BYTE> & Binary() const;               // REG_BINARY

    DWORD& Dword();                                         // REG_DWORD
    std::wstring & String();                                // REG_SZ
    std::wstring & ExpandString();                          // REG_EXPAND_SZ
    std::vector<std::wstring> & MultiString();              // REG_MULTI_SZ
    std::vector<BYTE> & Binary();                           // REG_BINARY


    // *** IMPLEMENTATION ***
private:
    // Win32 Registry value type
    TypeId m_typeId{ REG_NONE };

    DWORD m_dword{ 0 };                         // REG_DWORD
    std::wstring m_string;                      // REG_SZ
    std::wstring m_expandString;                // REG_EXPAND_SZ
    std::vector<std::wstring> m_multiString;    // REG_MULTI_SZ
    std::vector<BYTE> m_binary;                 // REG_BINARY

    // Clear all the data members representing various values (m_dword, m_string, etc.)
    void ResetValues();
};



//------------------------------------------------------------------------------
//
// The following functions wrap Win32 Windows Registry C-interface APIs.
// If those API calls fail, an exception of type RegException is thrown.
//
// Many of these functions have parameters that reflect the original parameters in the
// wrapped raw Win32 APIs (e.g. REGSAM, LPSECURITY_ATTRIBUTES, etc.).
// Please read MSDN documentation for the corresponding raw Win32 APIs for details
// on those parameters.
//
//------------------------------------------------------------------------------

// Wrapper on RegOpenKeyEx().
// See MSDN doc for ::RegOpenKeyEx().
// Note that the returned key is RAII-wrapped, so *don't* call ::RegCloseKey() on it!
// Let just RegKey's destructor close the key.
RegKey OpenKey(HKEY hKey, const std::wstring& subKeyName, REGSAM accessRights = KEY_READ);

// Wrapper on RegCreateKeyEx().
// See MSDN doc for ::RegCreateKeyEx().
// Note that the returned key is RAII-wrapped, so *don't* call ::RegCloseKey() on it!
// Let just RegKey's destructor close the key.
RegKey CreateKey(HKEY hKey, const std::wstring& subKeyName,
    DWORD options = 0, REGSAM accessRights = KEY_WRITE | KEY_READ,
    LPSECURITY_ATTRIBUTES securityAttributes = nullptr,
    LPDWORD disposition = nullptr);

// Returns names of sub-keys in the current key. 
std::vector<std::wstring> EnumerateSubKeyNames(HKEY hKey);

// Returns value names under the given open key.
std::vector<std::wstring> EnumerateValueNames(HKEY hKey);

// Reads a value from the registry.
// Throws std::invalid_argument is the value type is unsupported.
// Wraps ::RegQueryValueEx().
RegValue QueryValue(HKEY hKey, const std::wstring& valueName);

// Writes/updates a value in the registry.
// Wraps ::RegSetValueEx().
void SetValue(HKEY hKey, const std::wstring& valueName, const RegValue& value);

// Deletes a value from the registry.
void DeleteValue(HKEY hKey, const std::wstring& valueName);

// Deletes a sub-key and its values from the registry.
// Wraps ::RegDeleteKeyEx().
void DeleteKey(HKEY hKey, const std::wstring& subKey, REGSAM view = KEY_WOW64_64KEY);

// Creates a sub-key under HKEY_USERS or HKEY_LOCAL_MACHINE and loads the data 
// from the specified registry hive into that sub-key.
// Wraps ::RegLoadKey().
void LoadKey(HKEY hKey, const std::wstring& subKey, const std::wstring& filename);

// Saves the specified key and all of its sub-keys and values to a new file, in the standard format.
// Wraps ::RegSaveKey().
void SaveKey(HKEY hKey, const std::wstring& filename, LPSECURITY_ATTRIBUTES security = nullptr);

// Establishes a connection to a predefined registry key on another computer.
// Wraps ::RegConnectRegistry().
RegKey ConnectRegistry(const std::wstring& machineName, HKEY hKey);

// Expands environment-variable strings and replaces them with the values 
// defined for the current user.
// Wraps ::ExpandEnvironmentStrings().
std::wstring ExpandEnvironmentStrings(const std::wstring& source);

// Converts a registry value type (e.g. REG_SZ) to the corresponding string.
std::wstring ValueTypeIdToString(DWORD typeId);



//==============================================================================
//                          Inline Implementations
//==============================================================================


//------------------------------------------------------------------------------
//                      RegKey Inline Implementation
//------------------------------------------------------------------------------

inline RegKey::RegKey() noexcept
    : m_hKey(nullptr)
{}


inline RegKey::RegKey(HKEY hKey) noexcept
    : m_hKey(hKey)
{}


inline RegKey::RegKey(RegKey&& other) noexcept
    : m_hKey(other.m_hKey)
{
    other.m_hKey = nullptr;
}


inline RegKey& RegKey::operator=(RegKey&& other) noexcept
{
    if (&other != this)
    {
        Close();

        m_hKey = other.m_hKey;
        other.m_hKey = nullptr;
    }
    return *this;
}


inline RegKey::~RegKey() noexcept
{
    Close();
}


inline HKEY RegKey::Get() const noexcept
{
    return m_hKey;
}


inline void RegKey::Close() noexcept
{
    if (m_hKey != nullptr)
    {
        GD_WINREG_VERIFY(::RegCloseKey(m_hKey) == ERROR_SUCCESS);
    }
    m_hKey = nullptr;
}


inline bool RegKey::IsValid() const noexcept
{
    return m_hKey != nullptr;
}


inline HKEY RegKey::Detach() noexcept
{
    HKEY hKey = m_hKey;
    m_hKey = nullptr;
    return hKey;
}


inline void RegKey::Attach(HKEY hKey) noexcept
{
    // Release current key
    Close();

    m_hKey = hKey;
}


inline void RegKey::Swap(RegKey& other) noexcept
{
    using std::swap;
    swap(m_hKey, other.m_hKey);
}


inline void swap(RegKey& lhs, RegKey& rhs) noexcept
{
    lhs.Swap(rhs);
}


//------------------------------------------------------------------------------
//                  RegException Inline Implementation
//------------------------------------------------------------------------------

inline RegException::RegException(const char* msg, LONG errorCode)
    : std::runtime_error(msg)
    , m_errorCode(errorCode)
{}


inline RegException::RegException(const std::string& msg, LONG errorCode)
    : std::runtime_error(msg)
    , m_errorCode(errorCode)
{}


inline LONG RegException::ErrorCode() const noexcept 
{ 
    return m_errorCode; 
}


//------------------------------------------------------------------------------
//                      RegValue Inline Implementation
//------------------------------------------------------------------------------

inline RegValue::RegValue(TypeId typeId)
    : m_typeId(typeId)
{
}


inline RegValue::TypeId RegValue::GetType() const
{
    return m_typeId;
}


inline bool RegValue::IsEmpty() const
{
    return m_typeId == REG_NONE;
}


inline void RegValue::Reset(TypeId type)
{
    ResetValues();
    m_typeId = type;
}


inline DWORD RegValue::Dword() const
{
    GD_WINREG_ASSERT(m_typeId == REG_DWORD);
    if (m_typeId != REG_DWORD)
    {
        throw std::invalid_argument("RegValue::Dword() called on a non-DWORD registry value.");
    }

    return m_dword;
}


inline const std::wstring & RegValue::String() const
{
    GD_WINREG_ASSERT(m_typeId == REG_SZ);
    if (m_typeId != REG_SZ)
    {
        throw std::invalid_argument("RegValue::String() called on a non-REG_SZ registry value.");
    }

    return m_string;
}


inline const std::wstring & RegValue::ExpandString() const
{
    GD_WINREG_ASSERT(m_typeId == REG_EXPAND_SZ);
    if (m_typeId != REG_EXPAND_SZ)
    {
        throw std::invalid_argument(
            "RegValue::ExpandString() called on a non-REG_EXPAND_SZ registry value.");
    }

    return m_expandString;
}


inline const std::vector<std::wstring> & RegValue::MultiString() const
{
    GD_WINREG_ASSERT(m_typeId == REG_MULTI_SZ);
    if (m_typeId != REG_MULTI_SZ)
    {
        throw std::invalid_argument(
            "RegValue::MultiString() called on a non-REG_MULTI_SZ registry value.");
    }

    return m_multiString;
}


inline const std::vector<BYTE> & RegValue::Binary() const
{
    GD_WINREG_ASSERT(m_typeId == REG_BINARY);
    if (m_typeId != REG_BINARY)
    {
        throw std::invalid_argument(
            "RegValue::Binary() called on a non-REG_BINARY registry value.");
    }

    return m_binary;
}


inline DWORD & RegValue::Dword()
{
    GD_WINREG_ASSERT(m_typeId == REG_DWORD);
    if (m_typeId != REG_DWORD)
    {
        throw std::invalid_argument("RegValue::Dword() called on a non-DWORD registry value.");
    }

    return m_dword;
}


inline std::wstring & RegValue::String()
{
    GD_WINREG_ASSERT(m_typeId == REG_SZ);
    if (m_typeId != REG_SZ)
    {
        throw std::invalid_argument("RegValue::String() called on a non-REG_SZ registry value.");
    }

    return m_string;
}


inline std::wstring & RegValue::ExpandString()
{
    GD_WINREG_ASSERT(m_typeId == REG_EXPAND_SZ);
    if (m_typeId != REG_EXPAND_SZ)
    {
        throw std::invalid_argument(
            "RegValue::ExpandString() called on a non-REG_EXPAND_SZ registry value.");
    }

    return m_expandString;
}


inline std::vector<std::wstring> & RegValue::MultiString()
{
    GD_WINREG_ASSERT(m_typeId == REG_MULTI_SZ);
    if (m_typeId != REG_MULTI_SZ)
    {
        throw std::invalid_argument(
            "RegValue::MultiString() called on a non-REG_MULTI_SZ registry value.");
    }

    return m_multiString;
}


inline std::vector<BYTE> & RegValue::Binary()
{
    GD_WINREG_ASSERT(m_typeId == REG_BINARY);
    if (m_typeId != REG_BINARY)
    {
        throw std::invalid_argument(
            "RegValue::Binary() called on a non-REG_BINARY registry value.");
    }

    return m_binary;
}


inline void RegValue::ResetValues()
{
    m_dword = 0;
    m_string.clear();
    m_expandString.clear();
    m_multiString.clear();
    m_binary.clear();
}


} // namespace winreg 


#endif // GIOVANNI_DICANIO_WINREG_HPP_INCLUDED

