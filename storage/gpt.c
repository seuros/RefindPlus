// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2020-2024 Dayo Akanji
// SPDX-FileCopyrightText: 2014-2021 Roderick W. Smith

#include "gpt.h"
#include "lib.h"
#include "screenmgt.h"

#define BlockIoProtocol gEfiBlockIoProtocolGuid

#define MBR_BOOT_SIGNATURE 0xAA55
#define MBR_TYPE_GPT_PROTECTIVE 0xEE
#define GPT_HEADER_SIGNATURE 0x5452415020494645ULL
#define GPT_REVISION_1_0 0x00010000
#define GPT_ENTRY_SIZE 128

GPT_DATA *gPartitions = NULL;

GPT_DATA *AllocateGptData(VOID)
{
    GPT_DATA *GptData;

    GptData = AllocateZeroPool(sizeof(GPT_DATA));
    if (GptData == NULL) {

        return NULL;
    }

    GptData->Header = AllocateZeroPool(sizeof(GPT_HEADER));
    if (GptData->Header == NULL) {
        ClearGptData(GptData);

        return NULL;
    }

    GptData->ProtectiveMBR = AllocateZeroPool(sizeof(MBR_RECORD));
    if (GptData->ProtectiveMBR == NULL) {
        ClearGptData(GptData);
    }

    return GptData;
}

VOID ClearGptData(GPT_DATA *Data)
{
    if (Data == NULL) {

        return;
    }

    MRD_FREE_POOL(Data->ProtectiveMBR);
    MRD_FREE_POOL(Data->Entries);
    MRD_FREE_POOL(Data->Header);
    MRD_FREE_POOL(Data);
}

static BOOLEAN GptHeaderValid(GPT_DATA *GptData)
{
    BOOLEAN IsValid;
    UINT32 StoredCrcValue;
    UINT32 CrcValue;
    UINTN HeaderSize;

    if (GptData == NULL || GptData->Header == NULL || GptData->ProtectiveMBR == NULL) {
        return FALSE;
    }

    IsValid = (GptData->ProtectiveMBR->MBRSignature == MBR_BOOT_SIGNATURE);
    IsValid = IsValid && ((GptData->ProtectiveMBR->partitions[0].type == MBR_TYPE_GPT_PROTECTIVE) ||
                          (GptData->ProtectiveMBR->partitions[1].type == MBR_TYPE_GPT_PROTECTIVE) ||
                          (GptData->ProtectiveMBR->partitions[2].type == MBR_TYPE_GPT_PROTECTIVE) ||
                          (GptData->ProtectiveMBR->partitions[3].type == MBR_TYPE_GPT_PROTECTIVE));

    IsValid = IsValid && ((GptData->Header->signature == GPT_HEADER_SIGNATURE) &&
                          (GptData->Header->spec_revision == GPT_REVISION_1_0) &&
                          (GptData->Header->entry_size == GPT_ENTRY_SIZE));

    if (!IsValid) {

        return FALSE;
    }

    StoredCrcValue = GptData->Header->header_crc32;
    GptData->Header->header_crc32 = 0;

    HeaderSize = sizeof(GPT_HEADER);
    if (GptData->Header->header_size < HeaderSize) {
        HeaderSize = GptData->Header->header_size;
    }

    CrcValue = CalculateCrc32(GptData->Header, HeaderSize);
    if (CrcValue != StoredCrcValue) {
        IsValid = FALSE;
    }

    GptData->Header->header_crc32 = StoredCrcValue;

    return IsValid;
}

