#pragma once
#include <windows.h>
#include "nativeloader.h"

extern HINSTANCE g_hinst;
extern ULONGLONG g_start_tick;

int exe_dir(wchar_t *buf, size_t cch);

int load_genuine_version(void);

void console_open(const wchar_t *title);
void console_write(const wchar_t *prefix, const char *msg);
void nl_log(const char *fmt, ...);

void bridge_register_builtins(void);
void bridge_start(void);
