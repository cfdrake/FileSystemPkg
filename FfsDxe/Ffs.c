/*

*/

#include "Ffs.h"

EFI_DRIVER_BINDING_PROTOCOL gFfsDriverBinding = {
  FfsDriverBindingSupported,
  FfsDriverBindingStart,
  FfsDriverBindingStop,
  0xa,
  NULL,
  NULL
};

EFI_STATUS
EFIAPI
FfsDriverBindingSupported (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   ControllerHandle,
  IN  EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
/*++

Routine Description:

  Check to see if the driver supports a given controller.

Arguments:

  This                - A pointer to an instance of the EFI_DRIVER_BINDING_PROTOCOL.

  ControllerHandle    - EFI handle of the controller to test.

  RemainingDevicePath - Pointer to remaining portion of a device path.

Returns:

  EFI_SUCCESS         - The device specified by ControllerHandle and RemainingDevicePath is supported by the driver
                        specified by This.

  EFI_ALREADY_STARTED - The device specified by ControllerHandle and RemainingDevicePath is already being managed by
                        the driver specified by This.

  EFI_ACCESS_DENIED   - The device specified by ControllerHandle and RemainingDevicePath is already being managed by
                        a different driver or an application that requires exclusive access.

  EFI_UNSUPPORTED     - The device specified by ControllerHandle and RemainingDevicePath is not supported by the
                        driver specified by This.

--*/
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FfsDriverBindingStart (
  IN  EFI_DRIVER_BINDING_PROTOCOL   *This,
  IN  EFI_HANDLE                    ControllerHandle,
  IN  EFI_DEVICE_PATH_PROTOCOL      *RemainingDevicePath
  )
/*++

Routine Description:

  Starts a device controller or a bus controller.

Arguments:

  This                - A pointer to an instance of the EFI_DRIVER_BINDING_PROTOCOL.

  ControllerHandle    - EFI handle of the controller to start.

  RemainingDevicePath - Pointer to remaining portion of a device path.

Returns:

  EFI_SUCCESS           - The device or bus controller has been started.

  EFI_DEVICE_ERROR      - The device could not be started due to a device failure.

  EFI_OUT_OF_RESOURCES  - The request could not be completed due to lack of resources.

--*/
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FfsDriverBindingStop (
  IN  EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN  EFI_HANDLE                   ControllerHandle,
  IN  UINTN                        NumberOfChildren,
  IN  EFI_HANDLE                   *ChildHandleBuffer
  )
/*++

Routine Description:

  TODO: Add function description

Arguments:

  This              - A pointer to an instance of the EFI_DRIVER_BINDING_PROTOCOL.

  ControllerHandle  - A handle to the device to be stopped.

  NumberOfChildren  - The number of child device handles in ChildHandleBuffer.

  ChildHandleBuffer - An array of child device handles to be freed.

Returns:

  EFI_SUCCESS       - The device has been stopped.

  EFI_DEVICE_ERROR  - The device could not be stopped due to a device failure.

--*/
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FfsOpenVolume (
  IN EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *This,
  OUT EFI_FILE_PROTOCOL               **Root
  )
/*++

Routine Description:

  Open the root directory on a volume.

Arguments:

  This  - A pointer to the volume to open.

  Root  - A pointer to storage for the returned opened file handle of the root directory.

Returns:

  EFI_SUCCESS           - The volume was opened.

  EFI_UNSUPPORTED       - The volume does not support the requested file system type.

  EFI_NO_MEDIA          - The device has no media.

  EFI_DEVICE_ERROR      - The device reported an error.

  EFI_VOLUME_CORRUPTED  - The file system structures are corrupted.

  EFI_ACCESS_DENIED     - The service denied access to the file.

  EFI_OUT_OF_RESOURCES  - The file volume could not be opened due to lack of resources.

  EFI_MEDIA_CHANGED     - The device has new media or the media is no longer supported.

--*/
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FfsOpen (
  IN  EFI_FILE_PROTOCOL   *This,
  OUT EFI_FILE_PROTOCOL   **NewHandle,
  IN  CHAR16              *FileName,
  IN  UINT64              OpenMode,
  IN  UINT64              Attributes
  )
/*++

Routine Description:

  Open a file relative to the source file location.

Arguments:

  This        - A pointer to the source file location.

  NewHandle   - Pointer to storage for the new file handle.

  FileName    - Pointer to the file name to be opened.

  OpenMode    - File open mode information.

  Attributes  - File creation attributes.

Returns:

  EFI_SUCCESS           - The file was opened.

  EFI_NOT_FOUND         - The file could not be found in the volume.

  EFI_NO_MEDIA          - The device has no media.

  EFI_MEDIA_CHANGED     - The device has new media or the media is no longer supported.

  EFI_DEVICE_ERROR      - The device reported an error.

  EFI_VOLUME_CORRUPTED  - The file system structures are corrupted.

  EFI_WRITE_PROTECTED   - The volume or file is write protected.

  EFI_ACCESS_DENIED     - The service denied access to the file.

  EFI_OUT_OF_RESOURCES  - Not enough resources were available to open the file.

  EFI_VOLUME_FULL       - There is not enough space left to create the new file.

--*/
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FfsClose (
  IN EFI_FILE_PROTOCOL  *This
  )
/*++

Routine Description:

  Close the specified file handle.

Arguments:

  This  - Pointer to a returned opened file handle.

Returns:

  EFI_SUCCESS - The file handle has been closed.

--*/
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FfsDelete (
  IN EFI_FILE_PROTOCOL  *This
  )
/*++

Routine Description:

  Close and delete a file.

Arguments:

  This  - Pointer to a returned opened file handle.

Returns:

  EFI_SUCCESS             - The file handle was closed and deleted.

  EFI_WARN_DELETE_FAILURE - The handle was closed but could not be deleted.

--*/
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FfsRead (
  IN     EFI_FILE_PROTOCOL  *This,
  IN OUT UINTN              *BufferSize,
  OUT    VOID               *Buffer
  )
/*++

Routine Description:

  Read data from a file.

Arguments:

  This        - Pointer to a returned open file handle.

  BufferSize  - On input, the size of the Buffer.  On output, the number of bytes stored in the Buffer.

  Buffer      - Pointer to the first byte of the read Buffer.

Returns:

  EFI_SUCCESS           - The data was read.

  EFI_NO_MEDIA          - The device has no media.

  EFI_DEVICE_ERROR      - The device reported an error.

  EFI_VOLUME_CORRUPTED  - The file system structures are corrupted.

  EFI_BUFFER_TOO_SMALL  - The supplied buffer size was too small to store the current directory entry.
                          *BufferSize has been updated with the size needed to complete the request.

--*/
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FfsWrite (
  IN     EFI_FILE_PROTOCOL  *This,
  IN OUT UINTN              *BufferSize,
  IN     VOID               *Buffer
  )
/*++

Routine Description:

  Write data to a file.

Arguments:

  This        - Pointer to an opened file handle.

  BufferSize  - On input, the number of bytes in the Buffer to write to the file.  On output, the number of bytes
                of data written to the file.

  Buffer      - Pointer to the first by of data in the buffer to write to the file.

Returns:

  EFI_SUCCESS           - The data was written to the file.

  EFI_UNSUPPORTED       - Writes to an open directory are not supported.

  EFI_NO_MEDIA          - The device has no media.

  EFI_DEVICE_ERROR      - The device reported an error.

  EFI_VOLUME_CORRUPTED  - The file system structures are corrupt.

  EFI_WRITE_PROTECTED   - The file, directory, volume, or device is write protected.

  EFI_ACCESS_DENIED     - The file was opened read-only.

  EFI_VOLUME_FULL       - The volume is full.

--*/
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FfsSetPosition (
  IN EFI_FILE_PROTOCOL  *This,
  IN UINT64             Position
  )
/*++

Routine Description:

  Set a file's current position.

Arguments:

  This      - Pointer to an opened file handle.

  Position  - The byte position from the start of the file to set.

Returns:

  EFI_SUCCESS     - The file position has been changed.

  EFI_UNSUPPORTED - The seek request for non-zero is not supported for directories.

--*/
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FfsGetPosition (
  IN  EFI_FILE_PROTOCOL   *This,
  OUT UINT64              *Position
  )
/*++

Routine Description:

  Get a file's current position.

Arguments:

  This      - Pointer to an opened file handle.

  Position  - Pointer to storage for the current position.

Returns:

  EFI_SUCCESS     - The file position has been reported.

  EFI_UNSUPPORTED - Not valid for directories.

--*/
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FfsGetInfo (
  IN     EFI_FILE_PROTOCOL  *This,
  IN     EFI_GUID           *InformationType,
  IN OUT UINTN              *BufferSize,
  OUT    VOID               *Buffer
  )
/*++

Routine Description:

  Return information about a file or volume.

Arguments:

  This            - Pointer to an opened file handle.

  InformationType - GUID describing the type of information to be returned.

  BufferSize      - On input, the size of the information buffer.  On output, the number of bytes written to the
                    information buffer.

  Buffer          - Pointer to the first byte of the information buffer.

Returns:

  EFI_SUCCESS           - The requested information has been written into the buffer.

  EFI_UNSUPPORTED       - The InformationType is not known.

  EFI_NO_MEDIA          - The device has no media.

  EFI_DEVICE_ERROR      - The device reported an error.

  EFI_VOLUME_CORRUPTED  - The file system structures are corrupt.

  EFI_BUFFER_TOO_SMALL  - The buffer size was too small to contain the requested information.  The buffer size has
                          been updated with the size needed to complete the requested operation.

--*/
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FfsSetInfo (
  IN EFI_FILE_PROTOCOL*This,
  IN EFI_GUID         *InformationType,
  IN UINTN            BufferSize,
  IN VOID             *Buffer
  )
/*++

Routine Description:

  Set information about a file or volume.

Arguments:

  This            - Pointer to an opened file handle.

  InformationType - GUID identifying the type of information to set.

  BufferSize      - Number of bytes of data in the information buffer.

  Buffer          - Pointer to the first byte of data in the information buffer.

Returns:

  EFI_SUCCESS           - The file or volume information has been updated.

  EFI_UNSUPPORTED       - The information identifier is not recognised.

  EFI_NO_MEDIA          - The device has no media.

  EFI_DEVICE_ERROR      - The device reported an error.

  EFI_VOLUME_CORRUPTED  - The file system structures are corrupt.

  EFI_WRITE_PROTECTED   - The file, directory, volume, or device is write protected.

  EFI_ACCESS_DENIED     - The file was opened read-only.

  EFI_VOLUME_FULL       - The volume is full.

  EFI_BAD_BUFFER_SIZE   - The buffer size is smaller than the type indicated by InformationType.

--*/
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
FfsFlush (
  IN EFI_FILE_PROTOCOL  *This
  )
/*++

Routine Description:

  Flush all modified data to the media.

Arguments:

  This  - Pointer to an opened file handle.

Returns:

  EFI_SUCCESS           - The data has been flushed.

  EFI_NO_MEDIA          - The device has no media.

  EFI_DEVICE_ERROR      - The device reported an error.

  EFI_VOLUME_CORRUPTED  - The file system structures have been corrupted.

  EFI_WRITE_PROTECTED   - The file, directory, volume, or device is write protected.

  EFI_ACCESS_DENIED     - The file was opened read-only.

  EFI_VOLUME_FULL       - The volume is full.

--*/
{
  return EFI_UNSUPPORTED;
}
