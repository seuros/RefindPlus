// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2024 Dayo Akanji
// SPDX-FileCopyrightText: Intel Corporation

#ifndef __PE_IMAGE_H__
#define __PE_IMAGE_H__

#define SIGNATURE_16(A, B)        ((A) | (B << 8))
#define SIGNATURE_32(A, B, C, D)  (SIGNATURE_16 (A, B) | (SIGNATURE_16 (C, D) << 16))
#define SIGNATURE_64(A, B, C, D, E, F, G, H) \
	(SIGNATURE_32 (A, B, C, D) | ((UINT64) (SIGNATURE_32 (E, F, G, H)) << 32))

#define ALIGN_VALUE(Value, Alignment) ((Value) + (((Alignment) - (Value)) & ((Alignment) - 1)))
#define ALIGN_POINTER(Pointer, Alignment) ((VOID *) (ALIGN_VALUE ((UINTN)(Pointer), (Alignment))))

#define EFI_IMAGE_SUBSYSTEM_EFI_APPLICATION         10
#define EFI_IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER 11
#define EFI_IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER      12
#define EFI_IMAGE_SUBSYSTEM_SAL_RUNTIME_DRIVER      13

#define IMAGE_FILE_MACHINE_I386            0x014c
#define IMAGE_FILE_MACHINE_IA64            0x0200
#define IMAGE_FILE_MACHINE_EBC             0x0EBC
#define IMAGE_FILE_MACHINE_X64             0x8664
#define IMAGE_FILE_MACHINE_ARMTHUMB_MIXED  0x01c2

#define EFI_IMAGE_DOS_SIGNATURE     SIGNATURE_16('M', 'Z')
#define EFI_IMAGE_OS2_SIGNATURE     SIGNATURE_16('N', 'E')
#define EFI_IMAGE_OS2_SIGNATURE_LE  SIGNATURE_16('L', 'E')
#define EFI_IMAGE_NT_SIGNATURE      SIGNATURE_32('P', 'E', '\0', '\0')

typedef struct {
  UINT16  e_magic;
  UINT16  e_cblp;
  UINT16  e_cp;
  UINT16  e_crlc;
  UINT16  e_cparhdr;
  UINT16  e_minalloc;
  UINT16  e_maxalloc;
  UINT16  e_ss;
  UINT16  e_sp;
  UINT16  e_csum;
  UINT16  e_ip;
  UINT16  e_cs;
  UINT16  e_lfarlc;
  UINT16  e_ovno;
  UINT16  e_res[4];
  UINT16  e_oemid;
  UINT16  e_oeminfo;
  UINT16  e_res2[10];
  UINT32  e_lfanew;
} EFI_IMAGE_DOS_HEADER;

typedef struct {
  UINT16  Machine;
  UINT16  NumberOfSections;
  UINT32  TimeDateStamp;
  UINT32  PointerToSymbolTable;
  UINT32  NumberOfSymbols;
  UINT16  SizeOfOptionalHeader;
  UINT16  Characteristics;
} EFI_IMAGE_FILE_HEADER;

#define EFI_IMAGE_SIZEOF_FILE_HEADER        20

#define EFI_IMAGE_FILE_RELOCS_STRIPPED      (1 << 0)
#define EFI_IMAGE_FILE_EXECUTABLE_IMAGE     (1 << 1)
#define EFI_IMAGE_FILE_LINE_NUMS_STRIPPED   (1 << 2)
#define EFI_IMAGE_FILE_LOCAL_SYMS_STRIPPED  (1 << 3)
#define EFI_IMAGE_FILE_BYTES_REVERSED_LO    (1 << 7)
#define EFI_IMAGE_FILE_32BIT_MACHINE        (1 << 8)
#define EFI_IMAGE_FILE_DEBUG_STRIPPED       (1 << 9)
#define EFI_IMAGE_FILE_SYSTEM               (1 << 12)
#define EFI_IMAGE_FILE_DLL                  (1 << 13)
#define EFI_IMAGE_FILE_BYTES_REVERSED_HI    (1 << 15)

