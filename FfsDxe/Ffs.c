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

/**
  Gets the number of files on a given EFI_FIRMWARE_VOLUME2_PROTOCOL instance.

  @param[in] Fv2 A pointer to the firmware volume to search.

  @return The number of files on the volume as an integer.

**/
UINTN
FvGetNumberOfFiles (
  IN EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv2
  )
{
  EFI_STATUS             Status;
  VOID                   *Key;
  EFI_FV_FILETYPE        FileType;
  EFI_GUID               NameGuid;
  EFI_FV_FILE_ATTRIBUTES FvAttributes;
  UINTN                  Size, NumFiles;

  NumFiles = 0;
  Key      = AllocateZeroPool (Fv2->KeySize);

  while (TRUE) {
    //
    // Grab the next file in the Fv2 volume.
    //
    FileType = EFI_FV_FILETYPE_ALL;
    Status = Fv2->GetNextFile (
                    Fv2, 
                    Key,
                    &FileType,
                    &NameGuid,
                    &FvAttributes,
                    &Size);

    //
    // Check exit condition. If Status is an error status, then the list of
    // files tried has been exhausted. Break from the loop and return NULL.
    //
    if (Status == EFI_NOT_FOUND) {
      break;
    }
    
    NumFiles++;
  }

  //
  // Free all allocated memory and return number of files on the volume.
  //
  DEBUG ((EFI_D_INFO, "FvGetNumberOfFiles: Directory has %d files\n", NumFiles));
  FreePool (Key);
  return NumFiles;
}

/**
  Determines if the file identified by FileGuid is executable on the current system.

  @param  Fv2      A pointer to the firmware volume to search.
  @param  FileGuid A GUID naming the file that the system is trying to access.

  @retval True or False if the file is executable.

**/
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
  BOOLEAN          Executable;

  SectionType     = EFI_SECTION_PE32;
  Buffer          = NULL;
  SectionInstance = 0;
  BufferSize      = 0;

  //
  // Read the executable section of the file passed in.
  //
  Status = Fv2->ReadSection (Fv2,
                             FileGuid,
                             SectionType,
                             SectionInstance,
                             &Buffer,
                             &BufferSize,
                             &AuthenticationStatus);

  //
  // Return FALSE if there's an error reading the executable section.
  //
  if (EFI_ERROR (Status)) {
    Executable = FALSE;
  } else {
    //
    // Determine and return if the machine type is supported or not.
    //
    MachineType = PeCoffLoaderGetMachineType (Buffer);
    Executable  = EFI_IMAGE_MACHINE_TYPE_SUPPORTED (MachineType);
  }

  return Executable;
}

/**
  Gets the size of the file named by the provided GUID in FileGuid.

  @param  Fv2      A pointer to the firmware volume to search.
  @param  FileGuid A GUID representing the file that the system is trying to access.

  @retval The size of the file in bytes.

**/
UINTN
FvFileGetSize (
  IN EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv2,
  IN EFI_GUID                      *FileGuid
  )
{
  UINTN                  BufferSize, SectionInstance;
  UINT32                 AuthenticationStatus;
  EFI_FV_FILETYPE        FoundType;
  EFI_FV_FILE_ATTRIBUTES FileAttributes;
  EFI_SECTION_TYPE       SectionType;
  VOID                   *Buffer;

  Buffer = NULL;
  BufferSize = 0;

  if (IsFileExecutable (Fv2, FileGuid)) {
    //
    // When ReadSection is called with Buffer == NULL, the value returned in
    // BufferSize will be the size of the section.
    //
    SectionType     = EFI_SECTION_PE32;
    SectionInstance = 0;

    Fv2->ReadSection (
           Fv2,
           FileGuid,
           SectionType,
           SectionInstance,
           &Buffer,
           &BufferSize,
           &AuthenticationStatus);

    FreePool (Buffer);
  } else {
    //
    // When ReadFile is called with Buffer == NULL, the value returned in 
    // BufferSize will be the size of the file.
    //
    Fv2->ReadFile (
           Fv2,
           FileGuid,
           NULL,
           &BufferSize,
           &FoundType,
           &FileAttributes,
           &AuthenticationStatus);
  }

  return BufferSize;
}

