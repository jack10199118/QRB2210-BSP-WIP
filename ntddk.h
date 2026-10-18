/* MINIMAL STUB of the WDK API surface our sources use. For syntax/consistency checks
 * only (gcc -fsyntax-only). Prototypes are from memory; the real WDK is the authority. */
#ifndef STUB_NTDDK_H
#define STUB_NTDDK_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef int32_t NTSTATUS; typedef uint8_t UCHAR, *PUCHAR; typedef uint16_t USHORT, *PUSHORT;
typedef uint32_t ULONG, *PULONG; typedef uint64_t ULONG64; typedef int64_t LONGLONG;
typedef int BOOLEAN; typedef void *PVOID, VOID_T; typedef wchar_t *PWSTR; typedef size_t SIZE_T;
#define VOID void
#define TRUE 1
#define FALSE 0
#define NULL_ ((void*)0)
#define _In_
#define _Out_
#define _Inout_
#define NT_SUCCESS(s) ((s) >= 0)
#define UNREFERENCED_PARAMETER(p) ((void)(p))
#define FORCEINLINE inline
#define PAGE_SIZE 4096
#define PAGE_READWRITE 4
#define PAGE_NOCACHE 0x200
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define STATUS_SUCCESS 0
#define STATUS_INSUFFICIENT_RESOURCES ((NTSTATUS)0xC000009A)
#define STATUS_DEVICE_CONFIGURATION_ERROR ((NTSTATUS)0xC0000182)
#define STATUS_IO_TIMEOUT ((NTSTATUS)0xC00000B5)
#define STATUS_CRC_ERROR ((NTSTATUS)0xC000003F)
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xC000000D)
#define STATUS_IO_DEVICE_ERROR ((NTSTATUS)0xC0000185)
#define STATUS_DEVICE_NOT_READY ((NTSTATUS)0xC00000A3)
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023)
#define STATUS_ACCESS_DENIED ((NTSTATUS)0xC0000022)
#define STATUS_NOT_IMPLEMENTED ((NTSTATUS)0xC0000002)
#define STATUS_OBJECT_NAME_NOT_FOUND ((NTSTATUS)0xC0000034)
#define STATUS_DEVICE_BUSY ((NTSTATUS)0x80000011)
#define STATUS_INVALID_DEVICE_STATE ((NTSTATUS)0xC0000184)
#define STATUS_INVALID_DEVICE_REQUEST ((NTSTATUS)0xC0000010)
#define KdPrint(x) ((void)0)
#define FILE_DEVICE_UNKNOWN 0x22
#define METHOD_BUFFERED 0
#define FILE_ANY_ACCESS 0
#define CTL_CODE(t,f,m,a) (((t)<<16)|((a)<<14)|((f)<<2)|(m))
typedef struct _GUID { ULONG a; USHORT b, c; UCHAR d[8]; } GUID;
#define DEFINE_GUID(n,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) static const GUID n = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}
typedef union { struct { ULONG LowPart; LONGLONG HighPart_; }; LONGLONG QuadPart; } LARGE_INTEGER, PHYSICAL_ADDRESS;
typedef struct { USHORT Length, MaximumLength; PWSTR Buffer; } UNICODE_STRING;
#define DECLARE_CONST_UNICODE_STRING(n, s) static const UNICODE_STRING n = {0, 0, 0}
typedef struct _DRIVER_OBJECT DRIVER_OBJECT, *PDRIVER_OBJECT;
typedef UNICODE_STRING *PUNICODE_STRING;
typedef enum { KernelMode } KPROCESSOR_MODE;
typedef NTSTATUS DRIVER_INITIALIZE(PDRIVER_OBJECT, PUNICODE_STRING);
/* resources */
#define CmResourceTypeMemory 3
#define CmResourceTypeInterrupt 2
typedef struct { UCHAR Type; union { struct { PHYSICAL_ADDRESS Start; ULONG Length; } Memory;
                                      struct { ULONG Level, Vector; ULONG64 Affinity; } Interrupt; } u; } CM_PARTIAL_RESOURCE_DESCRIPTOR, *PCM_PARTIAL_RESOURCE_DESCRIPTOR;
/* registers */
UCHAR READ_REGISTER_UCHAR(PUCHAR); USHORT READ_REGISTER_USHORT(PUSHORT); ULONG READ_REGISTER_ULONG(PULONG);
void WRITE_REGISTER_UCHAR(PUCHAR, UCHAR); void WRITE_REGISTER_USHORT(PUSHORT, USHORT); void WRITE_REGISTER_ULONG(PULONG, ULONG);
PVOID MmMapIoSpaceEx(PHYSICAL_ADDRESS, SIZE_T, ULONG); void MmUnmapIoSpace(PVOID, SIZE_T);
void KeStallExecutionProcessor(ULONG); NTSTATUS KeDelayExecutionThread(KPROCESSOR_MODE, BOOLEAN, LARGE_INTEGER *);
NTSTATUS IoGetDeviceInterfaces(const GUID *, PVOID, ULONG, PWSTR *); void ExFreePool(PVOID);
void RtlInitUnicodeString(PUNICODE_STRING, PWSTR); void RtlZeroMemory(void *, size_t);
#define RtlZeroMemory(d,l) memset((d),0,(l))
#define RtlCopyMemory(d,s,l) memcpy((d),(s),(l))
#endif
