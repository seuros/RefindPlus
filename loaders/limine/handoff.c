// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#include "limine_boot.h"
#include "lib.h"

// The Limine handoff is long-mode specific: MSR writes, a hand-built GDT and an
// iretq into the kernel. The protocol port is x64-only by design, so on other
// architectures the loader compiles down to a refusal rather than a broken
// build. Everything else under loaders/limine is architecture-neutral.
#if defined(MDE_CPU_X64)

#define LIMINE_PAGE_SIZE 0x1000ULL

#define MSR_IA32_PAT 0x277
#define MSR_IA32_EFER 0xc0000080
#define MSR_IA32_FS_BASE 0xc0000100
#define MSR_IA32_GS_BASE 0xc0000101
#define MSR_IA32_KERNEL_GS_BASE 0xc0000102

#define EFER_NXE 0x800ULL

#define LIMINE_PAT_VALUE 0x0007010500070406ULL

#define GDT_ENTRIES 7

#pragma pack(1)
typedef struct
{
    UINT16 Limit;
    UINT64 Base;
} GDT_DESCRIPTOR;
#pragma pack()

static EFI_STATUS BuildGdt(IN OUT LIMINE_CTX *Ctx)
{
    EFI_STATUS Status;
    EFI_PHYSICAL_ADDRESS Page;
    UINT64 *Gdt;

    Page = 0;

    Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 1, &Page);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Gdt = (UINT64 *)(UINTN)Page;
    ZeroMem(Gdt, (UINTN)LIMINE_PAGE_SIZE);

    Gdt[0] = 0x0000000000000000ULL;
    Gdt[1] = 0x00009a000000ffffULL;
    Gdt[2] = 0x000092000000ffffULL;
    Gdt[3] = 0x00cf9a000000ffffULL;
    Gdt[4] = 0x00cf92000000ffffULL;
    Gdt[5] = 0x00af9a000000ffffULL;
    Gdt[6] = 0x00af92000000ffffULL;

    Ctx->GdtPhys = (UINT64)Page;

    return EFI_SUCCESS;
}

static EFI_STATUS ExitBootServicesWithMap(IN OUT LIMINE_CTX *Ctx, OUT UINTN *MapSize,
                                          OUT UINTN *DescSize)
{
    EFI_STATUS Status;
    UINTN MapKey;
    UINT32 DescVersion;
    UINTN Attempt;

    for (Attempt = 0; Attempt < 4; Attempt++) {
        *MapSize = Ctx->EfiMemmapBufferSize;
        MapKey = 0;
        *DescSize = 0;
        DescVersion = 0;

        Status = gBS->GetMemoryMap(MapSize, (EFI_MEMORY_DESCRIPTOR *)Ctx->EfiMemmapBuffer, &MapKey,
                                   DescSize, &DescVersion);
        if (EFI_ERROR(Status)) {
            return Status;
        }

        Status = gBS->ExitBootServices(SelfImageHandle, MapKey);
        if (!EFI_ERROR(Status)) {
            return EFI_SUCCESS;
        }

        if (Status != EFI_INVALID_PARAMETER) {
            return Status;
        }
    }

    return EFI_INVALID_PARAMETER;
}

static VOID SetMachineState(VOID)
{
    UINT64 Efer;

    AsmWriteMsr64(MSR_IA32_PAT, LIMINE_PAT_VALUE);

    Efer = AsmReadMsr64(MSR_IA32_EFER);
    AsmWriteMsr64(MSR_IA32_EFER, Efer | EFER_NXE);

    AsmWriteMsr64(MSR_IA32_FS_BASE, 0);
    AsmWriteMsr64(MSR_IA32_GS_BASE, 0);
    AsmWriteMsr64(MSR_IA32_KERNEL_GS_BASE, 0);

    IoWrite8(0x21, 0xff);
    IoWrite8(0xa1, 0xff);

    __asm__ __volatile__("mov %%cr0, %%rax\n\t"
                         "or $0x10000, %%rax\n\t"
                         "mov %%rax, %%cr0\n\t"
                         :
                         :
                         : "rax", "memory");
}

