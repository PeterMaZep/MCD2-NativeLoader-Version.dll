#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "loader.h"

typedef struct {
  wchar_t name[MAX_PATH];
  wchar_t dll[MAX_PATH];
  int console;
} Mod;

HINSTANCE g_hinst = NULL;
ULONGLONG g_start_tick = 0;

static int g_genuine_ok = 0;
static Mod *g_mods = NULL;
static int g_mod_count = 0;
static int g_console_mods = 0;
static const wchar_t *MOD_FOLDERS[] = { L"~mods", L"mods" };

int exe_dir(wchar_t *buf, size_t cch) {
  DWORD n = GetModuleFileNameW(NULL, buf, (DWORD)cch);
  if (n == 0 || n >= cch) return 0;
  wcsrchr(buf, L'\\')[1] = L'\0';
  return 1;
}

static int mods_dir(const wchar_t *exedir, const wchar_t *folder, wchar_t *out, DWORD cch) {
  wchar_t rel[MAX_PATH];
  if (_snwprintf_s(rel, MAX_PATH, _TRUNCATE, L"%ls..\\..\\Content\\Paks\\%ls", exedir, folder) < 0) return 0;
  DWORD n = GetFullPathNameW(rel, cch, out, NULL);
  return n != 0 && n < cch;
}

static int file_exists(const wchar_t *path) {
  DWORD attr = GetFileAttributesW(path);
  return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

static int known_mod(const wchar_t *name) {
  for (int i = 0; i < g_mod_count; i++) {
    if (CompareStringOrdinal(g_mods[i].name, -1, name, -1, TRUE) == CSTR_EQUAL) return 1;
  }
  return 0;
}

static void scan_mods(const wchar_t *mods) {
  wchar_t pattern[MAX_PATH];
  if (_snwprintf_s(pattern, MAX_PATH, _TRUNCATE, L"%ls\\*", mods) < 0) return;

  WIN32_FIND_DATAW fd;
  HANDLE h = FindFirstFileW(pattern, &fd);
  if (h == INVALID_HANDLE_VALUE) return;

  do {
    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
    if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;

    wchar_t dll[MAX_PATH];
    wchar_t marker[MAX_PATH];
    if (_snwprintf_s(dll, MAX_PATH, _TRUNCATE, L"%ls\\%ls\\%ls.dll", mods, fd.cFileName, fd.cFileName) < 0 || _snwprintf_s(marker, MAX_PATH, _TRUNCATE, L"%ls\\%ls\\console", mods, fd.cFileName) < 0) continue;
    if (!file_exists(dll) || known_mod(fd.cFileName)) continue;

    g_mods = realloc(g_mods, (g_mod_count + 1) * sizeof(Mod));
    Mod *m = &g_mods[g_mod_count++];
    wcscpy_s(m->name, MAX_PATH, fd.cFileName);
    wcscpy_s(m->dll, MAX_PATH, dll);
    m->console = file_exists(marker);
    if (m->console) g_console_mods++;
  } while (FindNextFileW(h, &fd));
  FindClose(h);
}

static void open_console(void) {
  const wchar_t *title = L"NativeLoader Console";
  if (g_console_mods == 1) {
    for (int i = 0; i < g_mod_count; i++) {
      if (g_mods[i].console) title = g_mods[i].name;
    }
  }
  console_open(title);
}

static void load_mods(void) {
  char name[MAX_PATH * 4];
  int loaded = 0;
  for (int i = 0; i < g_mod_count; i++) {
    Mod *m = &g_mods[i];
    HMODULE h = LoadLibraryExW(m->dll, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    DWORD err = GetLastError();
    WideCharToMultiByte(CP_UTF8, 0, m->name, -1, name, sizeof(name), NULL, NULL);
    if (h) loaded++;
    if (!m->console) continue;
    if (h) nl_log("loaded %s", name);
    else nl_log("FAILED to load %s, err=%lu", name, (unsigned long)err);
  }
  nl_log("%d of %d DLL mods loaded", loaded, g_mod_count);
}

static Mod *console_mod(const char *mod) {
  wchar_t name[MAX_PATH];
  if (!mod || !MultiByteToWideChar(CP_UTF8, 0, mod, -1, name, MAX_PATH)) return NULL;
  for (int i = 0; i < g_mod_count; i++) {
    if (g_mods[i].console && CompareStringOrdinal(g_mods[i].name, -1, name, -1, TRUE) == CSTR_EQUAL) return &g_mods[i];
  }
  return NULL;
}

void nl_mod_log(const char *mod, const char *fmt, ...) {
  if (!console_mod(mod)) return;
  char msg[1024];
  va_list ap;
  va_start(ap, fmt);
  _vsnprintf_s(msg, sizeof(msg), _TRUNCATE, fmt, ap);
  va_end(ap);
  nl_log("%s", msg);
}

__declspec(dllexport) void __cdecl NativeLoaderLog(const char *mod, const char *message) {
  if (!message) return;
  Mod *m = console_mod(mod);
  if (m) console_write(g_console_mods == 1 ? NULL : m->name, message);
}

static DWORD WINAPI startup_worker(LPVOID param) {
  (void)param;
  
  // MOVED HERE: Load the real version.dll safely outside of DllMain
  g_genuine_ok = load_genuine_version(); 

  wchar_t dir[MAX_PATH];
  wchar_t mods[MAX_PATH];
  if (!exe_dir(dir, MAX_PATH)) return 0;
  for (int i = 0; i < ARRAYSIZE(MOD_FOLDERS); i++) {
    if (mods_dir(dir, MOD_FOLDERS[i], mods, MAX_PATH)) scan_mods(mods);
  }
  if (g_console_mods > 0) open_console();
  if (!g_genuine_ok) nl_log("genuine System32 version.dll FAILED to load");
  bridge_register_builtins();
  load_mods();
  bridge_start();
  return 0;
}

static void start_worker(void) {
  HANDLE t = CreateThread(NULL, 0, startup_worker, NULL, 0, NULL);
  if (t) CloseHandle(t);
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved) {
  (void)reserved;
  if (reason == DLL_PROCESS_ATTACH) {

    // Pin the DLL in memory so the game can't unload it
    HMODULE hm;
    GetModuleHandleExW(
      GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
      (LPCWSTR)&DllMain,
      &hm
    );
    
    g_hinst = hinst;
    g_start_tick = GetTickCount64();
    DisableThreadLibraryCalls(hinst);
    g_genuine_ok = load_genuine_winmm();
    defer_until_game_starts(start_worker);
  }
  return TRUE;
}