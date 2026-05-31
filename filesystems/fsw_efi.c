// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2021-2026 Dayo Akanji
// SPDX-FileCopyrightText: 2006 Christoph Pfisterer

#include "fsw_efi.h"
#include "fsw_core.h"
#define MERIDIAN_EFI_DRIVER_BINDING_PROTOCOL EFI_DRIVER_BINDING_PROTOCOL
#define MERIDIAN_EFI_COMPONENT_NAME_PROTOCOL EFI_COMPONENT_NAME_PROTOCOL
#define MERIDIAN_EFI_COMPONENT_NAME_PROTOCOL_GUID EFI_COMPONENT_NAME_PROTOCOL_GUID
#define MERIDIAN_EFI_DRIVER_BINDING_PROTOCOL_GUID EFI_DRIVER_BINDING_PROTOCOL_GUID
#define MERIDIAN_EFI_DEVICE_PATH_PROTOCOL EFI_DEVICE_PATH_PROTOCOL
#define EFI_FILE_SYSTEM_VOLUME_LABEL_INFO_ID    \
    { 0xDB47D7D3,0xFE81, 0x11d3, {0x9A, 0x35, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D} }
#define gMyEfiSimpleFileSystemProtocolGuid gEfiSimpleFileSystemProtocolGuid

#include "version.h"

#define DEBUG_LEVEL 0

EFI_GUID gMyEfiDriverBindingProtocolGuid       = MERIDIAN_EFI_DRIVER_BINDING_PROTOCOL_GUID;
EFI_GUID gMyEfiComponentNameProtocolGuid       = MERIDIAN_EFI_COMPONENT_NAME_PROTOCOL_GUID;
EFI_GUID gMyEfiDiskIoProtocolGuid              = MERIDIAN_EFI_DISK_IO_PROTOCOL_GUID;
EFI_GUID gMyEfiBlockIoProtocolGuid             = MERIDIAN_EFI_BLOCK_IO_PROTOCOL_GUID;
EFI_GUID gMyEfiFileInfoGuid                    = EFI_FILE_INFO_ID;
EFI_GUID gMyEfiFileSystemInfoGuid              = EFI_FILE_SYSTEM_INFO_ID;
EFI_GUID gMyEfiFileSystemVolumeLabelInfoIdGuid = EFI_FILE_SYSTEM_VOLUME_LABEL_INFO_ID;

#define FSW_EFI_DRIVER_NAME_PREFIX  L"Meridian v" MERIDIAN_VERSION L" Filesystem Driver:- "

EFI_STATUS EFIAPI fsw_efi_driver_binding_supported (
    IN MERIDIAN_EFI_DRIVER_BINDING_PROTOCOL  *This,
    IN EFI_HANDLE                               ControllerHandle,
    IN MERIDIAN_EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
);
EFI_STATUS EFIAPI fsw_efi_driver_binding_start (
    IN MERIDIAN_EFI_DRIVER_BINDING_PROTOCOL  *This,
    IN EFI_HANDLE                               ControllerHandle,
    IN MERIDIAN_EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
);
EFI_STATUS EFIAPI fsw_efi_driver_binding_stop (
    IN MERIDIAN_EFI_DRIVER_BINDING_PROTOCOL  *This,
    IN EFI_HANDLE                               ControllerHandle,
    IN UINTN                                    NumberOfChildren,
    IN EFI_HANDLE                              *ChildHandleBuffer
);
EFI_STATUS EFIAPI fsw_efi_ComponentName_GetDriverName (
    IN  MERIDIAN_EFI_COMPONENT_NAME_PROTOCOL   *This,
    IN  CHAR8                                    *Language,
    OUT CHAR16                                  **DriverName
);
EFI_STATUS EFIAPI fsw_efi_ComponentName_GetControllerName (
    IN  MERIDIAN_EFI_COMPONENT_NAME_PROTOCOL  *This,
    IN  EFI_HANDLE                               ControllerHandle,
    IN  EFI_HANDLE                               ChildHandle  OPTIONAL,
    IN  CHAR8                                   *Language,
    OUT CHAR16                                 **ControllerName
);
void EFIAPI fsw_efi_change_blocksize (
    struct fsw_volume *vol,
    fsw_u32 old_phys_blocksize,
    fsw_u32 old_log_blocksize,
    fsw_u32 new_phys_blocksize,
    fsw_u32 new_log_blocksize
);
fsw_status_t EFIAPI fsw_efi_read_block (
    struct fsw_volume *vol,
    fsw_u64 phys_bno,
    void *buffer
);
EFI_STATUS fsw_efi_map_status (
    fsw_status_t     fsw_status,
    FSW_VOLUME_DATA *Volume
);
EFI_STATUS EFIAPI fsw_efi_FileSystem_OpenVolume (
    IN EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *This,
    OUT EFI_FILE_PROTOCOL              **Root
);
EFI_STATUS fsw_efi_dnode_to_filehandle (
    IN  struct fsw_dnode   *dno,
    OUT EFI_FILE_PROTOCOL **NewFileHandle
);
EFI_STATUS fsw_efi_file_read (
    IN FSW_FILE_DATA *File,
    IN OUT UINTN *BufferSize,
    OUT VOID *Buffer
);
EFI_STATUS fsw_efi_file_getpos (
    IN FSW_FILE_DATA *File,
    OUT UINT64 *Position
);
EFI_STATUS fsw_efi_file_setpos (
    IN FSW_FILE_DATA *File,
    IN UINT64 Position
);
EFI_STATUS fsw_efi_dir_open (
    IN FSW_FILE_DATA        *File,
    OUT EFI_FILE_PROTOCOL  **NewHandle,
    IN CHAR16               *FileName,
    IN UINT64                OpenMode,
    IN UINT64                Attributes
);
EFI_STATUS fsw_efi_dir_read (
    IN FSW_FILE_DATA *File,
    IN OUT UINTN *BufferSize,
    OUT VOID *Buffer
);
EFI_STATUS fsw_efi_dir_setpos (
    IN FSW_FILE_DATA *File,
    IN UINT64 Position
);
EFI_STATUS fsw_efi_dnode_getinfo (
    IN FSW_FILE_DATA *File,
    IN EFI_GUID *InformationType,
    IN OUT UINTN *BufferSize,
    OUT VOID *Buffer
);
EFI_STATUS fsw_efi_dnode_fill_FileInfo (
    IN FSW_VOLUME_DATA *Volume,
    IN struct fsw_dnode *dno,
    IN OUT UINTN *BufferSize,
    OUT VOID *Buffer
);

