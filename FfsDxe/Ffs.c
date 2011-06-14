/** @file

    ...

**/

#include <PiDxe.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/FirmwareVolume.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>

//
// Private data structures
//

// ...

typedef struct {
  // ...
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
