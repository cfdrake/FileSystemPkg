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

EFI_TIME mModuleLoadTime;
EFI_EVENT mFfsRegistration;

FILE_SYSTEM_PRIVATE_DATA mFileSystemPrivateDataTemplate = {
  FILE_SYSTEM_PRIVATE_DATA_SIGNATURE,
  {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_REVISION,
    FfsOpenVolume
  },
  NULL,
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
  NULL,
  0
};

//
// Misc. helper methods
//

EFI_GUID *
FvGetFile (
  IN EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv2,
  IN CHAR16                        *FileName
  )
{
  EFI_STATUS                    Status;
  BOOLEAN                       Found;
  VOID                          *Key;
  EFI_FV_FILETYPE               FileType;
  EFI_GUID                      *NameGuid;
  EFI_FV_FILE_ATTRIBUTES        FvAttributes;
  UINTN                         Size;
  CHAR16                        *GuidAsString;

  // Loop through all FV2 files.
  Found        = FALSE;
  GuidAsString = AllocateZeroPool (SIZE_OF_GUID);
  NameGuid     = AllocateZeroPool (sizeof (EFI_GUID));
  Key          = AllocateZeroPool (Fv2->KeySize);

  while (TRUE) {
    // Grab the next file in the Fv2 volume.
    Status = Fv2->GetNextFile (Fv2, 
                               Key,
                               &FileType,
                               NameGuid,
                               &FvAttributes,
                               &Size);

    // Check for the file we're looking for by converting the found GUID to a
    // string and testing with StrCmp(). If the GUID and FileName input string
    // are equal, then the requested file was found.
    UnicodeSPrint (GuidAsString, SIZE_OF_GUID, L"%g", &NameGuid);
    DEBUG ((EFI_D_INFO, "FvGetFile: Checking GUID: %s...\n", GuidAsString));

    if (StrCmp (GuidAsString, FileName) == 0) {
      Found = TRUE;
      break;
    }

    // Check exit condition. If Status is an error status, then the list of
    // files tried has been exhausted. Break from the loop and return NULL.
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_INFO, "FvGetFile: At last file or ERROR found...\n"));
      break;
    }
  }

  // Free all allocated memory.
  FreePool (GuidAsString);
  FreePool (NameGuid);
  FreePool (Key);

  // Return either the GUID of a matching file or a pointer to NULL, indicating
  // that the requested file was not found.
  return (Found ? NameGuid : NULL);
}