typedef struct {
  UINT32  VirtualAddress;
  UINT32  Size;
} EFI_IMAGE_DATA_DIRECTORY;

#define EFI_IMAGE_DIRECTORY_ENTRY_EXPORT      0
#define EFI_IMAGE_DIRECTORY_ENTRY_IMPORT      1
#define EFI_IMAGE_DIRECTORY_ENTRY_RESOURCE    2
#define EFI_IMAGE_DIRECTORY_ENTRY_EXCEPTION   3
#define EFI_IMAGE_DIRECTORY_ENTRY_SECURITY    4
#define EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC   5
#define EFI_IMAGE_DIRECTORY_ENTRY_DEBUG       6
#define EFI_IMAGE_DIRECTORY_ENTRY_COPYRIGHT   7
#define EFI_IMAGE_DIRECTORY_ENTRY_GLOBALPTR   8
#define EFI_IMAGE_DIRECTORY_ENTRY_TLS         9
#define EFI_IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG 10

#define EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES 16

#define EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC 0x10b

typedef struct {

  UINT16                    Magic;
  UINT8                     MajorLinkerVersion;
  UINT8                     MinorLinkerVersion;
  UINT32                    SizeOfCode;
  UINT32                    SizeOfInitializedData;
  UINT32                    SizeOfUninitializedData;
  UINT32                    AddressOfEntryPoint;
  UINT32                    BaseOfCode;
  UINT32                    BaseOfData;

  UINT32                    ImageBase;
  UINT32                    SectionAlignment;
  UINT32                    FileAlignment;
  UINT16                    MajorOperatingSystemVersion;
  UINT16                    MinorOperatingSystemVersion;
  UINT16                    MajorImageVersion;
  UINT16                    MinorImageVersion;
  UINT16                    MajorSubsystemVersion;
  UINT16                    MinorSubsystemVersion;
  UINT32                    Win32VersionValue;
  UINT32                    SizeOfImage;
  UINT32                    SizeOfHeaders;
  UINT32                    CheckSum;
  UINT16                    Subsystem;
  UINT16                    DllCharacteristics;
  UINT32                    SizeOfStackReserve;
  UINT32                    SizeOfStackCommit;
  UINT32                    SizeOfHeapReserve;
  UINT32                    SizeOfHeapCommit;
  UINT32                    LoaderFlags;
  UINT32                    NumberOfRvaAndSizes;
  EFI_IMAGE_DATA_DIRECTORY  DataDirectory[EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES];
} EFI_IMAGE_OPTIONAL_HEADER32;

#define EFI_IMAGE_NT_OPTIONAL_HDR64_MAGIC 0x20b

typedef struct {

  UINT16                    Magic;
  UINT8                     MajorLinkerVersion;
  UINT8                     MinorLinkerVersion;
  UINT32                    SizeOfCode;
  UINT32                    SizeOfInitializedData;
  UINT32                    SizeOfUninitializedData;
  UINT32                    AddressOfEntryPoint;
  UINT32                    BaseOfCode;

  UINT64                    ImageBase;
  UINT32                    SectionAlignment;
  UINT32                    FileAlignment;
  UINT16                    MajorOperatingSystemVersion;
  UINT16                    MinorOperatingSystemVersion;
  UINT16                    MajorImageVersion;
  UINT16                    MinorImageVersion;
  UINT16                    MajorSubsystemVersion;
  UINT16                    MinorSubsystemVersion;
  UINT32                    Win32VersionValue;
  UINT32                    SizeOfImage;
  UINT32                    SizeOfHeaders;
  UINT32                    CheckSum;
  UINT16                    Subsystem;
  UINT16                    DllCharacteristics;
  UINT64                    SizeOfStackReserve;
  UINT64                    SizeOfStackCommit;
  UINT64                    SizeOfHeapReserve;
  UINT64                    SizeOfHeapCommit;
  UINT32                    LoaderFlags;
  UINT32                    NumberOfRvaAndSizes;
  EFI_IMAGE_DATA_DIRECTORY  DataDirectory[EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES];
} EFI_IMAGE_OPTIONAL_HEADER64;