/**
  Gets an EFI_GUID representing an FV2 file given stringified GUID name.

  @param  Fv2      A pointer to the firmware volume to search.
  @param  FileName A string representing the file that the system is trying to access.

  @retval a GUID The file was found, and the associated GUID was returned.
  @retval NULL   The file was not found.

**/
EFI_GUID *
FvGetFile (
  IN EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv2,
  IN CHAR16                        *FileName
  )
{
  EFI_STATUS             Status;
  BOOLEAN                Found;
  VOID                   *Key;
  EFI_FV_FILETYPE        FileType;
  EFI_GUID               NameGuid, *OutputGuid;
  EFI_FV_FILE_ATTRIBUTES FvAttributes;
  UINTN                  Size;
  CHAR16                 *GuidAsString;

  // Loop through all FV2 files.
  Found        = FALSE;
  GuidAsString = AllocateZeroPool (SIZE_OF_GUID);
  OutputGuid   = AllocateZeroPool (sizeof (EFI_GUID));
  Key          = AllocateZeroPool (Fv2->KeySize);

  while (TRUE) {
    //
    // Grab the next file in the Fv2 volume.
    //
    FileType = EFI_FV_FILETYPE_ALL;
    Status = Fv2->GetNextFile (
                    Fv2, 
                    Key,
                    &FileType,
                    &NameGuid,
                    &FvAttributes,
                    &Size);

    //
    // Check exit condition. If Status is an error status, then the list of
    // files tried has been exhausted. Break from the loop and return NULL.
    //
    if (Status == EFI_NOT_FOUND) {
      DEBUG ((EFI_D_INFO, "FvGetFile: At last file or ERROR found...\n"));
      break;
    }

    //
    // Check for the file we're looking for by converting the found GUID to a
    // string and testing with StrCmp(). If the GUID and FileName input string
    // are equal, then the requested file was found.
    //
    UnicodeSPrint (GuidAsString, SIZE_OF_GUID, L"%g", &NameGuid);

    if (StrCmp (GuidAsString, FileName) == 0) {
      Found = TRUE;
      break;
    }
  }

  //
  // Free all allocated memory.
  //
  FreePool (GuidAsString);
  FreePool (Key);

  //
  // Return either the GUID of a matching file or a pointer to NULL, indicating
  // that the requested file was not found.
  //
  CopyMem (OutputGuid, &NameGuid, sizeof (EFI_GUID));
  return (Found ? OutputGuid : NULL);
}

/**
  Gets the EFI_GUID of the next file in the GetNextFile call chain.

  @param  PrivateFile Pointer to the FILE_PRIVATE_DATA instance representing
                      the root directory.

  @retval a GUID A file was found, and the associated GUID was returned.
  @retval NULL   End of directory listing.

**/
EFI_GUID *
RootGetNextFile (
  IN OUT FILE_PRIVATE_DATA *PrivateFile
  )
{
  EFI_STATUS                    Status;
  EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv2;
  EFI_FV_FILETYPE               FileType;
  EFI_GUID                      NameGuid, *OutputGuid;
  EFI_FV_FILE_ATTRIBUTES        FvAttributes;
  UINTN                         Size;

  Fv2        = PrivateFile->FileSystem->FirmwareVolume2;
  OutputGuid = AllocateZeroPool (sizeof (EFI_GUID));

  //
  // Grab the next file in the Fv2 volume. The Key parameter in this function
  // is preserved and set into PrivateFile->DirInfo->Key, so in the next call
  // of this function, the next file will be found.
  //
  FileType = EFI_FV_FILETYPE_ALL;
  Status = Fv2->GetNextFile (
                  Fv2,
                  PrivateFile->DirInfo->Key,
                  &FileType,
                  &NameGuid,
                  &FvAttributes,
                  &Size);

  //
  // Return either the GUID of a matching file or a pointer to NULL, indicating
  // that the file was not found.
  //
  CopyMem (OutputGuid, &NameGuid, sizeof (EFI_GUID));
  return (Status == EFI_NOT_FOUND ? NULL : OutputGuid);
}

/**
  Sums the file sizes of all files in a firmware volume to find the total size
  of the volume.

  @param  Fv2      A pointer to the firmware volume to calculate the size for.

  @retval The sum of all of the file sizes on a given volume.

**/
UINTN
FvGetVolumeSize (
  IN EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv2
  )
{
  EFI_STATUS             Status;
  VOID                   *Key;
  EFI_FV_FILETYPE        FileType;
  EFI_GUID               NameGuid;
  EFI_FV_FILE_ATTRIBUTES FvAttributes;
  UINTN                  Size, TotalSize;

  TotalSize    = 0;
  Key          = AllocateZeroPool (Fv2->KeySize);

  while (TRUE) {
    //
    // Grab the next file in the Fv2 volume.
    //
    FileType = EFI_FV_FILETYPE_ALL;
    Status = Fv2->GetNextFile (
                    Fv2, 
                    Key,
                    &FileType,
                    &NameGuid,
                    &FvAttributes,
                    &Size);

    TotalSize += Size;

    //
    // Check exit condition. If Status is an error status, then the list of
    // files tried has been exhausted. Break from the loop.
    //
    if (Status == EFI_NOT_FOUND) {
      break;
    }
  }

  //
  // Free all allocated memory and return the Fv2's total size.
  //
  FreePool (Key);
  return TotalSize;
}

