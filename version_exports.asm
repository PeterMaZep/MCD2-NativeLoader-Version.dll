EXTERN g_version:QWORD
EXTERN init_version:PROC

slot = 0

stub MACRO name
PUBLIC name
name PROC
  cmp qword ptr [g_version + 8 * slot], 0
  jne @F
  
  ; Save volatile registers (RCX, RDX, R8, R9)
  push rcx
  push rdx
  push r8
  push r9
  
  ; Allocate shadow space (32 bytes) + 8 bytes to align to 16-byte boundary
  sub rsp, 28h
  
  call init_version
  
  ; Restore stack and volatile registers
  add rsp, 28h
  pop r9
  pop r8
  pop rdx
  pop rcx

@@:
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