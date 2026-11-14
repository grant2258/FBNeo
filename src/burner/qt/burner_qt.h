#ifndef _BURNER_QT_H
#define _BURNER_QT_H

typedef unsigned char BYTE;
/*
typedef unsigned short WORD;
*/
#include <stdint.h>
typedef uint32_t DWORD;

extern int bDrvOkay;
extern int bRunPause;
extern bool bAlwaysProcessKeyboardInput;
extern int nAppVirtualFps;

// main.cpp
extern bool AppProcessKeyboardInput();
extern void InpDIPSWResetDIPs (void);
extern void IpsApplyPatches(UINT8 *, char *, UINT32, bool);
extern void IpsPatchInit();
extern void Reinitialise(void);
extern TCHAR *GetIsoPath();
extern int VidRecalcPal();
extern void wav_pause(bool bResume);

extern TCHAR szAppRomPaths[DIRS_MAX][MAX_PATH];
// drv.cpp
int DrvInit(int nDrvNum, bool bRestore);
int DrvInitCallback();
int DrvExit();

// progress.cpp
int ProgressUpdateBurner(double dProgress, const TCHAR* pszText, bool bAbs);
void ProgressCreate();
void ProgressDestroy();
bool ProgressIsRunning();

#ifdef _MSC_VER
#define snprintf _snprintf
#define ANSIToTCHAR(str, foo, bar) (str)
#endif

extern char *TCHARToANSI(const TCHAR* pszInString, char* pszOutString, int nOutSize);


class StringSet {
public:
    TCHAR* szText;
    int nLen;
    // printf function to add text to the Bzip string
    int __cdecl Add(TCHAR* szFormat, ...);
    int Reset();
    StringSet();
    ~StringSet();
};

extern StringSet BzipText;
extern StringSet BzipDetail;

#endif

#ifndef _TtoA
#define _TtoA(a) (a)
#endif
#ifndef _AtoT
#define _AtoT(a) (a)
#endif

#include <string.h>

#ifndef _tcsrchr
#define _tcsrchr strrchr
#endif
#ifndef _tcschr
#define _tcschr strchr
#endif
#ifndef _tcsstr
#define _tcsstr strstr
#endif
#ifndef _tcscpy
#define _tcscpy strcpy
#endif
#ifndef _tcsncpy
#define _tcsncpy strncpy
#endif
#ifndef _tcscat
#define _tcscat strcat
#endif
#ifndef _tcslen
#define _tcslen strlen
#endif
#ifndef _tcscmp
#define _tcscmp strcmp
#endif
#ifndef _tcsicmp
#define _tcsicmp strcasecmp
#endif
#ifndef _tcsnicmp
#define _tcsnicmp strncasecmp
#endif

#ifndef stricmp
#define stricmp strcasecmp
#endif
#ifndef strnicmp
#define strnicmp strncasecmp
#endif

#include <ctype.h>

#ifndef _totlower
#define _totlower tolower
#endif
#ifndef _totupper
#define _totupper toupper
#endif

#ifndef dprintf
#ifdef FBA_DEBUG
#define dprintf printf
#else
#define dprintf(...) do {} while (0)
#endif
#endif