/**
  Returns a FILE_PRIVATE_DATA instance for a GUID.

  @param  NameGuid   GUID representing the file to return.
  @param  FileSystem The FILE_SYSTEM_PRIVATE_DATA that the new file is to be a
                     part of.

  @retval FILE_PRIVATE_DATA instance representing Fv2 file named by NameGuid.

**/
FILE_PRIVATE_DATA *
GuidToFile (
  IN EFI_GUID                 *NameGuid,
  IN FILE_SYSTEM_PRIVATE_DATA *FileSystem
  )
{
  FILE_INFO *FileInfo;
  FILE_PRIVATE_DATA *PrivateFile;

  //
  // Allocate new file and file info instances and fill them out.
  //
  PrivateFile = AllocateCopyPool (
                  sizeof (FILE_PRIVATE_DATA),
                  &mFilePrivateDataTemplate);
  FileInfo    = AllocateZeroPool (sizeof (FILE_INFO));

  PrivateFile->DirInfo    = NULL;
  PrivateFile->FileInfo   = FileInfo;
  PrivateFile->FileSystem = FileSystem;
  FileInfo->NameGuid      = *NameGuid;
  FileInfo->IsExecutable  = IsFileExecutable(
                              PrivateFile->FileSystem->FirmwareVolume2,
                              NameGuid);

  //
  // Generate filename.
  //
  PrivateFile->FileName = AllocateZeroPool (SIZE_OF_GUID);

  if (FileInfo->IsExecutable) {
    UnicodeSPrint (PrivateFile->FileName, SIZE_OF_FILENAME, L"%g.efi", NameGuid);
  } else {
    UnicodeSPrint (PrivateFile->FileName, SIZE_OF_FILENAME, L"%g.ffs", NameGuid);
  }

  return PrivateFile;
}

/**
  Returns a FILE_PRIVATE_DATA instance for a new instance of the root directory.

  @param  Fs Private data for the filesystem the directory is to be a part of.

  @return FILE_PRIVATE_DATA instance representing the root directory.

**/
FILE_PRIVATE_DATA *
AllocateNewRoot (
  IN FILE_SYSTEM_PRIVATE_DATA *Fs
  )
{
  FILE_PRIVATE_DATA *PrivateFile;
  DIR_INFO          *RootInfo;

  //
  // Copy data from the template to a new private file instance.
  //
  PrivateFile = NULL;
  PrivateFile = AllocateCopyPool (
                  sizeof (FILE_PRIVATE_DATA),
                  (VOID *) &mFilePrivateDataTemplate
                  );

  if (PrivateFile == NULL) {
    goto RootDone;
  }

  RootInfo      = AllocateZeroPool (sizeof (DIR_INFO));
  RootInfo->Key = AllocateZeroPool (sizeof (Fs->FirmwareVolume2->KeySize));

  //
  // Fill out the rest of the private file data and assign it's File attribute
  // to Root.
  //
  PrivateFile->FileSystem  = Fs;
  PrivateFile->FileName    = L"";
  PrivateFile->IsDirectory = TRUE;
  PrivateFile->DirInfo     = RootInfo;

RootDone:

  return PrivateFile;
}

/**
  Removes the last directory or file entry in a path by changing the last
  L'\' to a CHAR_NULL.

  @param[in,out] Path    The pointer to the path to modify.

  @retval FALSE     Nothing was found to remove.
  @retval TRUE      A directory or file was removed.
**/
BOOLEAN
EFIAPI
PathRemoveLastItem(
  IN OUT CHAR16 *Path
  )
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
CHAR16*
EFIAPI
PathCleanUpDirectories(
  IN CHAR16 *Path
  )
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

  //
  // Remove '\' at the beginning of the path
  //
  while ((Path[0] == L'\\') && (Path[1] != CHAR_NULL)) {
    CopyMem (Path, Path + 1, StrSize (Path) - sizeof (L'\\'));
  }

  return (Path);
}

