/** @file

Copyright 2011 Colin Drake. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
EVENT SHALL <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of Colin Drake.

**/

#include "Ffs.h"

//
// Protocol templates and module-scope variables
//

EFI_EVENT mFfsRegistration;

FILE_SYSTEM_PRIVATE_DATA mFileSystemPrivateDataTemplate = {
  FILE_SYSTEM_PRIVATE_DATA_SIGNATURE,
  {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_REVISION,
    FfsOpenVolume
  },
  NULL
};

FILE_PRIVATE_DATA mFilePrivateDataTemplate = {
  FILE_PRIVATE_DATA_SIGNATURE,
  {
    EFI_FILE_PROTOCOL_REVISION,
    FfsOpen,
    FfsClose,
    FfsDelete,
    FfsRead,
    FfsWrite,
    FfsGetPosition,
    FfsSetPosition,
    FfsGetInfo,
    FfsSetInfo,
    FfsFlush 
  },
  NULL,
  NULL,
  FALSE,
  NULL,
  NULL
};

//
// Misc. helper methods
//

CHAR16 *
GuidToString (
  IN EFI_GUID Guid
  )
{
  CHAR16 *String;

  // Allocate a string to hold a 36-character GUID and NULL byte.
  String = AllocatePool (37);

  // Print the GUID into the string with the %g formatter.
  UnicodeSPrint (String, 37, L"%g", Guid);
  return String;
}

BOOLEAN
FvHasFile (
  IN EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv2,
  IN CHAR16                        *FileName
  )
{
  VOID                          *Key;
  EFI_FV_FILETYPE               FileType;
  EFI_GUID                      NameGuid, OldNameGuid;
  EFI_FV_FILE_ATTRIBUTES        FvAttributes;
  UINTN                         Size;
  BOOLEAN                       HasFile;

  HasFile = FALSE;
  Key = AllocateZeroPool (Fv2->KeySize);

  // Loop through all FV2 files.
  while (TRUE) {
    CHAR16 *GuidAsString;

    Fv2->GetNextFile (Fv2, 
                      Key,
                      &FileType,
                      &NameGuid,
                      &FvAttributes,
                      &Size);

    // Check exit condition.
    if (CompareGuid (&NameGuid, &OldNameGuid)) {
      break;
    }

    // Check for the file we're looking for.
    GuidAsString = GuidToString (NameGuid);

    if (StrCmp (GuidAsString, FileName) == 0) {
      HasFile = TRUE;
      break;
    }

    // Move on to next File.
    CopyGuid (&OldNameGuid, &NameGuid);
  }

  return HasFile;
}

//
// SimpleFileSystem and File protocol functions
//