#define CACHE_SIZE 131072
struct cache_data {
   fsw_u8           *Cache;
   fsw_u64           CacheStart;
   BOOLEAN           CacheValid;
   FSW_VOLUME_DATA  *Volume;
};

#define NUM_CACHES 2
static struct cache_data    Caches[NUM_CACHES];
static int LastRead = -1;

MERIDIAN_EFI_DRIVER_BINDING_PROTOCOL fsw_efi_driver_binding_table = {
    fsw_efi_driver_binding_supported,
    fsw_efi_driver_binding_start,
    fsw_efi_driver_binding_stop,
    0x10, NULL, NULL
};

MERIDIAN_EFI_COMPONENT_NAME_PROTOCOL fsw_efi_component_name_table = {
    fsw_efi_ComponentName_GetDriverName,
    fsw_efi_ComponentName_GetControllerName,
    (CHAR8*) "eng"
};

struct fsw_host_table   fsw_efi_host_table = {
    FSW_STRING_TYPE_UTF16,
    fsw_efi_change_blocksize,
    fsw_efi_read_block
};

VOID EFIAPI fsw_efi_clear_cache (VOID) {
   int i;

   for (i = 0; i < NUM_CACHES; i++) {
      if (Caches[i].Cache != NULL) {
         FreePool (Caches[i].Cache);
         Caches[i].Cache = NULL;
      }

      Caches[i].CacheStart = 0;
      Caches[i].CacheValid = FALSE;
      Caches[i].Volume     = NULL;
   }
   LastRead = -1;
}

EFI_STATUS EFIAPI fsw_efi_main (
    IN EFI_HANDLE          ImageHandle,
    IN EFI_SYSTEM_TABLE   *SystemTable
) {
    EFI_STATUS  Status;

    fsw_efi_driver_binding_table.ImageHandle         = ImageHandle;
    fsw_efi_driver_binding_table.DriverBindingHandle = ImageHandle;

    Status = gBS->InstallProtocolInterface(&fsw_efi_driver_binding_table.DriverBindingHandle, &gMyEfiDriverBindingProtocolGuid, EFI_NATIVE_INTERFACE, &fsw_efi_driver_binding_table);
    if (EFI_ERROR(Status)) return Status;

    Status = gBS->InstallProtocolInterface(&fsw_efi_driver_binding_table.DriverBindingHandle, &gMyEfiComponentNameProtocolGuid, EFI_NATIVE_INTERFACE, &fsw_efi_component_name_table);

    return Status;
}