//
// SimpleFileSystem and File protocol functions
//

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
  
  DEBUG ((EFI_D_INFO, "FfsOpenVolume: Start\n"));

  //
  // Get private structure for This and allocate a new root instance.
  //
  PrivateFileSystem = FILE_SYSTEM_PRIVATE_DATA_FROM_THIS (This);
  PrivateFile       = AllocateNewRoot (PrivateFileSystem);

  if (PrivateFile == NULL) {
    //
    // Error allocating a new instance.
    //
    Status = EFI_OUT_OF_RESOURCES;
  } else {
    //
    // Set outgoing param and status.
    //
    *Root = &(PrivateFile->File);
    Status = EFI_SUCCESS;
  }

  DEBUG ((EFI_D_INFO, "FfsOpenVolume: End of func\n"));
  return Status;
}

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
  EFI_STATUS               Status;
  FILE_PRIVATE_DATA        *PrivateFile, *NewPrivateFile;
  EFI_GUID                 *Guid;
  CHAR16                   *CleanPath, *Ext, *GuidAsString;

  Status = EFI_SUCCESS;
  DEBUG ((EFI_D_INFO, "FfsOpen: Start\n"));

  //
  // Check for a valid OpenMode parameter. Since this is a read-only filesystem
  // it must not be EFI_FILE_MODE_WRITE or EFI_FILE_MODE_CREATE. Additionally,
  // ensure that the file name to be accessed isn't empty.
  //
  if ((OpenMode & (EFI_FILE_MODE_READ | EFI_FILE_MODE_CREATE)) != EFI_FILE_MODE_READ) {
    DEBUG ((EFI_D_INFO, "FfsOpen: OpenMode must be Read\n"));
    Status = EFI_WRITE_PROTECTED;
    goto OpenDone;
  } else if (FileName == NULL || StrCmp (FileName, L"") == 0) {
    DEBUG ((EFI_D_INFO, "FfsOpen: Missing FileName!\n"));
    Status = EFI_NOT_FOUND;
    goto OpenDone;
  }

  DEBUG ((EFI_D_INFO, "FfsOpen: Opening: %s\n", FileName));
  PrivateFile = FILE_PRIVATE_DATA_FROM_THIS (This);
  CleanPath = PathCleanUpDirectories (FileName);
  DEBUG ((EFI_D_INFO, "FfsOpen: Path reconstructed as: %s\n", CleanPath));

  //
  // Check the filename that was specified to open.
  //
  if (StrCmp (CleanPath, L"\\") == 0 ||
             (StrCmp (CleanPath, L".") == 0 && PrivateFile->IsDirectory)) {
    //
    // Open the root directory.
    //
    DEBUG ((EFI_D_INFO, "FfsOpen: Open root\n"));

    NewPrivateFile = AllocateNewRoot (PrivateFile->FileSystem);
    *NewHandle = &(NewPrivateFile->File);
  } else if (StrCmp (CleanPath, L"..") == 0) {
    //
    // Open the parent directory. This is invalid on the filesystem.
    //
    DEBUG ((EFI_D_INFO, "FfsOpen: Open parent\n"));
    Status = EFI_NOT_FOUND;
  } else {
    //
    // Perform basic checks to ensure we don't go through a lot of code for
    // nothing. First of all, ensure the filename length is ok.
    //
    if (StrLen(CleanPath) != LENGTH_OF_FILENAME) {
      DEBUG ((EFI_D_INFO, "Filename isn't 40 characters\n"));
      Status = EFI_NOT_FOUND;
      goto OpenDone;
    }

    //
    // Ensure the file extension is either .ffs or .efi.
    //
    Ext = CleanPath + LENGTH_OF_FILENAME - 4;

    if (StrCmp (Ext, L".ffs") != 0 && StrCmp (Ext, L".efi") != 0) {
      DEBUG ((EFI_D_INFO, "Invalid extension (not ffs or efi)\n"));
      Status = EFI_NOT_FOUND;
      goto OpenDone;
    }
      
    //
    // Check everything up until the extension to make sure it is a GUID used
    // in this specific FV2.
    //
    GuidAsString = AllocateZeroPool (SIZE_OF_GUID);
    CopyMem (GuidAsString, CleanPath, SIZE_OF_GUID);
    GuidAsString[36] = '\0';

    DEBUG ((EFI_D_INFO, "Looking for %s\n", GuidAsString));
    Guid = FvGetFile (PrivateFile->FileSystem->FirmwareVolume2, GuidAsString);

    //
    // Grab the file.
    //
    if (Guid != NULL) {
      //
      // Found file.
      //
      DEBUG ((EFI_D_INFO, "FfsOpen: File found\n"));
      NewPrivateFile = GuidToFile (Guid, PrivateFile->FileSystem);

      //
      // Check that the file we got has the correct extension for its contents.
      //
      if ((StrCmp (Ext, L".ffs") == 0 && !NewPrivateFile->FileInfo->IsExecutable) ||
          (StrCmp (Ext, L".efi") == 0 &&  NewPrivateFile->FileInfo->IsExecutable)) {
        *NewHandle = &(NewPrivateFile->File);
      } else {
        DEBUG ((EFI_D_INFO, "Invalid extension for contents\n"));
        Status = EFI_NOT_FOUND;
      }
      
      FreePool (Guid);
    } else {
      //
      // File not found.
      //
      DEBUG ((EFI_D_INFO, "FfsOpen: File not found\n"));
      Status = EFI_NOT_FOUND;
    }

    FreePool (GuidAsString);
  }

  DEBUG ((EFI_D_INFO, "FfsOpen: End of func\n"));