typedef struct {
  UINT32                      Signature;
  EFI_IMAGE_FILE_HEADER       FileHeader;
  EFI_IMAGE_OPTIONAL_HEADER32 OptionalHeader;
} EFI_IMAGE_NT_HEADERS32;

#define EFI_IMAGE_SIZEOF_NT_OPTIONAL32_HEADER sizeof (EFI_IMAGE_NT_HEADERS32)

typedef struct {
  UINT32                      Signature;
  EFI_IMAGE_FILE_HEADER       FileHeader;
  EFI_IMAGE_OPTIONAL_HEADER64 OptionalHeader;
} EFI_IMAGE_NT_HEADERS64;

#define EFI_IMAGE_SIZEOF_NT_OPTIONAL64_HEADER sizeof (EFI_IMAGE_NT_HEADERS64)

#define EFI_IMAGE_SUBSYSTEM_UNKNOWN     0
#define EFI_IMAGE_SUBSYSTEM_NATIVE      1
#define EFI_IMAGE_SUBSYSTEM_WINDOWS_GUI 2
#define EFI_IMAGE_SUBSYSTEM_WINDOWS_CUI 3
#define EFI_IMAGE_SUBSYSTEM_OS2_CUI     5
#define EFI_IMAGE_SUBSYSTEM_POSIX_CUI   7

#define EFI_IMAGE_SIZEOF_SHORT_NAME 8

typedef struct {
  UINT8 Name[EFI_IMAGE_SIZEOF_SHORT_NAME];
  union {
    UINT32  PhysicalAddress;
    UINT32  VirtualSize;
  } Misc;
  UINT32  VirtualAddress;
  UINT32  SizeOfRawData;
  UINT32  PointerToRawData;
  UINT32  PointerToRelocations;
  UINT32  PointerToLinenumbers;
  UINT16  NumberOfRelocations;
  UINT16  NumberOfLinenumbers;
  UINT32  Characteristics;
} EFI_IMAGE_SECTION_HEADER;

#define EFI_IMAGE_SIZEOF_SECTION_HEADER       40

#define EFI_IMAGE_SCN_TYPE_NO_PAD                  BIT3
#define EFI_IMAGE_SCN_CNT_CODE                     BIT5
#define EFI_IMAGE_SCN_CNT_INITIALIZED_DATA         BIT6
#define EFI_IMAGE_SCN_CNT_UNINITIALIZED_DATA       BIT7

#define EFI_IMAGE_SCN_LNK_OTHER                    BIT8
#define EFI_IMAGE_SCN_LNK_INFO                     BIT9
#define EFI_IMAGE_SCN_LNK_REMOVE                   BIT11
#define EFI_IMAGE_SCN_LNK_COMDAT                   BIT12

#define EFI_IMAGE_SCN_ALIGN_1BYTES                 BIT20
#define EFI_IMAGE_SCN_ALIGN_2BYTES                 BIT21
#define EFI_IMAGE_SCN_ALIGN_4BYTES          (BIT20|BIT21)
#define EFI_IMAGE_SCN_ALIGN_8BYTES                 BIT22
#define EFI_IMAGE_SCN_ALIGN_16BYTES         (BIT20|BIT22)
#define EFI_IMAGE_SCN_ALIGN_32BYTES         (BIT21|BIT22)
#define EFI_IMAGE_SCN_ALIGN_64BYTES   (BIT20|BIT21|BIT22)