EFI_STATUS EFIAPI fsw_efi_driver_binding_supported (
    IN MERIDIAN_EFI_DRIVER_BINDING_PROTOCOL  *This,
    IN EFI_HANDLE                               ControllerHandle,
    IN MERIDIAN_EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
) {
    EFI_STATUS            Status;
    EFI_DISK_IO_PROTOCOL *DiskIo;

    Status = gBS->OpenProtocol(ControllerHandle, &gMyEfiDiskIoProtocolGuid, (VOID **) &DiskIo, This->DriverBindingHandle, ControllerHandle, EFI_OPEN_PROTOCOL_BY_DRIVER);
    if (EFI_ERROR(Status)) return Status;

    gBS->CloseProtocol(ControllerHandle, &gMyEfiDiskIoProtocolGuid, This->DriverBindingHandle, ControllerHandle);

    {
        EFI_BLOCK_IO_PROTOCOL *BlockIo;
        Status = gBS->OpenProtocol(ControllerHandle, &gMyEfiBlockIoProtocolGuid, (VOID **) &BlockIo, This->DriverBindingHandle, ControllerHandle, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
        if (EFI_ERROR(Status)) return Status;

        if (!BlockIo->Media->LogicalPartition) {
            return EFI_UNSUPPORTED;
        }
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
fsw_efi_driver_binding_start (
    IN MERIDIAN_EFI_DRIVER_BINDING_PROTOCOL  *This,
    IN EFI_HANDLE                               ControllerHandle,
    IN MERIDIAN_EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
) {
    EFI_STATUS             Status;
    EFI_BLOCK_IO_PROTOCOL *BlockIo;
    EFI_DISK_IO_PROTOCOL  *DiskIo;
    FSW_VOLUME_DATA       *Volume;

    Status = gBS->OpenProtocol(ControllerHandle, &gMyEfiBlockIoProtocolGuid, (VOID **) &BlockIo, This->DriverBindingHandle, ControllerHandle, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(Status)) return Status;

    Status = gBS->OpenProtocol(ControllerHandle, &gMyEfiDiskIoProtocolGuid, (VOID **) &DiskIo, This->DriverBindingHandle, ControllerHandle, EFI_OPEN_PROTOCOL_BY_DRIVER);
    if (EFI_ERROR(Status)) return Status;

    Volume = AllocateZeroPool (sizeof (FSW_VOLUME_DATA));
    if (Volume == NULL) return EFI_BUFFER_TOO_SMALL;

    Volume->Signature       = FSW_VOLUME_DATA_SIGNATURE;
    Volume->Handle          = ControllerHandle;
    Volume->DiskIo          = DiskIo;
    Volume->MediaId         = BlockIo->Media->MediaId;
    Volume->LastIOStatus    = EFI_SUCCESS;

    Status = fsw_efi_map_status (
        fsw_mount (
            Volume,
            &fsw_efi_host_table,
            fsw_active_fstype_table,
            &Volume->vol
        ),
        Volume
    );
    if (!EFI_ERROR(Status)) {

        Volume->FileSystem.Revision     = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_REVISION;
        Volume->FileSystem.OpenVolume   = fsw_efi_FileSystem_OpenVolume;

        Status = gBS->InstallMultipleProtocolInterfaces(&ControllerHandle, &gMyEfiSimpleFileSystemProtocolGuid, &Volume->FileSystem, NULL);
    }

    if (EFI_ERROR(Status)) {
        if (Volume->vol != NULL) {
            fsw_unmount (Volume->vol);
        }
        FreePool (Volume);

        gBS->CloseProtocol(ControllerHandle, &gMyEfiDiskIoProtocolGuid, This->DriverBindingHandle, ControllerHandle);
    }

    return Status;
}

EFI_STATUS EFIAPI fsw_efi_driver_binding_stop (
    IN  MERIDIAN_EFI_DRIVER_BINDING_PROTOCOL  *This,
    IN  EFI_HANDLE                   ControllerHandle,
    IN  UINTN                        NumberOfChildren,
    IN  EFI_HANDLE                  *ChildHandleBuffer
) {
    EFI_STATUS                       Status;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    FSW_VOLUME_DATA                 *Volume;

    Status = gBS->OpenProtocol(ControllerHandle, &gMyEfiSimpleFileSystemProtocolGuid, (VOID **) &FileSystem, This->DriverBindingHandle, ControllerHandle, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(Status)) return EFI_UNSUPPORTED;

    Volume = FSW_VOLUME_FROM_FILE_SYSTEM(FileSystem);

    Status = gBS->UninstallMultipleProtocolInterfaces(ControllerHandle, &gMyEfiSimpleFileSystemProtocolGuid, &Volume->FileSystem, NULL);
    if (EFI_ERROR(Status)) return Status;

    if (Volume->vol != NULL) {
        fsw_unmount (Volume->vol);
    }
    FreePool (Volume);

    Status = gBS->CloseProtocol(ControllerHandle, &gMyEfiDiskIoProtocolGuid, This->DriverBindingHandle, ControllerHandle);

    fsw_efi_clear_cache();

    return Status;
}

EFI_STATUS EFIAPI fsw_efi_ComponentName_GetDriverName (
    IN  MERIDIAN_EFI_COMPONENT_NAME_PROTOCOL  *This,
    IN  CHAR8                                   *Language,
    OUT CHAR16                                 **DriverName
) {

    static CHAR16 *mDriverName = NULL;

    if (Language == NULL || DriverName == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    if (Language[0] == 'e' && Language[1] == 'n' && Language[2] == 'g' && Language[3] == 0) {
        if (mDriverName == NULL) {
            mDriverName = CatSPrint (NULL, L"%s%s", FSW_EFI_DRIVER_NAME_PREFIX, fsw_active_fstype_name);
            if (mDriverName == NULL) {
                return EFI_OUT_OF_RESOURCES;
            }
        }
        *DriverName = mDriverName;
        return EFI_SUCCESS;
    }

    return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI fsw_efi_ComponentName_GetControllerName (
    IN  MERIDIAN_EFI_COMPONENT_NAME_PROTOCOL    *This,
    IN  EFI_HANDLE                                 ControllerHandle,
    IN  EFI_HANDLE                                 ChildHandle  OPTIONAL,
    IN  CHAR8                                     *Language,
    OUT CHAR16                                   **ControllerName
) {
    return EFI_UNSUPPORTED;
}

void EFIAPI fsw_efi_change_blocksize (
    struct fsw_volume *vol,
    fsw_u32 old_phys_blocksize,
    fsw_u32 old_log_blocksize,
    fsw_u32 new_phys_blocksize,
    fsw_u32 new_log_blocksize)
{

    return;
}

fsw_status_t EFIAPI fsw_efi_read_block (
    struct fsw_volume *vol,
    fsw_u64            phys_bno,
    void              *buffer
) {
   int              i, ReadCache = -1;
   FSW_VOLUME_DATA  *Volume = (FSW_VOLUME_DATA *)vol->host_data;
   EFI_STATUS       Status = EFI_SUCCESS;
   BOOLEAN          ReadOneBlock = FALSE;
   UINT64           StartRead = (UINT64) phys_bno * (UINT64) vol->phys_blocksize;

   if (buffer == NULL) {
       FSW_MSG_L03((
           FSW_MSG_STR(
               "FSW_EFI: fsw_efi_read_block ... Leaving with Status: 'EFI_BAD_BUFFER_SIZE'\n"
           )
       ));

       return (fsw_status_t) EFI_BAD_BUFFER_SIZE;
   }

   if (LastRead < 0) fsw_efi_clear_cache();

   i = 0;
   do {
      if ((Caches[i].Volume == Volume) &&
          Caches[i].CacheValid &&
          (StartRead >= Caches[i].CacheStart) &&
          ((StartRead + vol->phys_blocksize) <= (Caches[i].CacheStart + CACHE_SIZE))) {
         ReadCache = i;
      }
      i++;
   } while ((i < NUM_CACHES) && (ReadCache < 0));

   if (ReadCache < 0) {
      if (LastRead == -1) LastRead = 1;
      ReadCache = 1 - LastRead;

      Caches[ReadCache].CacheValid = FALSE;
      if (Caches[ReadCache].Cache == NULL) {
          Caches[ReadCache].Cache = AllocatePool (CACHE_SIZE);
      }

      if (Caches[ReadCache].Cache == NULL) {
         ReadOneBlock = TRUE;
      }
      else {
         Status = Volume->DiskIo->ReadDisk(Volume->DiskIo, Volume->MediaId, StartRead, (UINTN) CACHE_SIZE, (VOID*) Caches[ReadCache].Cache);

         if (EFI_ERROR(Status)) {
            ReadOneBlock = TRUE;
         }
         else {
            Caches[ReadCache].CacheStart = StartRead;
            Caches[ReadCache].CacheValid = TRUE;
            Caches[ReadCache].Volume     = Volume;
            LastRead                     = ReadCache;
         }
      }
   }

   if (vol->phys_blocksize > 0      &&
       Caches[ReadCache].CacheValid &&
       Caches[ReadCache].Cache != NULL
   ) {
      CopyMem (
          buffer,
          &Caches[ReadCache].Cache[StartRead - Caches[ReadCache].CacheStart],
          vol->phys_blocksize
      );
   }
   else {
      ReadOneBlock = TRUE;
   }

   if (ReadOneBlock) {

      Status = Volume->DiskIo->ReadDisk(Volume->DiskIo, Volume->MediaId, phys_bno * vol->phys_blocksize, (UINTN) vol->phys_blocksize, (VOID*) buffer);
   }

   Volume->LastIOStatus = Status;

   FSW_MSG_L03((
       FSW_MSG_STR(
           "FSW_EFI: fsw_efi_read_block ... Leaving with Status: '%r'\n"
       ), Status
   ));

   return Status;
}

EFI_STATUS fsw_efi_map_status (
    fsw_status_t fsw_status,
    FSW_VOLUME_DATA *Volume
) {
    switch (fsw_status) {
        case FSW_SUCCESS:          return EFI_SUCCESS;
        case FSW_NOT_FOUND:        return EFI_NOT_FOUND;
        case FSW_UNSUPPORTED:      return EFI_UNSUPPORTED;
        case FSW_IO_ERROR:         return Volume->LastIOStatus;
        case FSW_OUT_OF_MEMORY:    return EFI_VOLUME_CORRUPTED;
        case FSW_VOLUME_CORRUPTED: return EFI_VOLUME_CORRUPTED;
        default:                   return EFI_DEVICE_ERROR;
    }
}

EFI_STATUS EFIAPI fsw_efi_FileSystem_OpenVolume (
    IN  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *This,
    OUT EFI_FILE_PROTOCOL               **Root
) {
    EFI_STATUS          Status;
    FSW_VOLUME_DATA     *Volume = FSW_VOLUME_FROM_FILE_SYSTEM(This);

    fsw_efi_clear_cache();
    Status = fsw_efi_dnode_to_filehandle (
        Volume->vol->root, Root
    );

    return Status;
}

EFI_STATUS EFIAPI fsw_efi_filehandle_open (
    IN  EFI_FILE_PROTOCOL  *This,
    OUT EFI_FILE_PROTOCOL **NewHandle,
    IN  CHAR16             *FileName,
    IN  UINT64              OpenMode,
    IN  UINT64              Attributes
) {
    EFI_STATUS          Status;
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);

    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_EFI: fsw_efi_filehandle_open ... Open File Handle: '%s'\n"
        ), FileName
    ));

    if (File->Type != FSW_EFI_FILE_TYPE_DIR) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_filehandle_open ... Error: 'File->Type != FSW_EFI_FILE_TYPE_DIR'\n"
            )
        ));

        Status = EFI_UNSUPPORTED;
    }
    else {
        Status = fsw_efi_dir_open (
            File, NewHandle, FileName,
            OpenMode, Attributes
        );
    }

    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_EFI: fsw_efi_filehandle_open ... Leaving with Status: '%r'\n"
        ), Status
    ));

    return Status;
}

