// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2026 Dayo Akanji
// SPDX-FileCopyrightText: 2006 Christoph Pfisterer

#ifndef _FSW_ISO9660_H_
#define _FSW_ISO9660_H_

#define VOLSTRUCTNAME fsw_iso9660_volume
#define DNODESTRUCTNAME fsw_iso9660_dnode
#include "fsw_core.h"

#define ISO9660_BLOCKSIZE          2048
#define ISO9660_BLOCKSIZE_BITS       11

#define ISO9660_SUPERBLOCK_BLOCKNO   16

#pragma pack(push, 1)

typedef struct {
    fsw_u16     lsb;
    fsw_u16     msb;
} iso9660_u16;

typedef struct {
    fsw_u32     lsb;
    fsw_u32     msb;
} iso9660_u32;

#define ISOINT(lsbmsbvalue) ((lsbmsbvalue).lsb)

struct iso9660_dirrec {
    fsw_u8      dirrec_length;
    fsw_u8      ear_length;
    iso9660_u32 extent_location;
    iso9660_u32 data_length;
    fsw_u8      recording_datetime[7];
    fsw_u8      file_flags;
    fsw_u8      file_unit_size;
    fsw_u8      interleave_gap_size;
    iso9660_u16 volume_sequence_number;
    fsw_u8      file_identifier_length;
    char        file_identifier[1];
};

struct iso9660_volume_descriptor {
    fsw_u8      volume_descriptor_type;
    char        standard_identifier[5];
    fsw_u8      volume_descriptor_version;
};

struct iso9660_primary_volume_descriptor {
    fsw_u8      volume_descriptor_type;
    char        standard_identifier[5];
    fsw_u8      volume_descriptor_version;
    fsw_u8      unused1;
    char        system_identifier[32];
    char        volume_identifier[32];
    fsw_u8      unused2[8];
    iso9660_u32 volume_space_size;
    fsw_u8      unused3[4];
    fsw_u8      escape[3];
    fsw_u8      unused4[25];
    iso9660_u16 volume_set_size;
    iso9660_u16 volume_sequence_number;
    iso9660_u16 logical_block_size;
    iso9660_u32 path_table_size;
    fsw_u32     location_type_l_path_table;
    fsw_u32     location_optional_type_l_path_table;
    fsw_u32     location_type_m_path_table;
    fsw_u32     location_optional_type_m_path_table;
    struct iso9660_dirrec root_directory;
    char        volume_set_identifier[128];
    char        publisher_identifier[128];
    char        data_preparer_identifier[128];
    char        application_identifier[128];
    char        copyright_file_identifier[37];
    char        abstract_file_identifier[37];
    char        bibliographic_file_identifier[37];
    char        volume_creation_datetime[17];
    char        volume_modification_datetime[17];
    char        volume_expiration_datetime[17];
    char        volume_effective_datetime[17];
    fsw_u8      file_structure_version;
    fsw_u8      reserved1;
    fsw_u8      application_use[512];
    fsw_u8      reserved2[653];
};

#pragma pack(pop)

struct iso9660_dirrec_buffer {
    fsw_u32     ino;
    struct fsw_string name;
    struct iso9660_dirrec dirrec;
    char        dirrec_buffer[222];
};

struct fsw_iso9660_volume {
    struct fsw_volume g;

    int fJoliet;

    int fRockRidge;

    int rr_susp_skip;

    struct iso9660_primary_volume_descriptor *primary_voldesc;
};

struct fsw_iso9660_dnode {
    struct fsw_dnode g;

    struct iso9660_dirrec dirrec;
};

struct fsw_rock_ridge_susp_entry
{
    fsw_u8  sig[2];
    fsw_u8  len;
    fsw_u8  ver;
};

struct fsw_rock_ridge_susp_sp
{
    struct fsw_rock_ridge_susp_entry e;
    fsw_u8  magic[2];
    fsw_u8  skip;
};

struct fsw_rock_ridge_susp_nm
{
    struct fsw_rock_ridge_susp_entry e;
    fsw_u8  flags;
    fsw_u8  name[1];
};

#define RR_NM_CONT (1<<0)
#define RR_NM_CURR (1<<1)
#define RR_NM_PARE (1<<2)

union fsw_rock_ridge_susp_ce
{
    struct X{
        struct fsw_rock_ridge_susp_entry e;
        iso9660_u32 block_loc;
        iso9660_u32 offset;
        iso9660_u32 len;
    } X;
    fsw_u8 raw[28];
};

#endif