EFI_STATUS LimineHandoff(IN OUT LIMINE_CTX *Ctx)
{
    EFI_STATUS Status;
    GDT_DESCRIPTOR *Gdtr;
    EFI_PHYSICAL_ADDRESS Scratch;
    UINTN MapSize;
    UINTN DescSize;
    UINT64 StackTop;
    UINT64 *StackSlot;

    if (Ctx == NULL || Ctx->Pml4Phys == 0 || Ctx->StackPhys == 0 || Ctx->EntryPoint == 0) {
        return EFI_INVALID_PARAMETER;
    }

    Status = BuildGdt(Ctx);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Scratch = 0;
    Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, 17, &Scratch);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Gdtr = (GDT_DESCRIPTOR *)(UINTN)Scratch;
    Gdtr->Limit = (GDT_ENTRIES * sizeof(UINT64)) - 1;
    Gdtr->Base = Ctx->GdtPhys;

    Ctx->EfiMemmapBuffer = (VOID *)(UINTN)(Scratch + LIMINE_PAGE_SIZE);
    Ctx->EfiMemmapBufferSize = (UINTN)(16 * LIMINE_PAGE_SIZE);

    StackSlot = (UINT64 *)(UINTN)(Ctx->StackPhys + Ctx->StackSize - sizeof(UINT64));
    *StackSlot = 0;

    StackTop = Ctx->StackPhys + Ctx->HhdmOffset + Ctx->StackSize - sizeof(UINT64);

    MapSize = 0;
    DescSize = 0;

    Status = ExitBootServicesWithMap(Ctx, &MapSize, &DescSize);
    if (EFI_ERROR(Status)) {

        return Status;
    }

    LimineFillMemmap(Ctx, (EFI_MEMORY_DESCRIPTOR *)Ctx->EfiMemmapBuffer, MapSize, DescSize);

    SetMachineState();

    __asm__ __volatile__("cli\n\t"
                         "lgdt (%[gdtr])\n\t"
                         "mov %[cr3], %%cr3\n\t"

                         "mov $0x30, %%eax\n\t"
                         "mov %%eax, %%ds\n\t"
                         "mov %%eax, %%es\n\t"
                         "mov %%eax, %%fs\n\t"
                         "mov %%eax, %%gs\n\t"
                         "mov %%eax, %%ss\n\t"

                         "pushq $0x30\n\t"
                         "pushq %[stack]\n\t"
                         "pushq $0x2\n\t"
                         "pushq $0x28\n\t"
                         "pushq %[entry]\n\t"

                         "xor %%eax, %%eax\n\t"
                         "xor %%ebx, %%ebx\n\t"
                         "xor %%ecx, %%ecx\n\t"
                         "xor %%edx, %%edx\n\t"
                         "xor %%esi, %%esi\n\t"
                         "xor %%edi, %%edi\n\t"
                         "xor %%ebp, %%ebp\n\t"
                         "xor %%r8d, %%r8d\n\t"
                         "xor %%r9d, %%r9d\n\t"
                         "xor %%r10d, %%r10d\n\t"
                         "xor %%r11d, %%r11d\n\t"
                         "xor %%r12d, %%r12d\n\t"
                         "xor %%r13d, %%r13d\n\t"
                         "xor %%r14d, %%r14d\n\t"
                         "xor %%r15d, %%r15d\n\t"

                         "iretq\n\t"
                         :
                         : [gdtr] "r"(Gdtr), [cr3] "r"(Ctx->Pml4Phys), [stack] "r"(StackTop),
                           [entry] "r"(Ctx->EntryPoint)

                         : "rax", "memory");

    return EFI_LOAD_ERROR;
}

#else

EFI_STATUS LimineHandoff(IN OUT LIMINE_CTX *Ctx)
{
    (VOID) Ctx;
    return EFI_UNSUPPORTED;
}

#endif
