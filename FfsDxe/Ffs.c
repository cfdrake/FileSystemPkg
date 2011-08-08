/** @file

Copyright 2011 Colin Drake. All rights reserved.
Copyright (c) 2011, Intel Corporation. All rights reserved.<BR>

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

UINTN
FvFileGetSize (
  IN EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv2,
  IN EFI_GUID                      *FileGuid
  )
{
  UINTN                  BufferSize;
  UINT32                 AuthenticationStatus;
  EFI_FV_FILETYPE        FoundType;
  EFI_FV_FILE_ATTRIBUTES FileAttributes;

  // When ReadFile is called with Buffer == NULL, the value returned in 
  // BufferSize will be the size of the file.
  Fv2->ReadFile (Fv2,
                 FileGuid,
                 NULL,
                 &BufferSize,
                 &FoundType,
                 &FileAttributes,
                 &AuthenticationStatus);

  // Return the file size.
  return BufferSize;
}

BOOLEAN
IsFileExecutable (
  IN EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv2,
  IN EFI_GUID                      *FileGuid
  )
{
  EFI_STATUS       Status;
  EFI_SECTION_TYPE SectionType;
  VOID             *Buffer;
  UINTN            MachineType,
                   BufferSize,
                   SectionInstance;
  UINT32           AuthenticationStatus;

  // Initialize local variables.
  SectionType = EFI_SECTION_PE32;
  Buffer = NULL;
  SectionInstance = 0;
  BufferSize = 0;

  // Read the executable section of the file passed in.
  Status = Fv2->ReadSection (Fv2,
                             FileGuid,
                             SectionType,
                             SectionInstance,
                             &Buffer,
                             &BufferSize,
                             &AuthenticationStatus);

  // Return FALSE if there's an error reading the executable section.
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  // Determine and return if the machine type is supported or not.
  MachineType = PeCoffLoaderGetMachineType (Buffer);
  return EFI_IMAGE_MACHINE_TYPE_SUPPORTED (MachineType);
}

EFI_GUID *
FvGetFile (
  IN EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv2,
  IN CHAR16                        *FileName
  )
/**
  Gets an EFI_GUID representing a FV2 given in a stringified GUID name.

  @param  Fv2      A pointer to the firmware volume to search.
  @param  FileName A string representing the file that the system is trying to access.

  @retval a GUID The file was found, and the associated GUID was returned.
  @retval NULL   The file was not found.

**/
{
  EFI_STATUS             Status;
  BOOLEAN                Found;
  VOID                   *Key;
  EFI_FV_FILETYPE        FileType;
  EFI_GUID               *NameGuid;
  EFI_FV_FILE_ATTRIBUTES FvAttributes;
  UINTN                  Size;
  CHAR16                 *GuidAsString;

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
/**
  Sums the file sizes of all files in a firmware volume to find the total size
  of the volume.

  @param  Fv2      A pointer to the firmware volume to calculate the size for.

  @retval The sum of all of the file sizes on a given volume.

**/
{
  EFI_STATUS             Status;
  VOID                   *Key;
  EFI_FV_FILETYPE        FileType;
  EFI_GUID               *NameGuid;
  EFI_FV_FILE_ATTRIBUTES FvAttributes;
  UINTN                  Size, TotalSize;

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

BOOLEAN
EFIAPI
PathRemoveLastItem(
  IN OUT CHAR16 *Path
  )
/**
  Removes the last directory or file entry in a path by changing the last
  L'\' to a CHAR_NULL.

  @param[in,out] Path    The pointer to the path to modify.

  @retval FALSE     Nothing was found to remove.
  @retval TRUE      A directory or file was removed.
**/
{
  CHAR16        *Walker;
  CHAR16        *LastSlash;
  //
  // get directory name from path... ('chop' off extra)
  //
  for ( Walker = Path, LastSlash = NULL
      ; Walker != NULL && *Walker != CHAR_NULL
      ; Walker++
     ){
    if (*Walker == L'\\' && *(Walker + 1) != CHAR_NULL) {
      LastSlash = Walker+1;
    }
  }
  if (LastSlash != NULL) {
    *LastSlash = CHAR_NULL;
    return (TRUE);
  }
  return (FALSE);
}

CHAR16*
EFIAPI
PathCleanUpDirectories(
  IN CHAR16 *Path
  )
/**
  Function to clean up paths.  
  
  - Single periods in the path are removed.
  - Double periods in the path are removed along with a single parent directory.
  - Forward slashes L'/' are converted to backward slashes L'\'.

  This will be done inline and the existing buffer may be larger than required 
  upon completion.

  @param[in] Path       The pointer to the string containing the path.

  @retval NULL          An error occured.
  @return Path in all other instances.
**/
{
  CHAR16  *TempString;
  UINTN   TempSize;
  if (Path==NULL) {
    return(NULL);
  }

  //
  // Fix up the '/' vs '\'
  //
  for (TempString = Path ; TempString != NULL && *TempString != CHAR_NULL ; TempString++) {
    if (*TempString == L'/') {
      *TempString = L'\\';
    }
  }

  //
  // Fix up the ..
  //
  while ((TempString = StrStr(Path, L"\\..\\")) != NULL) {
    *TempString = CHAR_NULL;
    TempString  += 4;
    PathRemoveLastItem(Path);
    TempSize = StrSize(TempString);
    CopyMem(Path+StrLen(Path), TempString, TempSize);
  }
  if ((TempString = StrStr(Path, L"\\..")) != NULL && *(TempString + 3) == CHAR_NULL) {
    *TempString = CHAR_NULL;
    PathRemoveLastItem(Path);
  }

  //
  // Fix up the .
  //
  while ((TempString = StrStr(Path, L"\\.\\")) != NULL) {
    *TempString = CHAR_NULL;
    TempString  += 2;
    TempSize = StrSize(TempString);
    CopyMem(Path+StrLen(Path), TempString, TempSize);
  }
  if ((TempString = StrStr(Path, L"\\.")) != NULL && *(TempString + 2) == CHAR_NULL) {
    *TempString = CHAR_NULL;
  }

  return (Path);
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
/**
  Open the root directory on a volume.

  @param  This A pointer to the volume to open the root directory.
  @param  Root A pointer to the location to return the opened file handle for the
               root directory.

  @retval EFI_SUCCESS          The device was opened.
  @retval EFI_UNSUPPORTED      This volume does not support the requested file system type.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_ACCESS_DENIED    The service denied access to the file.
  @retval EFI_OUT_OF_RESOURCES The volume was not opened due to lack of resources.
  @retval EFI_MEDIA_CHANGED    The device has a different medium in it or the medium is no
                               longer supported. Any existing file handles for this volume are
                               no longer valid. To access the files on the new medium, the
                               volume must be reopened with OpenVolume().

**/
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
/**
  Opens a new file relative to the source file's location.

  @param  This       A pointer to the EFI_FILE_PROTOCOL instance that is the file
                     handle to the source location. This would typically be an open
                     handle to a directory.
  @param  NewHandle  A pointer to the location to return the opened handle for the new
                     file.
  @param  FileName   The Null-terminated string of the name of the file to be opened.
                     The file name may contain the following path modifiers: "\", ".",
                     and "..".
  @param  OpenMode   The mode to open the file. The only valid combinations that the
                     file may be opened with are: Read, Read/Write, or Create/Read/Write.
  @param  Attributes Only valid for EFI_FILE_MODE_CREATE, in which case these are the 
                     attribute bits for the newly created file.

  @retval EFI_SUCCESS          The file was opened.
  @retval EFI_NOT_FOUND        The specified file could not be found on the device.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_MEDIA_CHANGED    The device has a different medium in it or the medium is no
                               longer supported.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED  An attempt was made to create a file, or open a file for write
                               when the media is write-protected.
  @retval EFI_ACCESS_DENIED    The service denied access to the file.
  @retval EFI_OUT_OF_RESOURCES Not enough resources were available to open the file.
  @retval EFI_VOLUME_FULL      The volume is full.

**/
{
  EFI_STATUS               Status;
  FILE_PRIVATE_DATA        *PrivateFile, *NewPrivateFile;
  FILE_INFO                *FileInfo;
  EFI_GUID                 *Guid;
  FILE_SYSTEM_PRIVATE_DATA *FileSystem;
  CHAR16                   *CleanPath;

  Status = EFI_SUCCESS;
  DEBUG ((EFI_D_INFO, "FfsOpen: Start\n"));

  // Check for a valid OpenMode parameter. Since this is a read-only filesystem
  // it must not be EFI_FILE_MODE_WRITE or EFI_FILE_MODE_CREATE. Additionally,
  // ensure that the file name to be accessed isn't empty.
  if (OpenMode & EFI_FILE_MODE_READ ||
      OpenMode & EFI_FILE_MODE_CREATE) {
    DEBUG ((EFI_D_INFO, "FfsOpen: OpenMode must be Read\n"));
    return EFI_WRITE_PROTECTED;
  } else if (FileName == NULL || StrCmp (FileName, L"") == 0) {
    DEBUG ((EFI_D_INFO, "FfsOpen: Missing FileName!\n"));
    return EFI_NOT_FOUND;
  }

  DEBUG ((EFI_D_INFO, "FfsOpen: Opening: %s\n", FileName));
  PrivateFile = FILE_PRIVATE_DATA_FROM_THIS (This);
  CleanPath = PathCleanUpDirectories (FileName);
  DEBUG ((EFI_D_INFO, "FfsOpen: Path reconstructed as: %s\n", CleanPath));

  // Check the filename that was specified to open.
  if (StrCmp (CleanPath, L"\\") == 0 ||
             (StrCmp (CleanPath, L".") == 0 && PrivateFile->IsDirectory)) {
    // Open the root directory.
    DEBUG ((EFI_D_INFO, "FfsOpen: Open root\n"));

    FileSystem = PrivateFile->FileSystem;
    *NewHandle = &(FileSystem->Root->File);

    return Status;
  } else if (StrCmp (CleanPath, L"..") == 0) {
    // Open the parent directory.
    DEBUG ((EFI_D_INFO, "FfsOpen: Open parent\n"));

    return EFI_NOT_FOUND;
  } else {
    // Check for the filename on the FV2 volume.
    Guid = FvGetFile (PrivateFile->FileSystem->FirmwareVolume2, FileName);

    if (Guid != NULL) {
      // Found file.
      DEBUG ((EFI_D_INFO, "FfsOpen: File found\n"));

      // Allocate a file info instance for this file.
      FileInfo = AllocateZeroPool (sizeof (FILE_INFO));
      FileInfo->IsExecutable = IsFileExecutable(PrivateFile->FileSystem->FirmwareVolume2,
                                                Guid);
      FileInfo->NameGuid = *Guid;

      // Allocate a new private file instance for this found file.
      NewPrivateFile = AllocateCopyPool (sizeof (FILE_PRIVATE_DATA),
                                         &mFilePrivateDataTemplate);
      NewPrivateFile->DirInfo = NULL;
      NewPrivateFile->FileInfo = FileInfo;

      // Assign the outgoing parameters
      *NewHandle = &(NewPrivateFile->File);
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
/**
  Closes a specified file handle.

  @param  This          A pointer to the EFI_FILE_PROTOCOL instance that is the file 
                        handle to close.

  @retval EFI_SUCCESS   The file was closed.

**/
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
/**
  Close and delete the file handle.

  @param  This                     A pointer to the EFI_FILE_PROTOCOL instance that is the
                                   handle to the file to delete.

  @retval EFI_SUCCESS              The file was closed and deleted, and the handle was closed.
  @retval EFI_WARN_DELETE_FAILURE  The handle was closed, but the file was not deleted.

**/
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
/**
  Reads data from a file.

  @param  This       A pointer to the EFI_FILE_PROTOCOL instance that is the file
                     handle to read data from.
  @param  BufferSize On input, the size of the Buffer. On output, the amount of data
                     returned in Buffer. In both cases, the size is measured in bytes.
  @param  Buffer     The buffer into which the data is read.

  @retval EFI_SUCCESS          Data was read.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_DEVICE_ERROR     An attempt was made to read from a deleted file.
  @retval EFI_DEVICE_ERROR     On entry, the current file position is beyond the end of the file.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_BUFFER_TO_SMALL  The BufferSize is too small to read the current directory
                               entry. BufferSize has been updated with the size
                               needed to complete the request.

**/
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
/**
  Writes data to a file.

  @param  This       A pointer to the EFI_FILE_PROTOCOL instance that is the file
                     handle to write data to.
  @param  BufferSize On input, the size of the Buffer. On output, the amount of data
                     actually written. In both cases, the size is measured in bytes.
  @param  Buffer     The buffer of data to write.

  @retval EFI_SUCCESS          Data was written.
  @retval EFI_UNSUPPORTED      Writes to open directory files are not supported.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_DEVICE_ERROR     An attempt was made to write to a deleted file.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED  The file or medium is write-protected.
  @retval EFI_ACCESS_DENIED    The file was opened read only.
  @retval EFI_VOLUME_FULL      The volume is full.

**/
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
/**
  Returns a file's current position.

  @param  This            A pointer to the EFI_FILE_PROTOCOL instance that is the file
                          handle to get the current position on.
  @param  Position        The address to return the file's current position value.

  @retval EFI_SUCCESS      The position was returned.
  @retval EFI_UNSUPPORTED  The request is not valid on open directories.
  @retval EFI_DEVICE_ERROR An attempt was made to get the position from a deleted file.

**/
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
/**
  Sets a file's current position.

  @param  This            A pointer to the EFI_FILE_PROTOCOL instance that is the
                          file handle to set the requested position on.
  @param  Position        The byte position from the start of the file to set.

  @retval EFI_SUCCESS      The position was set.
  @retval EFI_UNSUPPORTED  The seek request for nonzero is not valid on open
                           directories.
  @retval EFI_DEVICE_ERROR An attempt was made to set the position of a deleted file.

**/
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
    PrivateFile->Position = FvFileGetSize (PrivateFile->FileSystem->FirmwareVolume2,
                                           &(PrivateFile->FileInfo->NameGuid));
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
/**
  Returns information about a file.

  @param  This            A pointer to the EFI_FILE_PROTOCOL instance that is the file
                          handle the requested information is for.
  @param  InformationType The type identifier for the information being requested.
  @param  BufferSize      On input, the size of Buffer. On output, the amount of data
                          returned in Buffer. In both cases, the size is measured in bytes.
  @param  Buffer          A pointer to the data buffer to return. The buffer's type is
                          indicated by InformationType.

  @retval EFI_SUCCESS          The information was returned.
  @retval EFI_UNSUPPORTED      The InformationType is not known.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_BUFFER_TOO_SMALL The BufferSize is too small to read the current directory entry.
                               BufferSize has been updated with the size needed to complete
                               the request.
**/
{
  EFI_STATUS           Status;
  EFI_FILE_INFO        *FileInfo;
  EFI_FILE_SYSTEM_INFO *FsInfo;
  FILE_PRIVATE_DATA    *PrivateFile;
  UINTN                DataSize;
  CHAR16               *VolumeLabel;

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
        FileInfo->FileSize = FvFileGetSize (PrivateFile->FileSystem->FirmwareVolume2,
                                            &(PrivateFile->FileInfo->NameGuid));
      }

      // Use the same value for PhysicalSize as FileSize calculated beforehand.
      FileInfo->PhysicalSize = FileInfo->FileSize;

      // Copy the memory to Buffer, set the output value of BufferSize, and
      // free the temporary data structure.
      CopyMem (Buffer, FileInfo, DataSize);
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

      VolumeLabel = AllocateZeroPool (SIZE_OF_FV_LABEL);
      UnicodeSPrint (VolumeLabel,
                     SIZE_OF_FV_LABEL,
                     L"FV2@0x%x",
                     &(PrivateFile->FileSystem->FirmwareVolume2));
      StrCpy (FsInfo->VolumeLabel, VolumeLabel);
      FreePool (VolumeLabel);
      
      // Copy the memory to Buffer, set the output value of BufferSize, and
      // free the temporary data structure.
      CopyMem (Buffer, FsInfo, DataSize);
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
/**
  Sets information about a file.

  @param  File            A pointer to the EFI_FILE_PROTOCOL instance that is the file
                          handle the information is for.
  @param  InformationType The type identifier for the information being set.
  @param  BufferSize      The size, in bytes, of Buffer.
  @param  Buffer          A pointer to the data buffer to write. The buffer's type is
                          indicated by InformationType.

  @retval EFI_SUCCESS          The information was set.
  @retval EFI_UNSUPPORTED      The InformationType is not known.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED  InformationType is EFI_FILE_INFO_ID and the media is
                               read-only.
  @retval EFI_WRITE_PROTECTED  InformationType is EFI_FILE_PROTOCOL_SYSTEM_INFO_ID
                               and the media is read only.
  @retval EFI_WRITE_PROTECTED  InformationType is EFI_FILE_SYSTEM_VOLUME_LABEL_ID
                               and the media is read-only.
  @retval EFI_ACCESS_DENIED    An attempt is made to change the name of a file to a
                               file that is already present.
  @retval EFI_ACCESS_DENIED    An attempt is being made to change the EFI_FILE_DIRECTORY
                               Attribute.
  @retval EFI_ACCESS_DENIED    An attempt is being made to change the size of a directory.
  @retval EFI_ACCESS_DENIED    InformationType is EFI_FILE_INFO_ID and the file was opened
                               read-only and an attempt is being made to modify a field
                               other than Attribute.
  @retval EFI_VOLUME_FULL      The volume is full.
  @retval EFI_BAD_BUFFER_SIZE  BufferSize is smaller than the size of the type indicated
                               by InformationType.

**/
{
  DEBUG ((EFI_D_INFO, "*** FfsSetInfo: Unsupported ***\n"));
  return EFI_WRITE_PROTECTED;
}

EFI_STATUS
EFIAPI
FfsFlush (IN EFI_FILE_PROTOCOL *This)
/**
  Flushes all modified data associated with a file to a device.

  @param  This A pointer to the EFI_FILE_PROTOCOL instance that is the file 
               handle to flush.

  @retval EFI_SUCCESS          The data was flushed.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED  The file or medium is write-protected.
  @retval EFI_ACCESS_DENIED    The file was opened read-only.
  @retval EFI_VOLUME_FULL      The volume is full.

**/
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
