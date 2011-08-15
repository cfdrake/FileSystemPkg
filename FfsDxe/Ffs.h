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

#ifndef _FFS_H_
#define _FFS_H_

///
/// Required file includes.
///
#include <PiDxe.h>
#include <Guid/FileInfo.h>
#include <Guid/FileSystemInfo.h>
#include <Guid/FileSystemVolumeLabelInfo.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/FirmwareVolume2.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PeCoffGetEntryPointLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Uefi/UefiBaseType.h>

//
// Miscellaneous helpful macros.
//
#define END_OF_FILE_POSITION (0xFFFFFFFFFFFFFFFF)
#define SIZE_OF_GUID         (sizeof (CHAR16) * 37)
#define SIZE_OF_FILENAME     (SIZE_OF_GUID + sizeof (CHAR16) * 4)
#define LENGTH_OF_FILENAME   (40)
#define SIZE_OF_FV_LABEL     (sizeof (CHAR16) * 15)

//
// Forward-declared typedefs for later data structures.
//
typedef struct _FILE_SYSTEM_PRIVATE_DATA FILE_SYSTEM_PRIVATE_DATA;
typedef struct _FILE_PRIVATE_DATA        FILE_PRIVATE_DATA;
typedef struct _DIR_INFO                 DIR_INFO;
typedef struct _FILE_INFO                FILE_INFO;

///
/// Signature to identify FILE_SYSTEM_PRIVATE_DATA instances.
///
#define FILE_SYSTEM_PRIVATE_DATA_SIGNATURE (SIGNATURE_32 ('f', 'f', 's', 't'))

///
/// Private data structure for filesystem instances. For each mounted
/// EFI_SIMPLE_FILE_SYSTEM_PROTOCOL instance in the filesystem, there will be
/// one corresponding FILE_SYSTEM_PRIVATE_DATA instance to hold associated data.
///
struct _FILE_SYSTEM_PRIVATE_DATA {
  UINT32                          Signature;        ///< Datatype signature.

  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL SimpleFileSystem; ///< Holds the SFS interface.
  EFI_FIRMWARE_VOLUME2_PROTOCOL   *FirmwareVolume2; ///< Pointer to the filesystem's FV2 instance.
  FILE_PRIVATE_DATA               *Root;            ///< Pointer to the root directory of the filesystem.
};

///
/// Macro to grab the FILE_SYSTEM_PRIVATE_DATA instance associated with a given
/// pointer to an EFI_SIMPLE_FILE_SYSTEM_PROTOCOL.
///
#define FILE_SYSTEM_PRIVATE_DATA_FROM_THIS(a) CR (a, FILE_SYSTEM_PRIVATE_DATA, SimpleFileSystem, FILE_SYSTEM_PRIVATE_DATA_SIGNATURE)

///
/// Signature to identify FILE_PRIVATE_DATA instances.
///
#define FILE_PRIVATE_DATA_SIGNATURE (SIGNATURE_32 ('f', 'f', 's', 'f'))

///
/// Private data structure for file instances. For each EFI_FILE_PROTOCOL instance
/// created in the filesystem, there will be one corresponding FILE_PRIVATE_DATA
/// instance to hold associated data.
///
struct _FILE_PRIVATE_DATA {
  UINT32                   Signature;   ///< Datatype signature.

  EFI_FILE_PROTOCOL        File;        ///< Holds the EFI_FILE_PROTOCOL interface.
  FILE_SYSTEM_PRIVATE_DATA *FileSystem; ///< Pointer to the file's filesystem instance.

  CHAR16                   *FileName;   ///< String name used to reference the file.

  BOOLEAN                  IsDirectory; ///< Determines if the file is a directory or not.
  DIR_INFO                 *DirInfo;    ///< Associated information for directories.
  FILE_INFO                *FileInfo;   ///< Associated information for files.

  UINT64                   Position;    ///< Number of bytes to offset calls to Read() by.
};

///
/// Macro to grab the FILE_PRIVATE_DATA instance associated with a given
/// pointer to an EFI_FILE_PROTOCOL.
///
#define FILE_PRIVATE_DATA_FROM_THIS(a) CR (a, FILE_PRIVATE_DATA, File, FILE_PRIVATE_DATA_SIGNATURE)

///
/// Directory information datatype. This data structure has members that give
/// more specific information about FILE_PRIVATE_DATA instances that are
/// directories rather than files.
///
struct _DIR_INFO {
  LIST_ENTRY Children; ///< List of children files in this directory.
  VOID       *Key;     ///< Search key to keep track of directory index position.
};

///
/// File information datatype. This data structure has members that give more
/// specific information about FILE_PRIVATE_DATA instances that are files rather
/// than directories.
///
struct _FILE_INFO {
  BOOLEAN  IsExecutable; ///< Determines if the file has an executable section or not.
  EFI_GUID NameGuid;     ///< The EFI_GUID that represents the file in it's FV2 instance.
};

//
// SimpleFileSystem and File protocol functions
//

EFI_STATUS
EFIAPI
FfsOpenVolume (
  IN EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *This,
  OUT EFI_FILE_PROTOCOL               **Root
  )
;

EFI_STATUS
EFIAPI
FfsOpen (
  IN  EFI_FILE_PROTOCOL *This,
  OUT EFI_FILE_PROTOCOL **NewHandle,
  IN  CHAR16            *FileName,
  IN  UINT64            OpenMode,
  IN  UINT64            Attributes
  )
;

EFI_STATUS
EFIAPI
FfsClose (IN EFI_FILE_PROTOCOL *This)
;

EFI_STATUS
EFIAPI
FfsDelete (IN EFI_FILE_PROTOCOL *This)
;

EFI_STATUS
EFIAPI
FfsRead (
  IN EFI_FILE_PROTOCOL *This,
  IN OUT UINTN *BufferSize,
  OUT VOID *Buffer
  )
;

EFI_STATUS
EFIAPI
FfsWrite (
  IN EFI_FILE_PROTOCOL *This,
  IN OUT UINTN *BufferSize,
  IN VOID *Buffer
  )
;

EFI_STATUS
EFIAPI
FfsGetPosition (
  IN EFI_FILE_PROTOCOL *This,
  OUT UINT64 *Position
  )
;

EFI_STATUS
EFIAPI
FfsSetPosition (
  IN EFI_FILE_PROTOCOL *This,
  IN UINT64 Position
  )
;

EFI_STATUS
EFIAPI
FfsGetInfo (
  IN EFI_FILE_PROTOCOL *This,
  IN EFI_GUID *InformationType,
  IN OUT UINTN *BufferSize,
  OUT VOID *Buffer
  )
;

EFI_STATUS
EFIAPI
FfsSetInfo (
  IN EFI_FILE_PROTOCOL *This,
  IN EFI_GUID *InformationType,
  IN UINTN BufferSize,
  IN VOID *Buffer
  )
;

EFI_STATUS
EFIAPI
FfsFlush (IN EFI_FILE_PROTOCOL *This)
;

#endif  // _FFS_H_