OpenDone:

  return Status;
}

/**
  Closes a specified file handle.

  @param  This          A pointer to the EFI_FILE_PROTOCOL instance that is the file 
                        handle to close.

  @retval EFI_SUCCESS   The file was closed.

**/
EFI_STATUS
EFIAPI
FfsClose (IN EFI_FILE_PROTOCOL *This)
{
  FILE_PRIVATE_DATA *PrivateFile;

  DEBUG ((EFI_D_INFO, "*** FfsClose: Start of func ***\n"));

  //
  // Grab the associated private data.
  //
  PrivateFile = FILE_PRIVATE_DATA_FROM_THIS (This);

  //
  // Free up all of the private data.
  //
  if (PrivateFile->IsDirectory) {
    FreePool (PrivateFile->DirInfo);
  } else {
    FreePool (PrivateFile->FileInfo);
  }

  FreePool (PrivateFile);

  DEBUG ((EFI_D_INFO, "*** FfsClose: End of func ***\n"));
  return EFI_SUCCESS;
}

/**
  Close and delete the file handle.

  @param  This                     A pointer to the EFI_FILE_PROTOCOL instance that is the
                                   handle to the file to delete.

  @retval EFI_SUCCESS              The file was closed and deleted, and the handle was closed.
  @retval EFI_WARN_DELETE_FAILURE  The handle was closed, but the file was not deleted.

**/
EFI_STATUS
EFIAPI
FfsDelete (IN EFI_FILE_PROTOCOL *This)
{
  DEBUG ((EFI_D_INFO, "*** FfsDelete: Unsupported ***\n"));
  return EFI_UNSUPPORTED;
}

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
EFI_STATUS
EFIAPI
FfsRead (
  IN EFI_FILE_PROTOCOL *This,
  IN OUT UINTN *BufferSize,
  OUT VOID *Buffer
  )
{
  EFI_STATUS                    Status;
  FILE_PRIVATE_DATA             *PrivateFile, *NextFile;
  UINTN                         ReadStart, FileSize, SectionInstance;
  EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv2;
  EFI_GUID                      *NextFileGuid;
  VOID                          *FileContents, *FileReadStart;
  EFI_FV_FILETYPE               FoundType;
  EFI_FV_FILE_ATTRIBUTES        FileAttributes;
  EFI_SECTION_TYPE              SectionType;
  UINT32                        AuthenticationStatus;

  Status = EFI_SUCCESS;
  DEBUG ((EFI_D_INFO, "*** FfsRead: Start of func ***\n"));

  //
  // Grab private data and determine the starting location to read from.
  //
  PrivateFile = FILE_PRIVATE_DATA_FROM_THIS (This);
  Fv2         = PrivateFile->FileSystem->FirmwareVolume2;
  ReadStart   = (UINTN) PrivateFile->Position;

  DEBUG ((EFI_D_INFO, "*** FfsRead: Start reading from %d ***\n", ReadStart));

  // Check filetype.
  if (PrivateFile->IsDirectory) {
    DEBUG ((EFI_D_INFO, "*** FfsRead: Called on directory ***\n"));

    //
    // Ensure that Buffer is large enough to hold the EFI_FILE_INFO struct.
    //
    if (*BufferSize < SIZE_OF_EFI_FILE_INFO + SIZE_OF_GUID) {
      DEBUG ((EFI_D_INFO, "*** FfsRead: Need a larger buffer\n"));
      *BufferSize = SIZE_OF_EFI_FILE_INFO + SIZE_OF_GUID;
      Status = EFI_BUFFER_TOO_SMALL;
      goto ReadDone;
    }

    //
    // Ensure we're not at the end of the directory.
    //
    if (FvGetNumberOfFiles(PrivateFile->FileSystem->FirmwareVolume2) <= ReadStart) {
      DEBUG ((EFI_D_INFO, "*** FfsRead: At end of directory listing\n"));
      *BufferSize = 0;
      Status = EFI_SUCCESS;
      goto ReadDone;
    }

    //
    // Grab the next file in the directory and get its EFI_FILE_INFO.
    //
    NextFileGuid = AllocateZeroPool (sizeof(EFI_GUID));
    NextFileGuid = RootGetNextFile (PrivateFile);
    NextFile     = GuidToFile (NextFileGuid, PrivateFile->FileSystem);

    NextFile->File.GetInfo (
                     &(NextFile->File),
                     &gEfiFileInfoGuid,
                     BufferSize,
                     Buffer);

    //
    // Update the current position to the next directory entry.
    //
    PrivateFile->Position += 1;
  } else {
    DEBUG ((EFI_D_INFO, "*** FfsRead: Called on file ***\n"));

    //
    // Determine how many bytes we will actually read. If the read request is
    // going to go out of bounds, change it to read only to the EOF.
    //
    FileSize = FvFileGetSize (
                 PrivateFile->FileSystem->FirmwareVolume2,
                 &(PrivateFile->FileInfo->NameGuid));

    //
    // Cap off the amount of data read so we don't go past the EOF.
    //
    if (ReadStart + *BufferSize > FileSize) {
      DEBUG ((EFI_D_INFO, "Increasing buffersize for read...\n"));
      *BufferSize = FileSize - ReadStart;
    }

    //
    // Allocate a buffer to use to hold the file's contents.
    //
    FileContents = AllocateZeroPool (FileSize);

    // Read the data.
    if (IsFileExecutable (
          PrivateFile->FileSystem->FirmwareVolume2,
          &(PrivateFile->FileInfo->NameGuid))) {
      //
      // Read executable section.
      //       
      SectionType     = EFI_SECTION_PE32;
      SectionInstance = 0;

      Fv2->ReadSection (
             Fv2,
             &(PrivateFile->FileInfo->NameGuid),
             SectionType,
             SectionInstance,
             &FileContents,
             &FileSize,
             &AuthenticationStatus);
    } else {
      //
      // Read from whole file.
      //
      Fv2->ReadFile (
             Fv2,
             &(PrivateFile->FileInfo->NameGuid),
             &FileContents,
             &FileSize,
             &FoundType,
             &FileAttributes,
             &AuthenticationStatus);
    }

    //
    // Copy the requested segment of data from the file's contents.
    //
    FileReadStart = FileContents + ReadStart;
    CopyMem (Buffer, FileReadStart, *BufferSize);
    FreePool (FileContents);

    //
    // Update the file's position to be the original location added to the
    // number of bytes read.
    //
    PrivateFile->Position = ReadStart + *BufferSize;
  }

  DEBUG ((EFI_D_INFO, "*** FfsRead: End of reading %s ***\n", PrivateFile->FileName));

ReadDone:

  return Status;
}

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

