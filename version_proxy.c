#include <windows.h>
#include <string.h>
#include <wchar.h>
#include "version_exports.h"
#include "loader.h"

#define VERSION_NAME(name, ordinal) #name,
#define VERSION_EXPORT(name, ordinal) __pragma(comment(linker, "/EXPORT:" #name ",@" #ordinal))

static const char *const VERSION_NAMES[] = { VERSION_EXPORTS(VERSION_NAME) };
VERSION_EXPORTS(VERSION_EXPORT)

void *g_version[ARRAYSIZE(VERSION_NAMES)];

int load_genuine_version(void) {
  wchar_t path[MAX_PATH];
  GetSystemDirectoryW(path, MAX_PATH);
  wcscat_s(path, MAX_PATH, L"\\version.dll");
  HMODULE h = LoadLibraryExW(path, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!h) return 0;
  for (int i = 0; i < ARRAYSIZE(VERSION_NAMES); i++) g_version[i] = (void *)GetProcAddress(h, VERSION_NAMES[i]);
  return 1;
}

typedef UINT (WINAPI *PeriodFn)(UINT);
typedef DWORD (WINAPI *TimeFn)(void);
typedef UINT (WINAPI *CapsFn)(void *, UINT);

static void (*g_on_start)(void);
static volatile LONG g_started = 0;
static PeriodFn g_real_begin;
static TimeFn g_real_time;
static CapsFn g_real_caps;

static void fire(void) {
  if (!InterlockedExchange(&g_started, 1)) g_on_start();
}

static UINT WINAPI deferred_begin(UINT period) {
  fire();
  return g_real_begin(period);
}

static UINT WINAPI deferred_caps(void *caps, UINT size) {
  fire();
  return g_real_caps(caps, size);
}

static DWORD WINAPI deferred_time(void) {
  if (!g_started && GetTickCount64() - g_start_tick > 5000) fire();
  return g_real_time();
}

static int slot(const char *name) {
  for (int i = 0; i < ARRAYSIZE(VERSION_NAMES); i++) {
    if (strcmp(VERSION_NAMES[i], name) == 0) return i;
  }
  return -1;
}

void defer_until_game_starts(void (*start)(void)) {
  int begin = slot("timeBeginPeriod"), time = slot("timeGetTime"), caps = slot("timeGetDevCaps");
  g_on_start = start;
  if (begin < 0 || time < 0 || caps < 0 || !g_version[begin] || !g_version[time] || !g_version[caps]) {
    fire();
    return;
  }
  g_real_begin = (PeriodFn)g_version[begin];
  g_real_time = (TimeFn)g_version[time];
  g_real_caps = (CapsFn)g_version[caps];
  g_version[begin] = (void *)deferred_begin;
  g_version[time] = (void *)deferred_time;
  g_version[caps] = (void *)deferred_caps;
}