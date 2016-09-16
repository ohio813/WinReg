# WinReg v0.5
## C++ Wrappers around Windows Registry Win32 APIs

by Giovanni Dicanio

The Windows Registry C-interface API is very low-level and very hard to use.

I developed some **C++ wrappers** around this low-level Win32 API, to raise the semantic level, using C++ classes like `std::wstring`, `std::vector`, etc. instead of raw C-style buffers and low-level mechanisms. 

For example, the `REG_MULTI_SZ` registry type associated to double-NUL-terminated C-style strings is handled using a much easier higher-level `vector<wstring>`. My C++ code does the _translation_ between high-level C++ STL-based stuff and low-level Win32 C-interface APIs.

**NOTE**: I did a _few_ tests, according to which the code seems to work, but _more_ tests are required. So please consider current library status kind of a _beta_ version! Moreover, code and comments may require some further adjustments.

Being very busy right now, I preferred releasing this library on GitHub in current status; feedback, bug-fixes, etc. are welcome.

I developed this code using **Visual Studio 2005 with Update 3**.

The library's code is split between a `WinReg.hpp` header (containing declarations and some inline implementations), and the `WinReg.cpp` source file with implementation code.

`WinRegTest.cpp` contains some demo/test code for the library: check it out for some sample usage.

The library exposes three main classes:

* `RegKey`: a wrapper around raw Win32 `HKEY` handles
* `RegValue`: a variant-style class representing registry values (currently supported registry types are: `REG_DWORD`, `REG_SZ`, `REG_EXPAND_SZ`, `REG_MULTI_SZ`, `REG_BINARY`; see the `WinReg.hpp` header for more details)
* `RegException`: exception class to signal error conditions

In addition, there are various functions that wrap raw Win32 registry APIs.

The library stuff lives under the `winreg` namespace.