/**
  Returns a file's current position.

  @param  This            A pointer to the EFI_FILE_PROTOCOL instance that is the file
                          handle to get the current position on.
  @param  Position        The address to return the file's current position value.

  @retval EFI_SUCCESS      The position was returned.
  @retval EFI_UNSUPPORTED  The request is not valid on open directories.
  @retval EFI_DEVICE_ERROR An attempt was made to get the position from a deleted file.

**/
EFI_STATUS
EFIAPI
FfsGetPosition (
  IN EFI_FILE_PROTOCOL *This,
  OUT UINT64 *Position
  )
{
  EFI_STATUS        Status;
  FILE_PRIVATE_DATA *PrivateFile;

  Status = EFI_SUCCESS;
  DEBUG ((EFI_D_INFO, "*** FfsGetPosition: Start of func ***\n"));

  //
  // Grab the private data associated with This.
  //
  PrivateFile = FILE_PRIVATE_DATA_FROM_THIS (This);

  //
  // Ensure that this function is not called on a directory.
  //
  if (PrivateFile->IsDirectory) {
    Status = EFI_UNSUPPORTED;
    goto GetPosDone;
  }

  DEBUG ((EFI_D_INFO, "*** FfsGetPosition: End of func ***\n"));
  *Position = PrivateFile->Position;

GetPosDone:

  return Status;
}

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
EFI_STATUS
EFIAPI
FfsSetPosition (
  IN EFI_FILE_PROTOCOL *This,
  IN UINT64 Position
  )
{
  EFI_STATUS        Status;
  FILE_PRIVATE_DATA *PrivateFile;

  DEBUG ((EFI_D_INFO, "*** FfsSetPosition: Start of func ***\n"));

  //
  // Grab private data associated with This.
  //
  PrivateFile = FILE_PRIVATE_DATA_FROM_THIS (This);

  //
  // Check for the invalid condition that This is a directory and the position
  // is non-zero. This has the effect of only allowing directory reads to be 
  // restarted.
  //
  if (PrivateFile->IsDirectory && Position != 0) {
    Status = EFI_UNSUPPORTED;
    goto SetPosDone;
  }

  if (Position == END_OF_FILE_POSITION) {
    //
    // Set to the end-of-file position.
    //
    PrivateFile->Position = FvFileGetSize (PrivateFile->FileSystem->FirmwareVolume2,
                                           &(PrivateFile->FileInfo->NameGuid));
  } else {
    //
    // Set the position normally.
    //
    PrivateFile->Position = Position;

    //
    // Reset the key for GetNextFile as well as setting the position if need be.
    //
    if (PrivateFile->IsDirectory && Position == 0) {
      ZeroMem (
        PrivateFile->DirInfo->Key,
        PrivateFile->FileSystem->FirmwareVolume2->KeySize);
    }
  }

  DEBUG ((EFI_D_INFO, "*** FfsSetPosition: End of func ***\n"));

SetPosDone:

  return EFI_SUCCESS;
}

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
EFI_STATUS
EFIAPI
FfsGetInfo (
  IN EFI_FILE_PROTOCOL *This,
  IN EFI_GUID *InformationType,
  IN OUT UINTN *BufferSize,
  OUT VOID *Buffer
  )
{
  EFI_STATUS           Status;
  EFI_FILE_INFO        *FileInfo;
  EFI_FILE_SYSTEM_INFO *FsInfo;
  FILE_PRIVATE_DATA    *PrivateFile;
  UINTN                DataSize;
  CHAR16               *VolumeLabel, *FileName;

  DEBUG ((EFI_D_INFO, "*** FfsGetInfo: Start of func ***\n"));

  //
  // Grab the associated private data.
  //
  PrivateFile = FILE_PRIVATE_DATA_FROM_THIS (This);

  //
  // Check InformationType to determine what kind of data to return.
  //
  if (CompareGuid (InformationType, &gEfiFileInfoGuid)) {
    DEBUG ((EFI_D_INFO, "*** FfsGetInfo: EFI_FILE_INFO request ***\n"));

    //
    // Determine if the size of Buffer is adequate, and if not, break so we can
    // return an error and calculate the needed size;
    //
    DataSize = SIZE_OF_EFI_FILE_INFO + SIZE_OF_FILENAME;

    if (*BufferSize < DataSize) {
      //
      // Error condition. The buffer size is too small.
      //
      *BufferSize = DataSize;
      Status = EFI_BUFFER_TOO_SMALL;
    } else {
      //
      // Allocate and fill out an EFI_FILE_INFO instance for this file.
      //
      FileInfo                   = AllocateZeroPool (DataSize);
      FileInfo->Size             = DataSize;
      FileInfo->CreateTime       = mModuleLoadTime;
      FileInfo->LastAccessTime   = mModuleLoadTime;
      FileInfo->ModificationTime = mModuleLoadTime;
      FileInfo->Attribute        = EFI_FILE_READ_ONLY;

      //
      // Copy in the file name from private data.
      //
      FileName = AllocateZeroPool (SIZE_OF_FILENAME);
      UnicodeSPrint (FileName,
                     SIZE_OF_FILENAME,
                     L"%s",
                     PrivateFile->FileName);
      StrCpy (FileInfo->FileName, FileName);
      FreePool (FileName);

      //
      // Set the next params based on whether the file is a directory or not.
      //
      if (PrivateFile->IsDirectory) {
        //
        // Calculate size of the directory by summing the filesizes of each
        // file.
        //
        FileInfo->FileSize = FvGetVolumeSize (
                               PrivateFile->FileSystem->FirmwareVolume2);

        //
        // Update the Attributes field to reflect that this file is also a
        // directory.
        //
        FileInfo->Attribute |= EFI_FILE_DIRECTORY;
      } else {
        FileInfo->FileSize = FvFileGetSize (
                               PrivateFile->FileSystem->FirmwareVolume2,
                               &(PrivateFile->FileInfo->NameGuid));
      }

      //
      // Use the same value for PhysicalSize as FileSize calculated beforehand.
      // PhysicalSize is just an analogue of FileSize.
      //
      FileInfo->PhysicalSize = FileInfo->FileSize;

      //
      // Copy the memory to Buffer, set the output value of BufferSize, and
      // free the temporary data structure.
      //
      CopyMem (Buffer, FileInfo, DataSize);
      *BufferSize = DataSize;
      FreePool (FileInfo);
      Status = EFI_SUCCESS;
    }
  } else if (CompareGuid (InformationType, &gEfiFileSystemInfoGuid)) {
    DEBUG ((EFI_D_INFO, "*** FfsGetInfo: EFI_FILE_SYSTEM_INFO request ***\n"));

    //
    // Determine if the size of Buffer is adequate, and if not, break so we can
    // return an error and calculate the needed size.
    //
    DataSize = SIZE_OF_EFI_FILE_SYSTEM_INFO + SIZE_OF_FILENAME;

    if (*BufferSize < DataSize) {
      //
      // Error condition. The buffer passed in is too small to be used by this
      // function. Set the required minimal buffer size and return.
      //
      *BufferSize = DataSize;
      Status = EFI_BUFFER_TOO_SMALL;
    } else {
      //
      // Allocate and fill out an EFI_FILE_INFO instance for this file.
      //
      FsInfo = AllocateZeroPool (DataSize);
      FsInfo->Size = DataSize;
      FsInfo->ReadOnly = TRUE;
      FsInfo->VolumeSize = FvGetVolumeSize (PrivateFile->FileSystem->FirmwareVolume2);
      FsInfo->FreeSpace = 0;
      FsInfo->BlockSize = 512;

      //
      // Generate the volume name. This is of the format "FV2@0x...", where the
      // location in memory of the Fv2 instance replaces "...".
      //
      VolumeLabel = AllocateZeroPool (SIZE_OF_FV_LABEL);
      UnicodeSPrint (VolumeLabel,
                     SIZE_OF_FV_LABEL,
                     L"FV2@0x%x",
                     &(PrivateFile->FileSystem->FirmwareVolume2));
      StrCpy (FsInfo->VolumeLabel, VolumeLabel);
      FreePool (VolumeLabel);
      
      //
      // Copy the memory to Buffer, set the output value of BufferSize, and
      // free the temporary data structure.
      //
      CopyMem (Buffer, FsInfo, DataSize);
      *BufferSize = DataSize;
      FreePool (FsInfo);
      Status = EFI_SUCCESS;
    }
  } else {
    //
    // Invalid InformationType GUID, return that the call is unsupported.
    //
    DEBUG ((EFI_D_INFO, "*** FfsGetInfo: Invalid request ***\n"));
    Status = EFI_UNSUPPORTED;
  }

  return Status;
}

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

