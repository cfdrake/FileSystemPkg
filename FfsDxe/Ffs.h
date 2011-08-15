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

#define END_OF_FILE_POSITION (0xFFFFFFFFFFFFFFFF)
#define SIZE_OF_GUID         (sizeof (CHAR16) * 37)
#define SIZE_OF_FILENAME     (SIZE_OF_GUID + sizeof (CHAR16) * 4)
#define LENGTH_OF_FILENAME   (40)
#define SIZE_OF_FV_LABEL     (sizeof (CHAR16) * 15)

//
// Typedefs
//

typedef struct _FILE_SYSTEM_PRIVATE_DATA FILE_SYSTEM_PRIVATE_DATA;
typedef struct _FILE_PRIVATE_DATA FILE_PRIVATE_DATA;
typedef struct _DIR_INFO DIR_INFO;
typedef struct _FILE_INFO FILE_INFO;

//
// Private data structure for File System
//

#define FILE_SYSTEM_PRIVATE_DATA_SIGNATURE SIGNATURE_32 ('f', 'f', 's', 't')

struct _FILE_SYSTEM_PRIVATE_DATA {
  UINT32                          Signature;

  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL SimpleFileSystem;
  EFI_FIRMWARE_VOLUME2_PROTOCOL   *FirmwareVolume2;
  FILE_PRIVATE_DATA               *Root;
};

#define FILE_SYSTEM_PRIVATE_DATA_FROM_THIS(a) CR (a, FILE_SYSTEM_PRIVATE_DATA, SimpleFileSystem, FILE_SYSTEM_PRIVATE_DATA_SIGNATURE)

//
// Private data structure for File
//

#define FILE_PRIVATE_DATA_SIGNATURE SIGNATURE_32 ('f', 'f', 's', 'f')

struct _FILE_PRIVATE_DATA {
  UINT32                   Signature;

  EFI_FILE_PROTOCOL        File;
  FILE_SYSTEM_PRIVATE_DATA *FileSystem;

  CHAR16                   *FileName;

  BOOLEAN                  IsDirectory;
  DIR_INFO                 *DirInfo;
  FILE_INFO                *FileInfo;

  UINT64                   Position;
};

struct _DIR_INFO {
  LIST_ENTRY Children;
  VOID       *Key;
};

struct _FILE_INFO {
  BOOLEAN  IsExecutable;
  EFI_GUID NameGuid;
};

#define FILE_PRIVATE_DATA_FROM_THIS(a) CR (a, FILE_PRIVATE_DATA, File, FILE_PRIVATE_DATA_SIGNATURE)

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