#define EFI_IMAGE_SCN_MEM_DISCARDABLE              BIT25
#define EFI_IMAGE_SCN_MEM_NOT_CACHED               BIT26
#define EFI_IMAGE_SCN_MEM_NOT_PAGED                BIT27
#define EFI_IMAGE_SCN_MEM_SHARED                   BIT28
#define EFI_IMAGE_SCN_MEM_EXECUTE                  BIT29
#define EFI_IMAGE_SCN_MEM_READ                     BIT30
#define EFI_IMAGE_SCN_MEM_WRITE                    BIT31

#define EFI_IMAGE_SIZEOF_SYMBOL 18

#define EFI_IMAGE_SYM_UNDEFINED (UINT16) 0
#define EFI_IMAGE_SYM_ABSOLUTE  (UINT16) -1
#define EFI_IMAGE_SYM_DEBUG     (UINT16) -2

#define EFI_IMAGE_SYM_TYPE_NULL   0
#define EFI_IMAGE_SYM_TYPE_VOID   1
#define EFI_IMAGE_SYM_TYPE_CHAR   2
#define EFI_IMAGE_SYM_TYPE_SHORT  3
#define EFI_IMAGE_SYM_TYPE_INT    4
#define EFI_IMAGE_SYM_TYPE_LONG   5
#define EFI_IMAGE_SYM_TYPE_FLOAT  6
#define EFI_IMAGE_SYM_TYPE_DOUBLE 7
#define EFI_IMAGE_SYM_TYPE_STRUCT 8
#define EFI_IMAGE_SYM_TYPE_UNION  9
#define EFI_IMAGE_SYM_TYPE_ENUM   10
#define EFI_IMAGE_SYM_TYPE_MOE    11
#define EFI_IMAGE_SYM_TYPE_BYTE   12
#define EFI_IMAGE_SYM_TYPE_WORD   13
#define EFI_IMAGE_SYM_TYPE_UINT   14
#define EFI_IMAGE_SYM_TYPE_DWORD  15

#define EFI_IMAGE_SYM_DTYPE_NULL      0
#define EFI_IMAGE_SYM_DTYPE_POINTER   1
#define EFI_IMAGE_SYM_DTYPE_FUNCTION  2
#define EFI_IMAGE_SYM_DTYPE_ARRAY     3

#define EFI_IMAGE_SYM_CLASS_END_OF_FUNCTION   ((UINT8) -1)
#define EFI_IMAGE_SYM_CLASS_NULL              0
#define EFI_IMAGE_SYM_CLASS_AUTOMATIC         1
#define EFI_IMAGE_SYM_CLASS_EXTERNAL          2
#define EFI_IMAGE_SYM_CLASS_STATIC            3
#define EFI_IMAGE_SYM_CLASS_REGISTER          4
#define EFI_IMAGE_SYM_CLASS_EXTERNAL_DEF      5
#define EFI_IMAGE_SYM_CLASS_LABEL             6
#define EFI_IMAGE_SYM_CLASS_UNDEFINED_LABEL   7
#define EFI_IMAGE_SYM_CLASS_MEMBER_OF_STRUCT  8
#define EFI_IMAGE_SYM_CLASS_ARGUMENT          9
#define EFI_IMAGE_SYM_CLASS_STRUCT_TAG        10
#define EFI_IMAGE_SYM_CLASS_MEMBER_OF_UNION   11
#define EFI_IMAGE_SYM_CLASS_UNION_TAG         12
#define EFI_IMAGE_SYM_CLASS_TYPE_DEFINITION   13
#define EFI_IMAGE_SYM_CLASS_UNDEFINED_STATIC  14
#define EFI_IMAGE_SYM_CLASS_ENUM_TAG          15
#define EFI_IMAGE_SYM_CLASS_MEMBER_OF_ENUM    16
#define EFI_IMAGE_SYM_CLASS_REGISTER_PARAM    17
#define EFI_IMAGE_SYM_CLASS_BIT_FIELD         18
#define EFI_IMAGE_SYM_CLASS_BLOCK             100
#define EFI_IMAGE_SYM_CLASS_FUNCTION          101
#define EFI_IMAGE_SYM_CLASS_END_OF_STRUCT     102
#define EFI_IMAGE_SYM_CLASS_FILE              103
#define EFI_IMAGE_SYM_CLASS_SECTION           104
#define EFI_IMAGE_SYM_CLASS_WEAK_EXTERNAL     105

