/*

*/

#include "Ffs.h"

/**
  The user Entry Point for module Ffs. The user code starts with this function.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.  
  @param[in] SystemTable    A pointer to the EFI System Table.
  
  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
InitializeFfs(
  IN EFI_HANDLE           ImageHandle,
  IN EFI_SYSTEM_TABLE     *SystemTable
  )
{
  EFI_STATUS Status;

  Status = EfiLibInstallAllDriverProtocols(ImageHandle,
                                           SystemTable,
                                           &gFfsDriverBinding,
                                           ImageHandle,
                                           &gFfsComponentName,
                                           NULL,
                                           NULL);

  ASSERT_EFI_ERROR(Status);
  DEBUG((EFI_D_INFO, "TESTING 1 2 3......\n"));

  return Status;
}
