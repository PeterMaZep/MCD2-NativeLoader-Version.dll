EXTERN g_version:QWORD

slot = 0

stub MACRO name
PUBLIC name
name PROC
  jmp qword ptr [g_version + 8 * slot]
name ENDP
slot = slot + 1
ENDM

.code
stub GetFileVersionInfoA
stub GetFileVersionInfoByHandle
stub GetFileVersionInfoExA
stub GetFileVersionInfoExW
stub GetFileVersionInfoSizeA
stub GetFileVersionInfoSizeExA
stub GetFileVersionInfoSizeExW
stub GetFileVersionInfoSizeW
stub GetFileVersionInfoW
stub VerFindFileA
stub VerFindFileW
stub VerInstallFileA
stub VerInstallFileW
stub VerLanguageNameA
stub VerLanguageNameW
stub VerQueryValueA
stub VerQueryValueW

END