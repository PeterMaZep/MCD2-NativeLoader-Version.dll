#include <windows.h>
#include <string.h>
#include <wchar.h>
#include "winmm_exports.h"
#include "loader.h"

#define WINMM_NAME(name, ordinal) #name,
#define WINMM_EXPORT(name, ordinal) __pragma(comment(linker, "/EXPORT:" #name ",@" #ordinal))

static const char *const WINMM_NAMES[] = { WINMM_EXPORTS(WINMM_NAME) };
WINMM_EXPORTS(WINMM_EXPORT)

void *g_winmm[ARRAYSIZE(WINMM_NAMES)];

int load_genuine_winmm(void) {
  wchar_t path[MAX_PATH];
  GetSystemDirectoryW(path, MAX_PATH);
  wcscat_s(path, MAX_PATH, L"\\winmm.dll");
  HMODULE h = LoadLibraryExW(path, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!h) return 0;
  for (int i = 0; i < ARRAYSIZE(WINMM_NAMES); i++) g_winmm[i] = (void *)GetProcAddress(h, WINMM_NAMES[i]);
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
  for (int i = 0; i < ARRAYSIZE(WINMM_NAMES); i++) {
    if (strcmp(WINMM_NAMES[i], name) == 0) return i;
  }
  return -1;
}

void defer_until_game_starts(void (*start)(void)) {
  int begin = slot("timeBeginPeriod"), time = slot("timeGetTime"), caps = slot("timeGetDevCaps");
  g_on_start = start;
  if (begin < 0 || time < 0 || caps < 0 || !g_winmm[begin] || !g_winmm[time] || !g_winmm[caps]) {
    fire();
    return;
  }
  g_real_begin = (PeriodFn)g_winmm[begin];
  g_real_time = (TimeFn)g_winmm[time];
  g_real_caps = (CapsFn)g_winmm[caps];
  g_winmm[begin] = (void *)deferred_begin;
  g_winmm[time] = (void *)deferred_time;
  g_winmm[caps] = (void *)deferred_caps;
}