UINTN
FvGetVolumeSize (
  IN EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv2
  )
{
  EFI_STATUS                    Status;
  VOID                          *Key;
  EFI_FV_FILETYPE               FileType;
  EFI_GUID                      *NameGuid;
  EFI_FV_FILE_ATTRIBUTES        FvAttributes;
  UINTN                         Size, TotalSize;

  // Loop through all FV2 files.
  TotalSize    = 0;
  NameGuid     = AllocateZeroPool (sizeof (EFI_GUID));
  Key          = AllocateZeroPool (Fv2->KeySize);

  while (TRUE) {
    // Grab the next file in the Fv2 volume.
    Status = Fv2->GetNextFile (Fv2, 
                               Key,
                               &FileType,
                               NameGuid,
                               &FvAttributes,
                               &Size);

    // Update the sum of file sizes.
    TotalSize += Size;

    // Check exit condition. If Status is an error status, then the list of
    // files tried has been exhausted. Break from the loop and return NULL.
    if (EFI_ERROR (Status)) {
      break;
    }
  }

  // Free all allocated memory.
  FreePool (NameGuid);
  FreePool (Key);

  // Return the size of the volume.
  return TotalSize;
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
  EFI_STATUS               Status;
  FILE_SYSTEM_PRIVATE_DATA *PrivateFileSystem;
  FILE_PRIVATE_DATA        *PrivateFile;
  DIR_INFO                 *RootInfo;
  
  DEBUG ((EFI_D_INFO, "FfsOpenVolume: Start\n"));

  // Get private structure for This.
  PrivateFileSystem = FILE_SYSTEM_PRIVATE_DATA_FROM_THIS (This);

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

  RootInfo                 = AllocateZeroPool (sizeof (DIR_INFO));
  InitializeListHead (&(RootInfo->Children));
  PrivateFile->DirInfo     = RootInfo;

  // Set the root folder of the file system and the outgoing paramater Root,
  // and set the status code to return to EFI_SUCCESS.
  PrivateFileSystem->Root = PrivateFile;
  *Root = &(PrivateFile->File);
  Status = EFI_SUCCESS;

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
  EFI_STATUS                      Status;
  FILE_PRIVATE_DATA               *PrivateFile;
  EFI_GUID                        *Guid;
  FILE_SYSTEM_PRIVATE_DATA        *FileSystem;

  Status = EFI_SUCCESS;
  DEBUG ((EFI_D_INFO, "FfsOpen: Start\n"));

  // Check for a valid OpenMode parameter. Since this is a read-only filesystem
  // it must not be EFI_FILE_MODE_WRITE or EFI_FILE_MODE_CREATE.
  if (!(OpenMode & EFI_FILE_MODE_READ)) {
    DEBUG ((EFI_D_INFO, "FfsOpen: OpenMode must be Read\n"));
    return EFI_WRITE_PROTECTED;
  }

  DEBUG ((EFI_D_INFO, "OPENING : %s\n", FileName));
  PrivateFile = FILE_PRIVATE_DATA_FROM_THIS (This);

  // Check the filename that was specified to open.
  if (StrCmp (FileName, L"\\") == 0 ||
             (StrCmp (FileName, L".") == 0 && PrivateFile->IsDirectory)) {
    // Open the root directory.
    DEBUG ((EFI_D_INFO, "FfsOpen: Open root\n"));

    FileSystem = PrivateFile->FileSystem;
    *NewHandle = &(FileSystem->Root->File);

    return Status;
  } else if (StrCmp (FileName, L"..") == 0) {
    // Open the parent directory.
    DEBUG ((EFI_D_INFO, "FfsOpen: Open parent\n"));

    return EFI_NOT_FOUND;
  } else {
    // Check for the filename on the FV2 volume.
    Guid = FvGetFile (PrivateFile->FileSystem->FirmwareVolume2, FileName);

    if (Guid != NULL) {
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
  FILE_PRIVATE_DATA *PrivateFile;

  DEBUG ((EFI_D_INFO, "*** FfsClose: Start of func ***\n"));

  // Grab the associated private data.
  PrivateFile = FILE_PRIVATE_DATA_FROM_THIS (This);

  // Free up all of the private data.
  /*if (PrivateFile->IsDirectory) {
    FreePool (PrivateFile->DirInfo);
  } else {
    FreePool (PrivateFile->FileInfo);
  }  

  FreePool (PrivateFile);*/

  DEBUG ((EFI_D_INFO, "*** FfsClose: End of func ***\n"));
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FfsDelete (IN EFI_FILE_PROTOCOL *This)
{
  DEBUG ((EFI_D_INFO, "*** FfsDelete: Unsupported ***\n"));
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
  EFI_STATUS        Status;
  FILE_PRIVATE_DATA *PrivateFile;

  Status = EFI_SUCCESS;
  DEBUG ((EFI_D_INFO, "*** FfsRead: Start of func ***\n"));

  // Grab private data.
  PrivateFile = FILE_PRIVATE_DATA_FROM_THIS (This);

  // Check filetype.
  if (PrivateFile->IsDirectory) {
    // Called on a Directory.
    DEBUG ((EFI_D_INFO, "*** FfsRead: Called on directory ***\n"));
  } else {
    // Called on a File.
    DEBUG ((EFI_D_INFO, "*** FfsRead: Called on file ***\n"));
  }

  DEBUG ((EFI_D_INFO, "*** FfsRead: End of func ***\n"));
  return Status;
}

EFI_STATUS
EFIAPI
FfsWrite (
  IN EFI_FILE_PROTOCOL *This,
  IN OUT UINTN *BufferSize,
  IN VOID *Buffer
  )
{
  DEBUG ((EFI_D_INFO, "*** FfsWrite: Unsupported ***\n"));
  return EFI_ACCESS_DENIED;
}

EFI_STATUS
EFIAPI
FfsGetPosition (
  IN EFI_FILE_PROTOCOL *This,
  OUT UINT64 *Position
  )
{
  FILE_PRIVATE_DATA *PrivateFile;

  DEBUG ((EFI_D_INFO, "*** FfsGetPosition: Start of func ***\n"));

  // Grab the private data associated with This.
  PrivateFile = FILE_PRIVATE_DATA_FROM_THIS (This);

  // Ensure that this function is not called on a directory.
  if (PrivateFile->IsDirectory) {
    return EFI_UNSUPPORTED;
  }

  // Grab the current file position.
  *Position = PrivateFile->Position;

  DEBUG ((EFI_D_INFO, "*** FfsGetPosition: End of func ***\n"));
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
FfsSetPosition (
  IN EFI_FILE_PROTOCOL *This,
  IN UINT64 Position
  )
{
  FILE_PRIVATE_DATA *PrivateFile;

  DEBUG ((EFI_D_INFO, "*** FfsSetPosition: Start of func ***\n"));

  // Grab private data associated with This.
  PrivateFile = FILE_PRIVATE_DATA_FROM_THIS (This);

  // Check for the invalid condition that This is a directory and the position
  // is non-zero. This has the effect of only allowing directory reads to be 
  // restarted.
  if (PrivateFile->IsDirectory && Position != 0) {
    return EFI_UNSUPPORTED;
  }

  // Set the position in the private data structures.
  if (Position == END_OF_FILE_POSITION) {
    // Set to the end-of-file.
    // TODO: fixme
  } else {
    // Set position.
    PrivateFile->Position = Position;
  }

  DEBUG ((EFI_D_INFO, "*** FfsSetPosition: End of func ***\n"));
  return EFI_SUCCESS;
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
  EFI_STATUS                   Status;
  EFI_FILE_INFO                *FileInfo;
  EFI_FILE_SYSTEM_INFO         *FsInfo;
  FILE_PRIVATE_DATA            *PrivateFile;
  UINTN                        DataSize;

  DEBUG ((EFI_D_INFO, "*** FfsGetInfo: Start of func ***\n"));

  // Grab the associated private data.
  PrivateFile = FILE_PRIVATE_DATA_FROM_THIS (This);

  // Check InformationType.
  if (CompareGuid (InformationType, &gEfiFileInfoGuid)) {
    // Requesting EFI_FILE_INFO.
    DEBUG ((EFI_D_INFO, "*** FfsGetInfo: EFI_FILE_INFO request ***\n"));

    // Determine if the size of Buffer is adequate, and if not, break so we can
    // return an error and calculate the needed size;
    DataSize = SIZE_OF_EFI_FILE_INFO + SIZE_OF_GUID;

    if (*BufferSize < DataSize) {
      // Error condition.
      *BufferSize = DataSize;
      Status = EFI_BUFFER_TOO_SMALL;
    } else {
      // Allocate and fill out an EFI_FILE_INFO instance for this file.
      FileInfo = AllocateZeroPool (DataSize);
      FileInfo->Size = DataSize;
      FileInfo->CreateTime = mModuleLoadTime;
      FileInfo->LastAccessTime = mModuleLoadTime;
      FileInfo->ModificationTime = mModuleLoadTime;
      FileInfo->Attribute = EFI_FILE_READ_ONLY;
      //FileInfo->FileName = PrivateFile->FileName;

      // Set the next params based on whether the file is a directory or not.
      if (PrivateFile->IsDirectory) {
        // Calculate size of the directory.
        FileInfo->FileSize = FvGetVolumeSize (
                               PrivateFile->FileSystem->FirmwareVolume2);

        // Update the Attributes field and calculate the size.
        if (PrivateFile->IsDirectory) {
          FileInfo->Attribute |= EFI_FILE_DIRECTORY;
        }
      } else {
        // Calculate the size of the file.
        FileInfo->FileSize = 0; // FIXME:
      }

      // Use the same value for PhysicalSize as FileSize calculated beforehand.
      FileInfo->PhysicalSize = FileInfo->FileSize;

      // Copy the memory to Buffer, set the output value of BufferSize, and
      // free the temporary data structure.
      CopyMem (Buffer, &FileInfo, DataSize);
      *BufferSize = DataSize;
      FreePool (FileInfo);

      Status = EFI_SUCCESS;
    }
  } else if (CompareGuid (InformationType, &gEfiFileSystemInfoGuid)) {
    // Requesting EFI_FILE_SYSTEM_INFO.
    DEBUG ((EFI_D_INFO, "*** FfsGetInfo: EFI_FILE_SYSTEM_INFO request ***\n"));

    // Determine if the size of Buffer is adequate, and if not, break so we can
    // return an error and calculate the needed size;
    DataSize = SIZE_OF_EFI_FILE_SYSTEM_INFO + SIZE_OF_GUID;

    if (*BufferSize < DataSize) {
      // Error condition.
      *BufferSize = DataSize;
      Status = EFI_BUFFER_TOO_SMALL;
    } else {
      // Allocate and fill out an EFI_FILE_INFO instance for this file.
      FsInfo = AllocateZeroPool (DataSize);
      FsInfo->Size = DataSize;
      FsInfo->ReadOnly = TRUE;
      FsInfo->VolumeSize = FvGetVolumeSize (
                             PrivateFile->FileSystem->FirmwareVolume2);
      FsInfo->FreeSpace = 0;
      FsInfo->BlockSize = 512;
      //FsInfo->VolumeLabel
      
      // Copy the memory to Buffer, set the output value of BufferSize, and
      // free the temporary data structure.
      CopyMem (Buffer, &FileInfo, DataSize);
      *BufferSize = DataSize;
      FreePool (FsInfo);

      Status = EFI_SUCCESS;
    }
  } else {
    // Invalid InformationType GUID.
    DEBUG ((EFI_D_INFO, "*** FfsGetInfo: Invalid request ***\n"));
    Status = EFI_UNSUPPORTED;
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
  DEBUG ((EFI_D_INFO, "*** FfsSetInfo: Unsupported ***\n"));
  return EFI_WRITE_PROTECTED;
}

EFI_STATUS
EFIAPI
FfsFlush (IN EFI_FILE_PROTOCOL *This)
{
  DEBUG ((EFI_D_INFO, "*** FfsFlush: Unsupported ***\n"));
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

  gRT->GetTime (&mModuleLoadTime, NULL);
  return EFI_SUCCESS;
}
