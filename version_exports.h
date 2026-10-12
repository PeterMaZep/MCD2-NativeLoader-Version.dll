#pragma once

// Must stay in the same order as the stubs in version_exports.asm
#define VERSION_EXPORTS(X) \
  X(GetFileVersionInfoA, 1) \
  X(GetFileVersionInfoByHandle, 2) \
  X(GetFileVersionInfoExA, 3) \
  X(GetFileVersionInfoExW, 4) \
  X(GetFileVersionInfoSizeA, 5) \
  X(GetFileVersionInfoSizeExA, 6) \
  X(GetFileVersionInfoSizeExW, 7) \
  X(GetFileVersionInfoSizeW, 8) \
  X(GetFileVersionInfoW, 9) \
  X(VerFindFileA, 10) \
  X(VerFindFileW, 11) \
  X(VerInstallFileA, 12) \
  X(VerInstallFileW, 13) \
  X(VerLanguageNameA, 14) \
  X(VerLanguageNameW, 15) \
  X(VerQueryValueA, 16) \
  X(VerQueryValueW, 17)