EFI_STATUS
EFIAPI
FfsOpenVolume (
  IN EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *This,
  OUT EFI_FILE_PROTOCOL               **Root
  )
{
  EFI_STATUS                    Status;
  EFI_FILE_PROTOCOL             *File;
  FILE_SYSTEM_PRIVATE_DATA      *PrivateFileSystem;
  FILE_PRIVATE_DATA             *PrivateFile;
  DIR_INFO                      *RootInfo;
  
  Status = EFI_SUCCESS;
  DEBUG ((EFI_D_INFO, "FfsOpenVolume: Start\n"));

  // Get private structure for This.
  PrivateFileSystem = FILE_SYSTEM_PRIVATE_DATA_FROM_THIS (This);
  DEBUG ((EFI_D_INFO, "FfsOpenVolume: Grab private filesys struct\n"));

  // Allocate a new private FILE_PRIVATE_DATA instance.
  PrivateFile = NULL;

  PrivateFile = AllocateCopyPool (
              sizeof (FILE_PRIVATE_DATA),
              (VOID *) &mFilePrivateDataTemplate
              );

  if (PrivateFile == NULL) {
    DEBUG ((EFI_D_ERROR, "FfsOpenVolume: Couldn't allocate private file\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  DEBUG ((EFI_D_INFO, "FfsOpenVolume: Allocate private file struct\n"));

  // Fill out the rest of the private file data and assign it's File attribute
  // to Root.
  PrivateFile->FileSystem  = PrivateFileSystem;
  PrivateFile->FileName    = L"\\";
  PrivateFile->IsDirectory = TRUE;
  PrivateFile->FileInfo    = NULL;

  RootInfo = AllocateZeroPool (sizeof (DIR_INFO));
  InitializeListHead (&(RootInfo->Children));
  PrivateFile->DirInfo     = RootInfo;

  // Set outgoing parameters.
  File = &(PrivateFile->File);
  *Root = AllocateCopyPool (sizeof (EFI_FILE_PROTOCOL), (VOID *) File);

  DEBUG ((EFI_D_INFO, "FfsOpenVolume: End of func\n"));
  return Status;
}

EFI_STATUS
EFIAPI
FfsOpen (
  IN  EFI_FILE_PROTOCOL *This,
  OUT EFI_FILE_PROTOCOL **NewHandle,
  IN  CHAR16            *FileName,
  IN  UINT64            OpenMode,
  IN  UINT64            Attributes
  )
{
  EFI_STATUS        Status;
  FILE_PRIVATE_DATA *PrivateFile;

  Status = EFI_SUCCESS;
  DEBUG ((EFI_D_INFO, "FfsOpen: Start\n"));

  // Check for a valid OpenMode parameter. Since this is a read-only filesystem
  // it must not be EFI_FILE_MODE_WRITE or EFI_FILE_MODE_CREATE.
  if (OpenMode != EFI_FILE_MODE_READ) {
    DEBUG ((EFI_D_INFO, "FfsOpen: OpenMode must be Read\n"));
    return EFI_WRITE_PROTECTED;
  }

  DEBUG ((EFI_D_INFO, "OPENING : %s\n", FileName));

  PrivateFile = FILE_PRIVATE_DATA_FROM_THIS (This);

  // Check the filename that was specified to open.
  if (StrCmp (FileName, L".") == 0) {
    // Open the current directory.
    DEBUG ((EFI_D_INFO, "FfsOpen: Open self\n"));
  } else if (StrCmp (FileName, L"\\") == 0) {
    // Open the root directory.
    DEBUG ((EFI_D_INFO, "FfsOpen: Open root\n"));
  } else if (StrCmp (FileName, L"..") == 0) {
    // Open the parent directory.
    DEBUG ((EFI_D_INFO, "FfsOpen: Open parent\n"));
  } else {
    // Check for the filename on the FV2 volume.
    if (FvHasFile (PrivateFile->FileSystem->FirmwareVolume2, FileName)) {
      // Found file.
      DEBUG ((EFI_D_INFO, "FfsOpen: File found\n"));
    } else {
      // File not found.
      DEBUG ((EFI_D_INFO, "FfsOpen: File not found\n"));
      Status = EFI_NOT_FOUND;
    }
  }

  DEBUG ((EFI_D_INFO, "FfsOpen: End of func\n"));
  return Status;
}

EFI_STATUS
EFIAPI
FfsClose (IN EFI_FILE_PROTOCOL *This)
{
  DEBUG ((EFI_D_INFO, "*** FFSCLOSE ***\n"));
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FfsDelete (IN EFI_FILE_PROTOCOL *This)
{
  DEBUG ((EFI_D_INFO, "*** FFSDELETE ***\n"));
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FfsRead (
  IN EFI_FILE_PROTOCOL *This,
  IN OUT UINTN *BufferSize,
  OUT VOID *Buffer
  )
{
  DEBUG ((EFI_D_INFO, "*** FFSDELETE ***\n"));
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FfsWrite (
  IN EFI_FILE_PROTOCOL *This,
  IN OUT UINTN *BufferSize,
  IN VOID *Buffer
  )
{
  DEBUG ((EFI_D_INFO, "*** FFSDELETE ***\n"));
  return EFI_ACCESS_DENIED;
}

EFI_STATUS
EFIAPI
FfsGetPosition (
  IN EFI_FILE_PROTOCOL *This,
  OUT UINT64 *Position
  )
{
  DEBUG ((EFI_D_INFO, "*** FFSDELETE ***\n"));
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FfsSetPosition (
  IN EFI_FILE_PROTOCOL *This,
  IN UINT64 Position
  )
{
  DEBUG ((EFI_D_INFO, "*** FFSSETPOSITION ***\n"));
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FfsGetInfo (
  IN EFI_FILE_PROTOCOL *This,
  IN EFI_GUID *InformationType,
  IN OUT UINTN *BufferSize,
  OUT VOID *Buffer
  )
{
  EFI_STATUS Status;

  Status = EFI_SUCCESS;
  DEBUG ((EFI_D_INFO, "*** FfsGetInfo: Start of func ***\n"));

  // Check InformationType.
  if (CompareGuid (InformationType, &gEfiFileInfoGuid)) {
    // Requesting EFI_FILE_INFO.
    DEBUG ((EFI_D_INFO, "*** FfsGetInfo: EFI_FILE_INFO request ***\n"));
  } else if (CompareGuid (InformationType, &gEfiFileSystemInfoGuid)) {
    // Requesting EFI_FILE_SYSTEM_INFO.
    DEBUG ((EFI_D_INFO, "*** FfsGetInfo: EFI_FILE_SYSTEM_INFO request ***\n"));
  } else if (CompareGuid (InformationType, &gEfiFileSystemVolumeLabelInfoIdGuid)) {
    // Requesting EFI_FILE_SYSTEM_VOLUME_LABEL.
    DEBUG ((EFI_D_INFO, "*** FfsGetInfo: EFI_VOLUME_NAME request ***\n"));
  } else {
    // Invalid value.
    DEBUG ((EFI_D_INFO, "*** FfsGetInfo: Invalid request ***\n"));
  }

  return Status;
}

EFI_STATUS
EFIAPI
FfsSetInfo (
  IN EFI_FILE_PROTOCOL *This,
  IN EFI_GUID *InformationType,
  IN UINTN BufferSize,
  IN VOID *Buffer
  )
{
  DEBUG ((EFI_D_INFO, "*** FFSSETINFO ***\n"));
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FfsFlush (IN EFI_FILE_PROTOCOL *This)
{
  DEBUG ((EFI_D_INFO, "*** FFSFLUSH ***\n"));
  return EFI_ACCESS_DENIED;
}

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

  Callback function, notified when new FV2 volumes are mounted in the system.

  @returns Nothing.

  @param Event   The EFI_EVENT that triggered this function call.
  @param Context The context in which this function was called.

**/
{
  UINTN                           BufferSize;
  EFI_STATUS                      Status;
  EFI_HANDLE                      HandleBuffer;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFileSystem;
  FILE_SYSTEM_PRIVATE_DATA        *Private;

  while (TRUE) {
    // Grab the next FV2. If this is there are no more in the buffer, exit the
    // while loop and return.
    BufferSize = sizeof (HandleBuffer);
    Status = gBS->LocateHandle (
                    ByRegisterNotify,
                    &gEfiFirmwareVolume2ProtocolGuid,
                    mFfsRegistration,
                    &BufferSize,
                    &HandleBuffer
                    );

    if (EFI_ERROR (Status)) {
      break;
    }

    // Check to see if SimpleFileSystem is already installed on this handle. If
    // the protocol is already installed, skip to the next entry.
    Status = gBS->HandleProtocol (
                    HandleBuffer,
                    &gEfiSimpleFileSystemProtocolGuid,
                    (VOID **)&SimpleFileSystem
                    );

    if (!EFI_ERROR (Status)) {
      continue;
    }

    // Allocate space for the private data structure. If this fails, move on to
    // the next entry.
    Private = AllocateCopyPool (
                sizeof (FILE_SYSTEM_PRIVATE_DATA),
                &mFileSystemPrivateDataTemplate
                );

    if (Private == NULL) {
      continue;
    }

    // Retrieve the FV2 protocol
    Status = gBS->HandleProtocol (
                    HandleBuffer,
                    &gEfiFirmwareVolume2ProtocolGuid,
                    (VOID **)&Private->FirmwareVolume2
                    );

    ASSERT_EFI_ERROR (Status);

    // Install SimpleFileSystem on the handle
    Status = gBS->InstallMultipleProtocolInterfaces (
                    &HandleBuffer,
                    &gEfiSimpleFileSystemProtocolGuid,
                    &Private->SimpleFileSystem,
                    NULL
                    );

    ASSERT_EFI_ERROR (Status);

    DEBUG ((EFI_D_INFO, "FfsNotificationEvent: Installed SFS on FV2!\n"));
  }
}

EFI_STATUS
EFIAPI
InitializeFfsFileSystem (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
/**

  Entry point for the driver.

  @returns EFI status code.

  @param ImageHandle ImageHandle of the loaded driver.
  @param SystemTable A Pointer to the EFI System Table.

**/
{
  EfiCreateProtocolNotifyEvent (
    &gEfiFirmwareVolume2ProtocolGuid,
    TPL_CALLBACK,
    FfsNotificationEvent,
    NULL,
    &mFfsRegistration
    );

  return EFI_SUCCESS;
}