EFI_STATUS EFIAPI fsw_efi_filehandle_close (
    IN EFI_FILE_PROTOCOL *This
) {
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);

    fsw_shandle_close (&File->shand);
    FreePool (File);

    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI fsw_efi_filehandle_delete (
    IN EFI_FILE_PROTOCOL *This
) {
    EFI_STATUS          Status;

    Status = This->Close(This);
    if (!EFI_ERROR(Status)) {

        Status = EFI_WARN_DELETE_FAILURE;
    }

    return Status;
}

EFI_STATUS EFIAPI fsw_efi_filehandle_read (
    IN     EFI_FILE_PROTOCOL *This,
    IN OUT UINTN             *BufferSize,
       OUT VOID              *Buffer
) {
    EFI_STATUS          Status = EFI_UNSUPPORTED;
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);

    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_EFI: fsw_efi_filehandle_read ... Read File Handle: '%s'\n"
        ), File->shand.dnode->name.data
    ));

    if (File->Type == FSW_EFI_FILE_TYPE_FILE) {
        Status = fsw_efi_file_read (File, BufferSize, Buffer);
    }
    else {
        if (File->Type == FSW_EFI_FILE_TYPE_DIR) {
            Status = fsw_efi_dir_read (File, BufferSize, Buffer);
        }
    }

    #if FSW_DEBUG_LEVEL >= 2
    if (EFI_ERROR(Status)) {
        FSW_MSG_L02((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_filehandle_read ... Leaving with Status: '%r'\n"
            ), Status
        ));
    }
    else {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_filehandle_read ... Leaving with Status: 'SUCCESS'\n"
            )
        ));
    }
    #endif

    return Status;
}

