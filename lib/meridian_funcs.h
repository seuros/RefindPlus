// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2021-2024 Dayo Akanji

#ifndef _MERIDIAN_FUNCS_H
#define _MERIDIAN_FUNCS_H

extern BOOLEAN gKernelStarted;

extern VOID MeridianStall(UINTN StallLoops);

#define MRD_NATIVELOGGER_SET                                                                       \
    do {                                                                                           \
        if (!gKernelStarted) {                                                                     \
            ForceNative = FALSE;                                                                   \
            if (!MuteLogger) {                                                                     \
                ForceNative = TRUE;                                                                \
                NativeLogger = TRUE;                                                               \
            }                                                                                      \
        }                                                                                          \
    } while (0)
#define MRD_NATIVELOGGER_OFF                                                                       \
    do {                                                                                           \
        if (!gKernelStarted) {                                                                     \
            if (ForceNative) {                                                                     \
                NativeLogger = FALSE;                                                              \
            }                                                                                      \
        }                                                                                          \
    } while (0)

#define MRD_MUTELOGGER_SET                                                                         \
    do {                                                                                           \
        if (!gKernelStarted) {                                                                     \
            CheckMute = FALSE;                                                                     \
            if (!MuteLogger) {                                                                     \
                CheckMute = TRUE;                                                                  \
                MuteLogger = TRUE;                                                                 \
            }                                                                                      \
        }                                                                                          \
    } while (0)
#define MRD_MUTELOGGER_OFF                                                                         \
    do {                                                                                           \
        if (!gKernelStarted) {                                                                     \
            if (CheckMute) {                                                                       \
                MuteLogger = FALSE;                                                                \
            }                                                                                      \
        }                                                                                          \
    } while (0)

#define MRD_HYBRIDLOGGER_SET                                                                       \
    do {                                                                                           \
        if (!gKernelStarted) {                                                                     \
            HybridLogger = FALSE;                                                                  \
            if (NativeLogger) {                                                                    \
                HybridLogger = TRUE;                                                               \
                NativeLogger = FALSE;                                                              \
            }                                                                                      \
        }                                                                                          \
    } while (0)
#define MRD_HYBRIDLOGGER_OFF                                                                       \
    do {                                                                                           \
        if (!gKernelStarted) {                                                                     \
            if (HybridLogger) {                                                                    \
                NativeLogger = TRUE;                                                               \
                HybridLogger = FALSE;                                                              \
            }                                                                                      \
        }                                                                                          \
    } while (0)

#define MRD_FAKE_FREE(Pointer)                                                                     \
    do {                                                                                           \
        if (!gKernelStarted) {                                                                     \
            if (Pointer != NULL) {                                                                 \
                Pointer = NULL;                                                                    \
            }                                                                                      \
        }                                                                                          \
    } while (0)

#define MRD_SOFT_FREE(Pointer)                                                                     \
    do {                                                                                           \
        if (!gKernelStarted) {                                                                     \
            if (Pointer != NULL) {                                                                 \
                Pointer = NULL;                                                                    \
            }                                                                                      \
        }                                                                                          \
    } while (0)

#define MRD_FREE_POOL(Pointer)                                                                     \
    do {                                                                                           \
        if (!gKernelStarted) {                                                                     \
            if (Pointer != NULL) {                                                                 \
                FreePool(Pointer);                                                                 \
                Pointer = NULL;                                                                    \
            }                                                                                      \
        }                                                                                          \
    } while (0)

#define MRD_FREE_FILE(File)                                                                        \
    do {                                                                                           \
        if (!gKernelStarted) {                                                                     \
            if (File != NULL) {                                                                    \
                if (File->BufferData != NULL) {                                                    \
                    FreePool(File->BufferData);                                                    \
                    File->BufferData = NULL;                                                       \
                }                                                                                  \
                FreePool(File);                                                                    \
                File = NULL;                                                                       \
            }                                                                                      \
        }                                                                                          \
    } while (0)

#endif
