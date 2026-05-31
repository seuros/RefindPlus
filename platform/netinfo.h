// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>

#ifndef __NETINFO_H_
#define __NETINFO_H_

#include "tiano_includes.h"

typedef enum
{
    NET_IP_NONE = 0,
    NET_IP_STATIC,
    NET_IP_DHCP,
} MERIDIAN_IP_SOURCE;

typedef struct
{
    BOOLEAN Present;
    BOOLEAN LinkKnown;
    BOOLEAN LinkUp;
    BOOLEAN HasMac;
    UINT8 MacLen;
    UINT8 Mac[6];
    BOOLEAN HasIp;
    UINT8 Ip[4];
    UINT8 Mask[4];
    MERIDIAN_IP_SOURCE Source;
    CHAR8 Name[32];
} MERIDIAN_NIC_INFO;

extern UINTN MeridianNicCount;
extern MERIDIAN_NIC_INFO *MeridianNics;
extern BOOLEAN MeridianNetProbed;
extern BOOLEAN MeridianNetDhcpTried;

typedef BOOLEAN (*MERIDIAN_NET_PUMP)(VOID *Ctx);

VOID ScanNetwork(BOOLEAN Connect, BOOLEAN AttemptDhcp, MERIDIAN_NET_PUMP Pump, VOID *Ctx);

#endif