#define EFI_IMAGE_N_BTMASK  017
#define EFI_IMAGE_N_TMASK   060
#define EFI_IMAGE_N_TMASK1  0300
#define EFI_IMAGE_N_TMASK2  0360
#define EFI_IMAGE_N_BTSHFT  4
#define EFI_IMAGE_N_TSHIFT  2

#define EFI_IMAGE_COMDAT_SELECT_NODUPLICATES    1
#define EFI_IMAGE_COMDAT_SELECT_ANY             2
#define EFI_IMAGE_COMDAT_SELECT_SAME_SIZE       3
#define EFI_IMAGE_COMDAT_SELECT_EXACT_MATCH     4
#define EFI_IMAGE_COMDAT_SELECT_ASSOCIATIVE     5

#define EFI_IMAGE_WEAK_EXTERN_SEARCH_NOLIBRARY  1
#define EFI_IMAGE_WEAK_EXTERN_SEARCH_LIBRARY    2
#define EFI_IMAGE_WEAK_EXTERN_SEARCH_ALIAS      3

typedef struct {
  UINT32  VirtualAddress;
  UINT32  SymbolTableIndex;
  UINT16  Type;
} EFI_IMAGE_RELOCATION;

#define EFI_IMAGE_SIZEOF_RELOCATION 10

#define EFI_IMAGE_REL_I386_ABSOLUTE 0x0000
#define EFI_IMAGE_REL_I386_DIR16    0x0001
#define EFI_IMAGE_REL_I386_REL16    0x0002
#define EFI_IMAGE_REL_I386_DIR32    0x0006
#define EFI_IMAGE_REL_I386_DIR32NB  0x0007
#define EFI_IMAGE_REL_I386_SEG12    0x0009
#define EFI_IMAGE_REL_I386_SECTION  0x000A
#define EFI_IMAGE_REL_I386_SECREL   0x000B
#define EFI_IMAGE_REL_I386_REL32    0x0014

#define IMAGE_REL_AMD64_ABSOLUTE  0x0000
#define IMAGE_REL_AMD64_ADDR64    0x0001
#define IMAGE_REL_AMD64_ADDR32    0x0002
#define IMAGE_REL_AMD64_ADDR32NB  0x0003
#define IMAGE_REL_AMD64_REL32     0x0004
#define IMAGE_REL_AMD64_REL32_1   0x0005
#define IMAGE_REL_AMD64_REL32_2   0x0006
#define IMAGE_REL_AMD64_REL32_3   0x0007
#define IMAGE_REL_AMD64_REL32_4   0x0008
#define IMAGE_REL_AMD64_REL32_5   0x0009
#define IMAGE_REL_AMD64_SECTION   0x000A
#define IMAGE_REL_AMD64_SECREL    0x000B
#define IMAGE_REL_AMD64_SECREL7   0x000C
#define IMAGE_REL_AMD64_TOKEN     0x000D
#define IMAGE_REL_AMD64_SREL32    0x000E
#define IMAGE_REL_AMD64_PAIR      0x000F
#define IMAGE_REL_AMD64_SSPAN32   0x0010

typedef struct {
  UINT32  VirtualAddress;
  UINT32  SizeOfBlock;
} EFI_IMAGE_BASE_RELOCATION;

#define EFI_IMAGE_SIZEOF_BASE_RELOCATION  8

