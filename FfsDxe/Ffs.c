/** @file

    ...

**/

#include <PiDxe.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/FirmwareVolume2.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>

//
// Private data structures
//

#define FILE_SYSTEM_PRIVATE_DATA_SIGNATURE SIGNATURE_32 ('f', 'f', 's', 't')

typedef struct {
  UINT32                          Signature;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL SimpleFileSystem;
  EFI_FIRMWARE_VOLUME2_PROTOCOL   *FirmwareVolume2;
} FILE_SYSTEM_PRIVATE_DATA;

// ...

//
// Protocol template
//

EFI_EVENT mFfsRegistration;

// ...

//
// Global functions
//

VOID
EFIAPI
FfsNotificationEvent (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
/**

  Hello.

  @param Event
  @param Context

**/
{
  return;
}

EFI_STATUS
EFIAPI
InitializeFfsFileSystem (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
/**

  Hello.

  @param ImageHandle
  @param SystemTable

**/
{
  return EFI_UNSUPPORTED;
}