/**
  Callback function, notified when new FV2 volumes are mounted in the system.

  @param Event   The EFI_EVENT that triggered this function call.
  @param Context The context in which this function was called.

  @return Nothing.

**/
VOID
EFIAPI
FfsNotificationEvent (
  IN EFI_EVENT Event,
  IN VOID      *Context
  )
{
  UINTN                           BufferSize;
  EFI_STATUS                      Status;
  EFI_HANDLE                      HandleBuffer;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFileSystem;
  FILE_SYSTEM_PRIVATE_DATA        *Private;

  while (TRUE) {
    //
    // Grab the next FV2. If this is there are no more in the buffer, exit the
    // while loop and return.
    //
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

    //
    // Check to see if SimpleFileSystem is already installed on this handle. If
    // the protocol is already installed, skip to the next entry.
    //
    Status = gBS->HandleProtocol (
                    HandleBuffer,
                    &gEfiSimpleFileSystemProtocolGuid,
                    (VOID **)&SimpleFileSystem
                    );

    if (!EFI_ERROR (Status)) {
      continue;
    }

    //
    // Allocate space for the private data structure. If this fails, move on to
    // the next entry.
    //
    Private = AllocateCopyPool (
                sizeof (FILE_SYSTEM_PRIVATE_DATA),
                &mFileSystemPrivateDataTemplate
                );

    if (Private == NULL) {
      continue;
    }

    //
    // Retrieve the FV2 protocol.
    //
    Status = gBS->HandleProtocol (
                    HandleBuffer,
                    &gEfiFirmwareVolume2ProtocolGuid,
                    (VOID **)&Private->FirmwareVolume2
                    );

    ASSERT_EFI_ERROR (Status);

    //
    // Install SimpleFileSystem on the handle.
    //
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

/**
  Entry point for the driver.

  @param ImageHandle ImageHandle of the loaded driver.
  @param SystemTable A Pointer to the EFI System Table.

  @return EFI status code.

**/
EFI_STATUS
EFIAPI
InitializeFfsFileSystem (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
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