#define EFI_IMAGE_REL_BASED_ABSOLUTE        0
#define EFI_IMAGE_REL_BASED_HIGH            1
#define EFI_IMAGE_REL_BASED_LOW             2
#define EFI_IMAGE_REL_BASED_HIGHLOW         3
#define EFI_IMAGE_REL_BASED_HIGHADJ         4
#define EFI_IMAGE_REL_BASED_MIPS_JMPADDR    5
#define EFI_IMAGE_REL_BASED_ARM_MOV32A      5
#define EFI_IMAGE_REL_BASED_ARM_MOV32T      7
#define EFI_IMAGE_REL_BASED_IA64_IMM64      9
#define EFI_IMAGE_REL_BASED_MIPS_JMPADDR16  9
#define EFI_IMAGE_REL_BASED_DIR64           10

typedef struct {
  union {
    UINT32  SymbolTableIndex;
    UINT32  VirtualAddress;
  } Type;
  UINT16  Linenumber;
} EFI_IMAGE_LINENUMBER;

#define EFI_IMAGE_SIZEOF_LINENUMBER 6

#define EFI_IMAGE_ARCHIVE_START_SIZE        8
#define EFI_IMAGE_ARCHIVE_START             "!<arch>\n"
#define EFI_IMAGE_ARCHIVE_END               "`\n"
#define EFI_IMAGE_ARCHIVE_PAD               "\n"
#define EFI_IMAGE_ARCHIVE_LINKER_MEMBER     "/               "
#define EFI_IMAGE_ARCHIVE_LONGNAMES_MEMBER  "//              "

typedef struct {
  UINT8 Name[16];
  UINT8 Date[12];
  UINT8 UserID[6];
  UINT8 GroupID[6];
  UINT8 Mode[8];
  UINT8 Size[10];
  UINT8 EndHeader[2];
} EFI_IMAGE_ARCHIVE_MEMBER_HEADER;

#define EFI_IMAGE_SIZEOF_ARCHIVE_MEMBER_HDR 60

typedef struct {
  UINT32  Characteristics;
  UINT32  TimeDateStamp;
  UINT16  MajorVersion;
  UINT16  MinorVersion;
  UINT32  Name;
  UINT32  Base;
  UINT32  NumberOfFunctions;
  UINT32  NumberOfNames;
  UINT32  AddressOfFunctions;
  UINT32  AddressOfNames;
  UINT32  AddressOfNameOrdinals;
} EFI_IMAGE_EXPORT_DIRECTORY;

typedef struct {
  UINT16  Hint;
  UINT8   Name[1];
} EFI_IMAGE_IMPORT_BY_NAME;

typedef struct {
  union {
    UINT32                    Function;
    UINT32                    Ordinal;
    EFI_IMAGE_IMPORT_BY_NAME  *AddressOfData;
  } u1;
} EFI_IMAGE_THUNK_DATA;

#define EFI_IMAGE_ORDINAL_FLAG              BIT31
#define EFI_IMAGE_SNAP_BY_ORDINAL(Ordinal)  ((Ordinal & EFI_IMAGE_ORDINAL_FLAG) != 0)
#define EFI_IMAGE_ORDINAL(Ordinal)          (Ordinal & 0xffff)

typedef struct {
  UINT32                Characteristics;
  UINT32                TimeDateStamp;
  UINT32                ForwarderChain;
  UINT32                Name;
  EFI_IMAGE_THUNK_DATA  *FirstThunk;
} EFI_IMAGE_IMPORT_DESCRIPTOR;

typedef struct {
  UINT32  Characteristics;
  UINT32  TimeDateStamp;
  UINT16  MajorVersion;
  UINT16  MinorVersion;
  UINT32  Type;
  UINT32  SizeOfData;
  UINT32  RVA;
  UINT32  FileOffset;
} EFI_IMAGE_DEBUG_DIRECTORY_ENTRY;

#define EFI_IMAGE_DEBUG_TYPE_CODEVIEW 2

