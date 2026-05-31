// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "limine_boot.h"
#include "lib.h"

#define LIMINE_PAGE_SIZE 0x1000ULL
#define LIMINE_LARGE_PAGE_SIZE 0x200000ULL

#define PTE_PRESENT 0x001ULL
#define PTE_WRITE 0x002ULL
#define PTE_PWT 0x008ULL
#define PTE_PCD 0x010ULL
#define PTE_LARGE 0x080ULL

#define PTE_PAT_LARGE 0x1000ULL
#define PTE_WC_LARGE (PTE_PAT_LARGE | PTE_PWT)

#define PTE_ADDR_MASK 0x000ffffffffff000ULL

#define PML4_INDEX(Virt) (((Virt) >> 39) & 0x1ff)
#define PDPT_INDEX(Virt) (((Virt) >> 30) & 0x1ff)
#define PD_INDEX(Virt) (((Virt) >> 21) & 0x1ff)
#define PT_INDEX(Virt) (((Virt) >> 12) & 0x1ff)

#define LIMINE_MAX_DIRECT_MAP (512ULL * 1024 * 1024 * 1024)

#define LIMINE_TABLE_POOL_SLACK 32

static UINT64 AllocTable(IN OUT LIMINE_CTX *Ctx)
{
    UINT64 Page;

    if (Ctx->TablePoolUsed >= Ctx->TablePoolPages) {
        return 0;
    }

    Page = Ctx->TablePoolPhys + ((UINT64)Ctx->TablePoolUsed * LIMINE_PAGE_SIZE);
    Ctx->TablePoolUsed++;

    ZeroMem((VOID *)(UINTN)Page, (UINTN)LIMINE_PAGE_SIZE);

    return Page;
}

static UINT64 NextLevel(IN OUT LIMINE_CTX *Ctx, IN UINT64 TablePhys, IN UINTN Index)
{
    UINT64 *Table;
    UINT64 Next;

    Table = (UINT64 *)(UINTN)TablePhys;

    if ((Table[Index] & PTE_PRESENT) != 0) {
        return Table[Index] & PTE_ADDR_MASK;
    }

    Next = AllocTable(Ctx);
    if (Next == 0) {
        return 0;
    }

    Table[Index] = Next | PTE_PRESENT | PTE_WRITE;

    return Next;
}

EFI_STATUS LimineMapPage(IN OUT LIMINE_CTX *Ctx, IN UINT64 Virt, IN UINT64 Phys, IN UINT64 Flags,
                         IN BOOLEAN Large)
{
    UINT64 Pdpt;
    UINT64 Pd;
    UINT64 Pt;
    UINT64 *Table;

    if (Ctx == NULL || Ctx->Pml4Phys == 0) {
        return EFI_INVALID_PARAMETER;
    }

    Pdpt = NextLevel(Ctx, Ctx->Pml4Phys, (UINTN)PML4_INDEX(Virt));
    if (Pdpt == 0) {
        return EFI_OUT_OF_RESOURCES;
    }

    Pd = NextLevel(Ctx, Pdpt, (UINTN)PDPT_INDEX(Virt));
    if (Pd == 0) {
        return EFI_OUT_OF_RESOURCES;
    }

    if (Large) {
        Table = (UINT64 *)(UINTN)Pd;
        Table[PD_INDEX(Virt)] = (Phys & ~(LIMINE_LARGE_PAGE_SIZE - 1)) | Flags | PTE_LARGE;

        return EFI_SUCCESS;
    }

    Pt = NextLevel(Ctx, Pd, (UINTN)PD_INDEX(Virt));
    if (Pt == 0) {
        return EFI_OUT_OF_RESOURCES;
    }

    Table = (UINT64 *)(UINTN)Pt;
    Table[PT_INDEX(Virt)] = (Phys & PTE_ADDR_MASK) | Flags;

    return EFI_SUCCESS;
}