EFI_STATUS EFIAPI fsw_efi_filehandle_write (
    IN     EFI_FILE_PROTOCOL *This,
    IN OUT UINTN             *BufferSize,
    IN     VOID              *Buffer
) {

    return EFI_WRITE_PROTECTED;
}

EFI_STATUS EFIAPI fsw_efi_filehandle_position_get (
    IN  EFI_FILE_PROTOCOL *This,
    OUT UINT64            *Position
) {
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);

    if (File->Type == FSW_EFI_FILE_TYPE_FILE) {
        return fsw_efi_file_getpos (File, Position);
    }

    return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI fsw_efi_filehandle_position_set (
    IN EFI_FILE_PROTOCOL *This,
    IN UINT64             Position
) {
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);

    if (File->Type == FSW_EFI_FILE_TYPE_FILE) {
        return fsw_efi_file_setpos (File, Position);
    }
    else if (File->Type == FSW_EFI_FILE_TYPE_DIR) {
        return fsw_efi_dir_setpos (File, Position);
    }

    return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI fsw_efi_filehandle_info_get (
    IN     EFI_FILE_PROTOCOL *This,
    IN     EFI_GUID          *InformationType,
    IN OUT UINTN             *BufferSize,
    OUT    VOID              *Buffer
) {
    FSW_FILE_DATA      *File = FSW_FILE_FROM_FILE_HANDLE(This);

    return fsw_efi_dnode_getinfo (
        File, InformationType,
        BufferSize, Buffer
    );
}

EFI_STATUS EFIAPI fsw_efi_filehandle_info_set (
    IN EFI_FILE_PROTOCOL *This,
    IN EFI_GUID          *InformationType,
    IN UINTN              BufferSize,
    IN VOID              *Buffer
) {

    return EFI_WRITE_PROTECTED;
}

EFI_STATUS EFIAPI fsw_efi_filehandle_flush (
    IN EFI_FILE_PROTOCOL *This
) {

    return EFI_WRITE_PROTECTED;
}

