#include <string>

#ifdef WIN32
void saveStringToFile(const wchar_t* fileName, const void* buf, size_t len);
void loadStringFromFile(std::string& buf, const wchar_t* fileName);
void chdirToFile(const wchar_t* fileName);
std::wstring replaceExtension(const std::wstring& fileName, const char* ext);

typedef wchar_t syschar_t;
typedef std::wstring sysstring_t;
#define _T(S) L##S
#else
typedef char syschar_t;
typedef std::string sysstring_t;
#define _T(S) S
#endif

void saveStringToFile(const char* fileName, const void* buf, size_t len);
void loadStringFromFile(std::string& buf, const char* fileName);
void chdirToFile(const char* fileName);
std::string replaceExtension(const std::string& fileName, const char* ext);

