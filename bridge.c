#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "loader.h"
#include "engine.h"

#define SEP '|'
#define BUILTIN_COUNT 2
#define MAX_REG (128 + BUILTIN_COUNT)
#define DISCOVERY_TRIES 240
#define DISCOVERY_INTERVAL_MS 500
#define NATIVELOADER_VERSION "1.2"

typedef struct { char mod[64]; char name[64]; NativeBridgeFn fn; } Reg;

static Reg g_reg[MAX_REG];
static int g_reg_count;
static SRWLOCK g_reg_lock = SRWLOCK_INIT;
static NativeFn g_orig_cvar;

static int add_function(const char *mod, const char *name, NativeBridgeFn fn) {
  int ok = 0;
  AcquireSRWLockExclusive(&g_reg_lock);
  if (g_reg_count < MAX_REG) {
    strcpy_s(g_reg[g_reg_count].mod, sizeof(g_reg[0].mod), mod);
    strcpy_s(g_reg[g_reg_count].name, sizeof(g_reg[0].name), name);
    g_reg[g_reg_count].fn = fn;
    g_reg_count++;
    ok = 1;
  }
  ReleaseSRWLockExclusive(&g_reg_lock);
  return ok;
}

static const char *__cdecl builtin_ready(const char *input) {
  (void)input;
  return "1";
}

static const char *__cdecl builtin_version(const char *input) {
  (void)input;
  return NATIVELOADER_VERSION;
}

void bridge_register_builtins(void) {
  add_function("NativeLoader", "ready", builtin_ready);
  add_function("NativeLoader", "version", builtin_version);
}

__declspec(dllexport) int __cdecl NativeLoaderRegister(const char *mod, const char *name, NativeBridgeFn fn) {
  if (!mod || !name || !fn) return 0;
  if (strlen(mod) >= sizeof(g_reg[0].mod) || strlen(name) >= sizeof(g_reg[0].name)) {
    nl_mod_log(mod, "bridge: FAILED to register %s/%s, mod and function names must be under %d bytes", mod, name, (int)sizeof(g_reg[0].name));
    return 0;
  }
  if (_stricmp(mod, "NativeLoader") == 0) {
    nl_log("bridge: FAILED to register %s/%s, the NativeLoader name is reserved", mod, name);
    return 0;
  }
  int ok = add_function(mod, name, fn);
  if (ok) nl_mod_log(mod, "bridge: registered %s/%s", mod, name);
  else nl_mod_log(mod, "bridge: FAILED to register %s/%s, table full", mod, name);
  return ok;
}

static NativeBridgeFn lookup(const char *mod, const char *name) {
  NativeBridgeFn fn = NULL;
  AcquireSRWLockShared(&g_reg_lock);
  for (int i = 0; i < g_reg_count; i++) {
    if (strcmp(g_reg[i].mod, mod) == 0 && strcmp(g_reg[i].name, name) == 0) {
      fn = g_reg[i].fn;
      break;
    }
  }
  ReleaseSRWLockShared(&g_reg_lock);
  return fn;
}

static const char *dispatch(char *req) {
  char *mod = req;
  char *p = strchr(mod, SEP);
  if (!p) return NULL;
  *p = 0;
  char *name = p + 1;
  p = strchr(name, SEP);
  if (!p) return NULL;
  *p = 0;
  char *input = p + 1;
  NativeBridgeFn fn = lookup(mod, name);
  if (!fn) {
    nl_log("bridge: no handler for %s/%s", mod, name);
    return NULL;
  }
  return fn(input);
}

static void __cdecl thunk_cvar(void *ctx, void *frame, void *result) {
  uint8_t **pCode = (uint8_t **)((char *)frame + FRAME_CODE);
  void *object = *(void **)((char *)frame + FRAME_OBJECT);
  uint8_t *saved = *pCode;
  int handled = 0;
  __try {
    if (saved) {
      FStr arg = { 0, 0, 0 };
      uint8_t op = **pCode;
      (*pCode)++;
      ((NativeFn)g_gnatives[op])(object, frame, &arg);
      if (arg.data && arg.num > 3 && arg.data[0] == 'N' && arg.data[1] == 'L' && arg.data[2] == SEP) {
        int len = arg.num - 1;
        int cb = WideCharToMultiByte(CP_UTF8, 0, arg.data, len, NULL, 0, NULL, NULL);
        char *req = malloc((size_t)cb + 1);
        WideCharToMultiByte(CP_UTF8, 0, arg.data, len, req, cb, NULL, NULL);
        req[cb] = 0;
        const char *reply = dispatch(req + 3);
        if (!reply) reply = "";
        int rwlen = MultiByteToWideChar(CP_UTF8, 0, reply, -1, NULL, 0);
        if (**pCode == EX_ENDFUNCTIONPARMS) (*pCode)++;
        if (rwlen <= arg.max) {
          MultiByteToWideChar(CP_UTF8, 0, reply, -1, arg.data, arg.max);
          arg.num = rwlen;
          *(FStr *)result = arg;
          arg.data = NULL;
        } else {
          engine_make_fstring(reply, (FStr *)result);
        }
        handled = 1;
        free(req);
      }
      engine_free_fstring(&arg);
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    nl_log("bridge: call FAILED with exception 0x%lx", GetExceptionCode());
  }
  if (!handled) {
    *pCode = saved;
    g_orig_cvar(ctx, frame, result);
  }
}

static DWORD WINAPI bridge_thread(LPVOID param) {
  (void)param;
  engine_init();

  int found = 0;
  for (int tries = 0; tries < DISCOVERY_TRIES && !found; tries++) {
    found = engine_find_globals();
    if (!found) Sleep(DISCOVERY_INTERVAL_MS);
  }
  if (!found) {
    nl_log("bridge: FAILED to find the engine globals, bridge disabled");
    return 0;
  }

  uint64_t uf = 0;
  for (int tries = 0; tries < DISCOVERY_TRIES && !uf; tries++) {
    uf = engine_find_function("KismetSystemLibrary", "GetConsoleVariableStringValue");
    if (!uf) Sleep(DISCOVERY_INTERVAL_MS);
  }
  if (!uf) {
    nl_log("bridge: FAILED to find GetConsoleVariableStringValue, bridge disabled");
    return 0;
  }
  if (!engine_swap_func(uf, thunk_cvar, &g_orig_cvar)) {
    nl_log("bridge: FAILED to install the hook, bridge disabled");
    return 0;
  }
  int count = g_reg_count - BUILTIN_COUNT;
  nl_log("bridge: ready, %d mod %s registered", count, count == 1 ? "function" : "functions");
  return 0;
}

void bridge_start(void) {
  HANDLE t = CreateThread(NULL, 0, bridge_thread, NULL, 0, NULL);
  if (t) CloseHandle(t);
}