EFI_STATUS fsw_efi_dnode_to_filehandle (
    IN  struct fsw_dnode   *dno,
    OUT EFI_FILE_PROTOCOL **NewFileHandle
) {
    EFI_STATUS          Status;
    FSW_FILE_DATA       *File;

    Status = fsw_efi_map_status (
        fsw_dnode_fill (dno),
        (FSW_VOLUME_DATA *) dno->vol->host_data
    );
    if (EFI_ERROR(Status)) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dnode_to_filehandle ... Leaving with Status: '%r'\n"
            ), Status
        ));

        return Status;
    }

    if (dno->type != FSW_DNODE_TYPE_DIR &&
        dno->type != FSW_DNODE_TYPE_FILE

    ) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dnode_to_filehandle ... Leaving with Status: 'EFI_UNSUPPORTED'\n"
            )
        ));

        return EFI_UNSUPPORTED;
    }

    File = AllocateZeroPool (sizeof (FSW_FILE_DATA));
    if (File == NULL) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dnode_to_filehandle ... Leaving with Status: 'EFI_BUFFER_TOO_SMALL'\n"
            )
        ));

        return EFI_BUFFER_TOO_SMALL;
    }

    File->Signature = FSW_FILE_DATA_SIGNATURE;
    if (dno->type == FSW_DNODE_TYPE_FILE) {
        File->Type = FSW_EFI_FILE_TYPE_FILE;
    }
    else {
        if (dno->type == FSW_DNODE_TYPE_DIR) {
            File->Type = FSW_EFI_FILE_TYPE_DIR;
        }
    }

    Status = fsw_efi_map_status (
        fsw_shandle_open (
            dno, &File->shand
        ),
        (FSW_VOLUME_DATA *) dno->vol->host_data
    );
    if (EFI_ERROR(Status)) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dnode_to_filehandle ... Leaving with Status: '%r'\n"
            ), Status
        ));

        FreePool (File);
        return Status;
    }

    File->FileHandle.Revision    = EFI_FILE_HANDLE_REVISION;
    File->FileHandle.Flush       = fsw_efi_filehandle_flush;
    File->FileHandle.Close       = fsw_efi_filehandle_close;
    File->FileHandle.Open        = fsw_efi_filehandle_open;
    File->FileHandle.Read        = fsw_efi_filehandle_read;
    File->FileHandle.Write       = fsw_efi_filehandle_write;
    File->FileHandle.Delete      = fsw_efi_filehandle_delete;
    File->FileHandle.GetInfo     = fsw_efi_filehandle_info_get;
    File->FileHandle.SetInfo     = fsw_efi_filehandle_info_set;
    File->FileHandle.GetPosition = fsw_efi_filehandle_position_get;
    File->FileHandle.SetPosition = fsw_efi_filehandle_position_set;

    *NewFileHandle = &File->FileHandle;

    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_EFI: fsw_efi_dnode_to_filehandle ... Leaving with Status: 'EFI_SUCCESS'\n"
        )
    ));

    return EFI_SUCCESS;
}

EFI_STATUS fsw_efi_file_read (
    IN     FSW_FILE_DATA *File,
    IN OUT UINTN         *BufferSize,
    OUT    VOID          *Buffer
) {
    EFI_STATUS          Status;
    fsw_u32             buffer_size;
    fsw_status_t        fsw_status;

    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_EFI: fsw_efi_file_read ... Read File: '%s'\n"
        ), File->shand.dnode->name.data
    ));

    buffer_size = (fsw_u32)*BufferSize;
    fsw_status = fsw_shandle_read (
        &File->shand, &buffer_size, Buffer
    );
    Status = fsw_efi_map_status (
        fsw_status,
        (FSW_VOLUME_DATA *)File->shand.dnode->vol->host_data
    );
    *BufferSize = buffer_size;

    #if FSW_DEBUG_LEVEL >= 1
    if (EFI_ERROR(Status)) {
        FSW_MSG_L01((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_file_read ... Leaving with Status: '%r'\n"
            ), Status
        ));
    }
    else {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_file_read ... Leaving with Status: 'SUCCESS'\n"
            )
        ));
    }
    #endif

    return Status;
}

EFI_STATUS fsw_efi_file_getpos (
    IN  FSW_FILE_DATA *File,
    OUT UINT64        *Position
) {
    *Position = File->shand.pos;
    return EFI_SUCCESS;
}

EFI_STATUS fsw_efi_file_setpos (
    IN FSW_FILE_DATA *File,
    IN UINT64         Position
) {
    File->shand.pos = (
        Position != 0xFFFFFFFFFFFFFFFFULL
    ) ? Position : File->shand.dnode->size;

    return EFI_SUCCESS;
}