#define CODEVIEW_SIGNATURE_NB10  SIGNATURE_32('N', 'B', '1', '0')
typedef struct {
  UINT32  Signature;
  UINT32  Unknown;
  UINT32  Unknown2;
  UINT32  Unknown3;

} EFI_IMAGE_DEBUG_CODEVIEW_NB10_ENTRY;

#define CODEVIEW_SIGNATURE_RSDS  SIGNATURE_32('R', 'S', 'D', 'S')
typedef struct {
  UINT32  Signature;
  UINT32  Unknown;
  UINT32  Unknown2;
  UINT32  Unknown3;
  UINT32  Unknown4;
  UINT32  Unknown5;

} EFI_IMAGE_DEBUG_CODEVIEW_RSDS_ENTRY;

#define CODEVIEW_SIGNATURE_MTOC  SIGNATURE_32('M', 'T', 'O', 'C')
typedef struct {
  UINT32    Signature;
  EFI_GUID      MachOUuid;

} EFI_IMAGE_DEBUG_CODEVIEW_MTOC_ENTRY;

typedef struct {
  UINT32  Characteristics;
  UINT32  TimeDateStamp;
  UINT16  MajorVersion;
  UINT16  MinorVersion;
  UINT16  NumberOfNamedEntries;
  UINT16  NumberOfIdEntries;

} EFI_IMAGE_RESOURCE_DIRECTORY;

typedef struct {
  union {
    struct {
      UINT32  NameOffset:31;
      UINT32  NameIsString:1;
    } s;
    UINT32  Id;
  } u1;
  union {
    UINT32  OffsetToData;
    struct {
      UINT32  OffsetToDirectory:31;
      UINT32  DataIsDirectory:1;
    } s;
  } u2;
} EFI_IMAGE_RESOURCE_DIRECTORY_ENTRY;

typedef struct {
  UINT16  Length;
  CHAR16  String[1];
} EFI_IMAGE_RESOURCE_DIRECTORY_STRING;

typedef struct {
  UINT32  OffsetToData;
  UINT32  Size;
  UINT32  CodePage;
  UINT32  Reserved;
} EFI_IMAGE_RESOURCE_DATA_ENTRY;

typedef struct {
  UINT16                    Signature;
  UINT16                    Machine;
  UINT8                     NumberOfSections;
  UINT8                     Subsystem;
  UINT16                    StrippedSize;
  UINT32                    AddressOfEntryPoint;
  UINT32                    BaseOfCode;
  UINT64                    ImageBase;
  EFI_IMAGE_DATA_DIRECTORY  DataDirectory[2];
} EFI_TE_IMAGE_HEADER;

#define EFI_TE_IMAGE_HEADER_SIGNATURE  SIGNATURE_16('V', 'Z')

#define EFI_TE_IMAGE_DIRECTORY_ENTRY_BASERELOC  0
#define EFI_TE_IMAGE_DIRECTORY_ENTRY_DEBUG      1

typedef union {
  EFI_IMAGE_NT_HEADERS32   Pe32;
  EFI_IMAGE_NT_HEADERS64   Pe32Plus;
  EFI_TE_IMAGE_HEADER      Te;
} EFI_IMAGE_OPTIONAL_HEADER_UNION;

typedef union {
  EFI_IMAGE_NT_HEADERS32            *Pe32;
  EFI_IMAGE_NT_HEADERS64            *Pe32Plus;
  EFI_TE_IMAGE_HEADER               *Te;
  EFI_IMAGE_OPTIONAL_HEADER_UNION   *Union;
} EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION;

typedef struct _WIN_CERTIFICATE {
	UINT32  dwLength;
	UINT16  wRevision;
	UINT16  wCertificateType;
} WIN_CERTIFICATE;

typedef struct {
	WIN_CERTIFICATE Hdr;
	UINT8           CertData[1];
} WIN_CERTIFICATE_EFI_PKCS;

#define SHA256_DIGEST_SIZE  32
#define WIN_CERT_TYPE_PKCS_SIGNED_DATA 0x0002

#endif