static UINT64 HighestPhysicalAddress(VOID)
{
    EFI_STATUS Status;
    EFI_MEMORY_DESCRIPTOR *Map;
    EFI_MEMORY_DESCRIPTOR *Desc;
    UINTN MapSize;
    UINTN MapKey;
    UINTN DescSize;
    UINT32 DescVersion;
    UINTN Offset;
    UINT64 Highest;

    MapSize = 0;
    MapKey = 0;
    DescSize = 0;
    DescVersion = 0;
    Highest = 0;

    Status = gBS->GetMemoryMap(&MapSize, NULL, &MapKey, &DescSize, &DescVersion);
    if (Status != EFI_BUFFER_TOO_SMALL) {
        return 0;
    }

    MapSize += DescSize * 16;

    Map = AllocatePool(MapSize);
    if (Map == NULL) {
        return 0;
    }

    Status = gBS->GetMemoryMap(&MapSize, Map, &MapKey, &DescSize, &DescVersion);
    if (EFI_ERROR(Status)) {
        MRD_FREE_POOL(Map);

        return 0;
    }

    for (Offset = 0; Offset + DescSize <= MapSize; Offset += DescSize) {
        UINT64 End;

        Desc = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)Map + Offset);

        switch (Desc->Type) {
        case EfiLoaderCode:
        case EfiLoaderData:
        case EfiBootServicesCode:
        case EfiBootServicesData:
        case EfiRuntimeServicesCode:
        case EfiRuntimeServicesData:
        case EfiConventionalMemory:
        case EfiACPIReclaimMemory:
        case EfiACPIMemoryNVS:
        case EfiPersistentMemory:
            break;

        default:

            continue;
        }

        End = Desc->PhysicalStart + (Desc->NumberOfPages * LIMINE_PAGE_SIZE);

        if (End > Highest) {
            Highest = End;
        }
    }

    MRD_FREE_POOL(Map);

    return (Highest + LIMINE_LARGE_PAGE_SIZE - 1) & ~(LIMINE_LARGE_PAGE_SIZE - 1);
}

EFI_STATUS LimineBuildPageTables(IN OUT LIMINE_CTX *Ctx)
{
    EFI_STATUS Status;
    EFI_PHYSICAL_ADDRESS Pool;
    UINTN PoolPages;
    UINT64 Highest;
    UINT64 Offset;
    UINT64 *Pml4;
    UINT64 FbStart;
    UINT64 FbEnd;

    if (Ctx == NULL || Ctx->ImagePhys == 0) {
        return EFI_INVALID_PARAMETER;
    }

    Ctx->HhdmOffset = LIMINE_HHDM_BASE;

    Highest = HighestPhysicalAddress();
    if (Highest == 0) {
        return EFI_DEVICE_ERROR;
    }

    INFO_LOG("Limine: direct map ceiling %lx", Highest);

    if (Highest > LIMINE_MAX_DIRECT_MAP) {

        return EFI_UNSUPPORTED;
    }

    PoolPages = (UINTN)(Highest / (1024ULL * 1024 * 1024)) + LIMINE_TABLE_POOL_SLACK +
                (UINTN)(Ctx->ImageSize / LIMINE_LARGE_PAGE_SIZE);

    Pool = 0;
    Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, PoolPages, &Pool);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Ctx->TablePoolPhys = (UINT64)Pool;
    Ctx->TablePoolPages = PoolPages;
    Ctx->TablePoolUsed = 0;

    Ctx->Pml4Phys = AllocTable(Ctx);
    if (Ctx->Pml4Phys == 0) {
        return EFI_OUT_OF_RESOURCES;
    }

    for (Offset = 0; Offset < Highest; Offset += LIMINE_LARGE_PAGE_SIZE) {
        Status =
            LimineMapPage(Ctx, LIMINE_HHDM_BASE + Offset, Offset, PTE_PRESENT | PTE_WRITE, TRUE);
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    if (Ctx->FbPhys != 0 && Ctx->FbSize != 0) {
        FbStart = Ctx->FbPhys & ~(LIMINE_LARGE_PAGE_SIZE - 1);
        FbEnd = (Ctx->FbPhys + Ctx->FbSize + LIMINE_LARGE_PAGE_SIZE - 1) &
                ~(LIMINE_LARGE_PAGE_SIZE - 1);

        if (FbEnd <= LIMINE_MAX_DIRECT_MAP) {
            for (Offset = FbStart; Offset < FbEnd; Offset += LIMINE_LARGE_PAGE_SIZE) {
                Status = LimineMapPage(Ctx, LIMINE_HHDM_BASE + Offset, Offset,
                                       PTE_PRESENT | PTE_WRITE | PTE_WC_LARGE, TRUE);
                if (EFI_ERROR(Status)) {
                    return Status;
                }
            }
        }
    }

    Pml4 = (UINT64 *)(UINTN)Ctx->Pml4Phys;
    Pml4[0] = Pml4[PML4_INDEX(LIMINE_HHDM_BASE)];

    for (Offset = 0; Offset < Ctx->ImageSize; Offset += LIMINE_PAGE_SIZE) {
        Status = LimineMapPage(Ctx, Ctx->ImageVirt + Offset, Ctx->ImagePhys + Offset,
                               PTE_PRESENT | PTE_WRITE, FALSE);
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    return EFI_SUCCESS;
}
