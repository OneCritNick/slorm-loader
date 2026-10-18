#include <windows.h>
extern "C" __declspec(dllexport) int SlormCoreProbe(){return 73;}
BOOL WINAPI DllMain(HINSTANCE,DWORD,void*){return TRUE;}
