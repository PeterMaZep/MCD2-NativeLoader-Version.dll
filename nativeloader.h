#pragma once
#include <windows.h>
#include <wchar.h>

typedef void (__cdecl *NativeLoaderLog_t)(const char *mod, const char *message);
typedef const char *(__cdecl *NativeBridgeFn)(const char *input);
typedef int (__cdecl *NativeLoaderRegister_t)(const char *mod, const char *name, NativeBridgeFn fn);

static __inline HMODULE NativeLoaderModule(void) {
  wchar_t path[MAX_PATH];
  DWORD n = GetModuleFileNameW(NULL, path, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return NULL;
  wchar_t *slash = wcsrchr(path, L'\\');
  if (wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - path), L"version.dll") != 0) return NULL;
  return GetModuleHandleW(path);
}

static __inline NativeLoaderLog_t NativeLoaderGetLog(void) {
  HMODULE h = NativeLoaderModule();
  return h ? (NativeLoaderLog_t)GetProcAddress(h, "NativeLoaderLog") : NULL;
}

static __inline NativeLoaderRegister_t NativeLoaderGetRegister(void) {
  HMODULE h = NativeLoaderModule();
  return h ? (NativeLoaderRegister_t)GetProcAddress(h, "NativeLoaderRegister") : NULL;
}