EFI_STATUS fsw_efi_dir_open (
    IN  FSW_FILE_DATA          *File,
    OUT EFI_FILE_PROTOCOL     **NewHandle,
    IN  CHAR16                 *FileName,
    IN  UINT64                  OpenMode,
    IN  UINT64                  Attributes
) {
    EFI_STATUS          Status;
    FSW_VOLUME_DATA    *Volume;
    struct fsw_dnode   *dno;
    struct fsw_dnode   *target_dno;
    struct fsw_string   lookup_path;

    if (OpenMode != EFI_FILE_MODE_READ) {
        FSW_MSG_L02((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dir_open ... Leaving with Status: '%r'\n"
            ), EFI_WRITE_PROTECTED
        ));

        return EFI_WRITE_PROTECTED;
    }

    lookup_path.type = FSW_STRING_TYPE_UTF16;
    lookup_path.len  = (int)StrLen (FileName);
    lookup_path.size = lookup_path.len * sizeof (fsw_u16);
    lookup_path.data = FileName;

    Volume = (FSW_VOLUME_DATA *)File->shand.dnode->vol->host_data;

    Status = fsw_efi_map_status (
        fsw_dnode_lookup_path (
            File->shand.dnode,
            &lookup_path,
            '\\', &dno
        ),
        Volume
    );
    if (EFI_ERROR(Status)) {
        FSW_MSG_L02((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dir_open ... Leaving with Status: '%r'\n"
            ), Status
        ));

        return Status;
    }

    Status = fsw_efi_map_status (
        fsw_dnode_resolve (
            dno, &target_dno
        ),
        Volume
    );
    fsw_dnode_release (dno);
    if (EFI_ERROR(Status)) {
        FSW_MSG_L02((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dir_open ... Leaving with Status: '%r'\n"
            ), Status
        ));

        return Status;
    }
    dno = target_dno;

    Status = fsw_efi_dnode_to_filehandle (dno, NewHandle);
    fsw_dnode_release (dno);
    return Status;
}

EFI_STATUS fsw_efi_dir_read (
    IN     FSW_FILE_DATA *File,
    IN OUT UINTN         *BufferSize,
    OUT    VOID          *Buffer
) {
    EFI_STATUS           Status;
    FSW_VOLUME_DATA     *Volume = (FSW_VOLUME_DATA *)File->shand.dnode->vol->host_data;
    struct fsw_dnode    *dno;

    Status = fsw_efi_map_status (
        fsw_dnode_dir_read (
            &File->shand, &dno
        ), Volume
    );
    if (Status == EFI_NOT_FOUND) {

        *BufferSize = 0;

        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dir_read ... No More Entries\n"
            )
        ));

        return EFI_SUCCESS;
    }
    if (EFI_ERROR(Status)) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dir_read ... Leaving with Status: '%r'\n"
            ), Status
        ));

        return Status;
    }

    Status = fsw_efi_dnode_fill_FileInfo (
        Volume, dno,
        BufferSize, Buffer
    );

    fsw_dnode_release (dno);

    #if FSW_DEBUG_LEVEL >= 1
    if (EFI_ERROR(Status)) {
        FSW_MSG_L01((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dir_read ... Leaving with Status: '%r'\n"
            ), Status
        ));
    }
    else {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dir_read ... Leaving with Status: 'SUCCESS'\n"
            )
        ));
    }
    #endif

    return Status;
}

EFI_STATUS fsw_efi_dir_setpos (
    IN FSW_FILE_DATA *File,
    IN UINT64         Position
) {
    if (Position == 0) {
        File->shand.pos = 0;
        return EFI_SUCCESS;
    }
    else {

        return EFI_UNSUPPORTED;
    }
}

EFI_STATUS fsw_efi_dnode_getinfo (
    IN     FSW_FILE_DATA *File,
    IN     EFI_GUID      *InformationType,
    IN OUT UINTN         *BufferSize,
    OUT    VOID          *Buffer
) {
    EFI_STATUS             Status;
    FSW_VOLUME_DATA       *Volume = (FSW_VOLUME_DATA *)File->shand.dnode->vol->host_data;
    EFI_FILE_SYSTEM_INFO  *FSInfo;
    UINTN                  RequiredSize;
    struct fsw_volume_stat vsb;

    if (CompareGuid (InformationType, &gMyEfiFileInfoGuid)) {
        Status = fsw_efi_dnode_fill_FileInfo (
            Volume, File->shand.dnode,
            BufferSize, Buffer
        );
    }
    else if (
        CompareGuid (
            InformationType,
            &gMyEfiFileSystemInfoGuid
        )
    ) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dnode_getinfo ... FILE_SYSTEM_INFO\n"
            )
        ));

        RequiredSize = fsw_efi_strsize (
            &Volume->vol->label
        ) + SIZE_OF_EFI_FILE_SYSTEM_INFO;
        if (*BufferSize < RequiredSize) {
            *BufferSize = RequiredSize;
            return EFI_BUFFER_TOO_SMALL;
        }

        FSInfo = (EFI_FILE_SYSTEM_INFO *)Buffer;
        FSInfo->Size        = RequiredSize;
        FSInfo->ReadOnly    = TRUE;
        FSInfo->BlockSize   = Volume->vol->log_blocksize;
        fsw_efi_strcpy (FSInfo->VolumeLabel, &Volume->vol->label);

        ZeroMem (&vsb, sizeof (struct fsw_volume_stat));
        Status = fsw_efi_map_status (
            fsw_volume_stat (
                Volume->vol, &vsb
            ),
            Volume
        );
        if (EFI_ERROR(Status)) {
            FSW_MSG_L03((
                FSW_MSG_STR(
                    "FSW_EFI: fsw_efi_dnode_getinfo ... Leaving with Status: '%r'\n"
                ), Status
            ));

            return Status;
        }

        FSInfo->VolumeSize  = vsb.total_bytes;
        FSInfo->FreeSpace   = vsb.free_bytes;

        *BufferSize = RequiredSize;
        Status = EFI_SUCCESS;

    }
    else if (
        CompareGuid (
            InformationType,
            &gMyEfiFileSystemVolumeLabelInfoIdGuid
        )
    ) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dnode_getinfo ... FILE_SYSTEM_VOLUME_LABEL\n"
            )
        ));

        RequiredSize = fsw_efi_strsize (
            &Volume->vol->label
        ) + SIZE_OF_EFI_FILE_SYSTEM_VOLUME_LABEL_INFO;
        if (*BufferSize < RequiredSize) {
            *BufferSize = RequiredSize;
            return EFI_BUFFER_TOO_SMALL;
        }

        fsw_efi_strcpy (
            ((EFI_FILE_SYSTEM_VOLUME_LABEL_INFO *) Buffer)->VolumeLabel,
            &Volume->vol->label
        );

        *BufferSize = RequiredSize;
        Status = EFI_SUCCESS;
    }
    else {
        Status = EFI_UNSUPPORTED;
    }

    return Status;
}

