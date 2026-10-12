#include <windows.h>
#include <wchar.h>
#include "version_exports.h"
#include "loader.h"

#define VERSION_NAME(name, ordinal) #name,
#define VERSION_EXPORT(name, ordinal) __pragma(comment(linker, "/EXPORT:" #name ",@" #ordinal))

static const char *const VERSION_NAMES[] = { VERSION_EXPORTS(VERSION_NAME) };
VERSION_EXPORTS(VERSION_EXPORT)

void *g_version[ARRAYSIZE(VERSION_NAMES)];
static int g_loaded = 0;

int load_genuine_version(void) {
  wchar_t path[MAX_PATH];
  GetSystemDirectoryW(path, MAX_PATH);
  wcscat_s(path, MAX_PATH, L"\\version.dll");
  HMODULE h = LoadLibraryExW(path, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!h) return 0;
  for (int i = 0; i < ARRAYSIZE(VERSION_NAMES); i++) g_version[i] = (void *)GetProcAddress(h, VERSION_NAMES[i]);
  g_loaded = 1;
  return 1;
}

// Called by the assembly stubs if the array is empty
__declspec(dllexport) void __cdecl init_version(void) {
    if (g_loaded) return;
    load_genuine_version();
}