EFI_STATUS ReadGptData(MERIDIAN_VOLUME *Volume, GPT_DATA **Data)
{
    EFI_STATUS Status;
    UINT64 BufferSize;
    UINTN i;
    GPT_DATA *GptData;

    if (Volume == NULL || Data == NULL) {

        return EFI_INVALID_PARAMETER;
    }

    if (Volume->BlockIO == NULL) {
        Status = gBS->HandleProtocol(Volume->DeviceHandle, &BlockIoProtocol,
                                     (VOID **)&(Volume->BlockIO));
        if (EFI_ERROR(Status)) {
            Volume->BlockIO = NULL;
            Print(L"Warning: Can't get BlockIO protocol in ReadGptData().\n");

            return EFI_NOT_READY;
        }
    }

    if (!Volume->BlockIO->Media->MediaPresent || Volume->BlockIO->Media->LogicalPartition) {

        return EFI_NO_MEDIA;
    }

    GptData = AllocateGptData();
    if (GptData == NULL) {

        return EFI_OUT_OF_RESOURCES;
    }

    Status = Volume->BlockIO->ReadBlocks(Volume->BlockIO, Volume->BlockIO->Media->MediaId, 0,
                                         sizeof(MBR_RECORD), (VOID *)GptData->ProtectiveMBR);
    if (EFI_ERROR(Status)) {
        ClearGptData(GptData);

        return Status;
    }

    Status = Volume->BlockIO->ReadBlocks(Volume->BlockIO, Volume->BlockIO->Media->MediaId, 1,
                                         sizeof(GPT_HEADER), GptData->Header);
    if (EFI_ERROR(Status)) {
        ClearGptData(GptData);

        return Status;
    }

    if (!GptHeaderValid(GptData)) {
        ClearGptData(GptData);

        return EFI_UNSUPPORTED;
    }

    BufferSize = (UINT64)(GptData->Header->entry_count) * GPT_ENTRY_SIZE;
    GptData->Entries = AllocatePool(BufferSize);
    if (GptData->Entries == NULL) {
        ClearGptData(GptData);

        return EFI_OUT_OF_RESOURCES;
    }

    Status = Volume->BlockIO->ReadBlocks(Volume->BlockIO, Volume->BlockIO->Media->MediaId,
                                         GptData->Header->entry_lba, BufferSize, GptData->Entries);
    if (EFI_ERROR(Status)) {
        ClearGptData(GptData);

        return Status;
    }

    if (CalculateCrc32(GptData->Entries, BufferSize) != GptData->Header->entry_crc32) {
        ClearGptData(GptData);

        return EFI_CRC_ERROR;
    }

    for (i = 0; i < GptData->Header->entry_count; i++) {
        GptData->Entries[i].name[35] = '\0';
    }

    ClearGptData(*Data);
    *Data = GptData;

    return EFI_SUCCESS;
}

GPT_ENTRY *FindPartWithGuid(EFI_GUID *Guid)
{
    UINTN i;
    GPT_ENTRY *Found;
    GPT_DATA *GptData;

    if (Guid == NULL || gPartitions == NULL) {

        return NULL;
    }

    Found = NULL;
    GptData = gPartitions;
    while (!Found && GptData) {
        i = 0;
        while (!Found && i < GptData->Header->entry_count) {
            if (!GuidsAreEqual((EFI_GUID *)&(GptData->Entries[i].partition_guid), Guid)) {
                i++;
                continue;
            }

            Found = AllocateZeroPool(sizeof(GPT_ENTRY));
            if (Found == NULL) {

                return NULL;
            }

            gBS->CopyMem(Found, &GptData->Entries[i], sizeof(GPT_ENTRY));
        }

        GptData = GptData->NextEntry;
    }

    return Found;
}

VOID ForgetPartitionTables(VOID)
{
    GPT_DATA *Next;

    while (gPartitions != NULL) {
        Next = gPartitions->NextEntry;
        ClearGptData(gPartitions);
        gPartitions = Next;
    }
}

VOID AddPartitionTable(MERIDIAN_VOLUME *Volume)
{
    EFI_STATUS Status;
    GPT_DATA *GptList;
    GPT_DATA *GptData;

    GptData = NULL;
    Status = ReadGptData(Volume, &GptData);
    if (EFI_ERROR(Status)) {
        if (GptData != NULL) {
            ClearGptData(GptData);
        }

        return;
    }

    if (gPartitions == NULL) {
        gPartitions = GptData;
    }
    else {
        GptList = gPartitions;

        while (GptList->NextEntry != NULL) {
            GptList = GptList->NextEntry;
        }

        GptList->NextEntry = GptData;
    }
}