void fsw_store_time_posix (
    struct fsw_dnode_stat *sb,
    int                    which,
    fsw_u32                posix_time
) {
    EFI_FILE_INFO       *FileInfo = (EFI_FILE_INFO *)sb->host_data;

    if (0);
    else if (which == FSW_DNODE_STAT_CTIME) fsw_efi_decode_time (&FileInfo->CreateTime,       posix_time);
    else if (which == FSW_DNODE_STAT_MTIME) fsw_efi_decode_time (&FileInfo->ModificationTime, posix_time);
    else if (which == FSW_DNODE_STAT_ATIME) fsw_efi_decode_time (&FileInfo->LastAccessTime,   posix_time);
}

void fsw_store_attr_posix (
    struct fsw_dnode_stat *sb,
    fsw_u16                posix_mode
) {
    EFI_FILE_INFO       *FileInfo = (EFI_FILE_INFO *)sb->host_data;

    if ((posix_mode & S_IWUSR) == 0) {
        FileInfo->Attribute |= EFI_FILE_READ_ONLY;
    }
}

void fsw_store_attr_efi (
    struct fsw_dnode_stat *sb,
    fsw_u16                attr
) {
    EFI_FILE_INFO       *FileInfo = (EFI_FILE_INFO *)sb->host_data;

    FileInfo->Attribute |= attr;
}

EFI_STATUS fsw_efi_dnode_fill_FileInfo (
    IN FSW_VOLUME_DATA  *Volume,
    IN struct fsw_dnode *dno,
    IN OUT UINTN        *BufferSize,
    OUT VOID            *Buffer
) {
    EFI_STATUS            Status;
    EFI_FILE_INFO        *FileInfo;
    UINTN                 RequiredSize;
    struct fsw_dnode_stat sb;

    Status = fsw_efi_map_status (
        fsw_dnode_fill (dno), Volume
    );
    if (EFI_ERROR(Status)) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dnode_fill_FileInfo ... Leaving with Status: '%r'\n"
            ), Status
        ));

        return Status;
    }

    RequiredSize = SIZE_OF_EFI_FILE_INFO + fsw_efi_strsize (
        &dno->name
    );
    if (*BufferSize < RequiredSize) {
        Status = EFI_BUFFER_TOO_SMALL;

        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dnode_fill_FileInfo ... Leaving with Status: '%r'\n"
            ), Status
        ));

        *BufferSize = RequiredSize;

        return Status;
    }

    ZeroMem (Buffer, RequiredSize);
    FileInfo = (EFI_FILE_INFO *)Buffer;
    FileInfo->Size              = RequiredSize;
    FileInfo->FileSize          = dno->size;
    FileInfo->Attribute         = 0;

    if (dno->type == FSW_DNODE_TYPE_DIR) {
        FileInfo->Attribute |= EFI_FILE_DIRECTORY;
    }

    fsw_efi_strcpy (FileInfo->FileName, &dno->name);

    ZeroMem (&sb, sizeof (struct fsw_dnode_stat));
    sb.host_data = FileInfo;

    Status = fsw_efi_map_status (
        fsw_dnode_stat (dno, &sb),
        Volume
    );
    if (EFI_ERROR(Status)) {
        FSW_MSG_L03((
            FSW_MSG_STR(
                "FSW_EFI: fsw_efi_dnode_fill_FileInfo ... Leaving with Status: '%r'\n"
            ), Status
        ));

        return Status;
    }

    FileInfo->PhysicalSize = sb.used_bytes;

    *BufferSize = RequiredSize;
    FSW_MSG_L03((
        FSW_MSG_STR(
            "FSW_EFI: fsw_efi_dnode_fill_FileInfo ... Returning '%s'\n"
        ), FileInfo->FileName
    ));

    return EFI_SUCCESS;
}
