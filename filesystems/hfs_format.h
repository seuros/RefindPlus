// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
// SPDX-FileCopyrightText: 2026 Dayo Akanji

#ifndef __HFS_FORMAT__
#define __HFS_FORMAT__

#ifdef _MSC_VER
# pragma pack(push,2)
# define HFS_ALIGNMENT
#else
#define HFS_ALIGNMENT __attribute__((aligned(2), packed))
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum {
	kHFSSigWord          = 0x4244,
	kHFSPlusSigWord      = 0x482B,
	kHFSXSigWord         = 0x4858,

	kHFSPlusVersion      = 0x0004,
	kHFSXVersion         = 0x0005,

	kHFSPlusMountVersion = 0x31302E30,
	kHFSJMountVersion    = 0x4846534a,
	kFSKMountVersion     = 0x46534b21
};

#ifdef __APPLE_API_PRIVATE

#define HFSPLUSMETADATAFOLDER       "\xE2\x90\x80\xE2\x90\x80\xE2\x90\x80\xE2\x90\x80HFS+ Private Data"
#define HFSPLUS_DIR_METADATA_FOLDER ".HFS+ Private Directory Data\xd"

#define HFS_INODE_PREFIX  "iNode"
#define HFS_DELETE_PREFIX "temp"

#define HFS_DIRINODE_PREFIX "dir_"

#define FIRST_LINK_XATTR_NAME      "com.apple.system.hfs.firstlink"
#define FIRST_LINK_XATTR_REC_SIZE (sizeof (HFSPlusAttrData) - 2 + 12)

#endif

enum {
	kHardLinkFileType = 0x686C6E6B,
	kHFSPlusCreator   = 0x6866732B
};

enum {
      kSymLinkFileType = 0x736C6E6B,
      kSymLinkCreator  = 0x72686170
};

#ifndef _HFSUNISTR255_DEFINED_
#define _HFSUNISTR255_DEFINED_

struct HFSUniStr255 {
	u_int16_t length;
	u_int16_t unicode[255];
} HFS_ALIGNMENT;
typedef struct HFSUniStr255 HFSUniStr255;
typedef const HFSUniStr255 *ConstHFSUniStr255Param;
#endif

enum {
	kHFSMaxVolumeNameChars   = 27,
	kHFSMaxFileNameChars     = 31,
	kHFSPlusMaxFileNameChars = 255
};

struct HFSExtentKey {
	u_int8_t  keyLength;
	u_int8_t  forkType;
	u_int32_t fileID;
	u_int16_t startBlock;
} HFS_ALIGNMENT;
typedef struct HFSExtentKey HFSExtentKey;

struct HFSPlusExtentKey {
	u_int16_t keyLength;
	u_int8_t  forkType;
	u_int8_t  pad;
	u_int32_t fileID;
	u_int32_t startBlock;
} HFS_ALIGNMENT;
typedef struct HFSPlusExtentKey HFSPlusExtentKey;

enum {
	kHFSExtentDensity     = 3,
	kHFSPlusExtentDensity = 8
};

struct HFSExtentDescriptor {
	u_int16_t startBlock;
	u_int16_t blockCount;
} HFS_ALIGNMENT;
typedef struct HFSExtentDescriptor HFSExtentDescriptor;

struct HFSPlusExtentDescriptor {
	u_int32_t startBlock;
	u_int32_t blockCount;
} HFS_ALIGNMENT;
typedef struct HFSPlusExtentDescriptor HFSPlusExtentDescriptor;

typedef HFSExtentDescriptor HFSExtentRecord[3];

typedef HFSPlusExtentDescriptor HFSPlusExtentRecord[8];

struct FndrFileInfo {
	u_int32_t fdType;
	u_int32_t fdCreator;
	u_int16_t fdFlags;
	struct {
	    int16_t v;
	    int16_t h;
	} fdLocation;
	int16_t	opaque;
} HFS_ALIGNMENT;
typedef struct FndrFileInfo FndrFileInfo;

struct FndrDirInfo {
	struct {
	    int16_t top;
	    int16_t	left;
	    int16_t	bottom;
	    int16_t	right;
	} frRect;
	unsigned short	frFlags;
	struct {
	    u_int16_t v;
	    u_int16_t h;
	} frLocation;
	int16_t	opaque;
} HFS_ALIGNMENT;
typedef struct FndrDirInfo FndrDirInfo;

struct FndrOpaqueInfo {
	int8_t opaque[16];
} HFS_ALIGNMENT;
typedef struct FndrOpaqueInfo FndrOpaqueInfo;

struct HFSPlusForkData {
	u_int64_t           logicalSize;
	u_int32_t           clumpSize;
	u_int32_t           totalBlocks;
	HFSPlusExtentRecord	extents;
} HFS_ALIGNMENT;
typedef struct HFSPlusForkData HFSPlusForkData;

struct HFSPlusBSDInfo {
	u_int32_t     ownerID;
	u_int32_t     groupID;
	u_int8_t      adminFlags;
	u_int8_t      ownerFlags;
	u_int16_t     fileMode;
	union {
	    u_int32_t iNodeNum;
	    u_int32_t linkCount;
	    u_int32_t rawDevice;
	} special;
} HFS_ALIGNMENT;
typedef struct HFSPlusBSDInfo HFSPlusBSDInfo;

#define hl_firstLinkID     reserved1

#define hl_prevLinkID      bsdInfo.ownerID
#define hl_nextLinkID      bsdInfo.groupID

#define hl_linkReference   bsdInfo.special.iNodeNum
#define hl_linkCount       bsdInfo.special.linkCount

enum {
	kHFSRootParentID           =  1,
	kHFSRootFolderID           =  2,
	kHFSExtentsFileID          =  3,
	kHFSCatalogFileID          =  4,
	kHFSBadBlockFileID         =  5,
	kHFSAllocationFileID       =  6,
	kHFSStartupFileID          =  7,
	kHFSAttributesFileID       =  8,
	kHFSAttributeDataFileID    = 13,

	kHFSRepairCatalogFileID    = 14,
	kHFSBogusExtentFileID      = 15,
	kHFSFirstUserCatalogNodeID = 16
};

struct HFSCatalogKey {
	u_int8_t  keyLength;
	u_int8_t  reserved;
	u_int32_t parentID;
	u_int8_t  nodeName[kHFSMaxFileNameChars + 1];
} HFS_ALIGNMENT;
typedef struct HFSCatalogKey HFSCatalogKey;

struct HFSPlusCatalogKey {
	u_int16_t    keyLength;
	u_int32_t    parentID;
	HFSUniStr255 nodeName;
} HFS_ALIGNMENT;
typedef struct HFSPlusCatalogKey HFSPlusCatalogKey;

enum {

	kHFSFolderRecord           = 0x0100,
	kHFSFileRecord             = 0x0200,
	kHFSFolderThreadRecord     = 0x0300,
	kHFSFileThreadRecord       = 0x0400,

	kHFSPlusFolderRecord       = 1,
	kHFSPlusFileRecord         = 2,
	kHFSPlusFolderThreadRecord = 3,
	kHFSPlusFileThreadRecord   = 4
};

enum {
	kHFSFileLockedBit      = 0x0000,
	kHFSFileLockedMask     = 0x0001,

	kHFSThreadExistsBit    = 0x0001,
	kHFSThreadExistsMask   = 0x0002,

	kHFSHasAttributesBit   = 0x0002,
	kHFSHasAttributesMask  = 0x0004,

	kHFSHasSecurityBit     = 0x0003,
	kHFSHasSecurityMask    = 0x0008,

	kHFSHasFolderCountBit  = 0x0004,
	kHFSHasFolderCountMask = 0x0010,

	kHFSHasLinkChainBit   = 0x0005,
	kHFSHasLinkChainMask  = 0x0020,

	kHFSHasChildLinkBit   = 0x0006,
	kHFSHasChildLinkMask  = 0x0040
};

struct HFSCatalogFolder {
	int16_t         recordType;
	u_int16_t       flags;
	u_int16_t       valence;
	u_int32_t       folderID;
	u_int32_t       createDate;
	u_int32_t       modifyDate;
	u_int32_t       backupDate;
	FndrDirInfo		userInfo;
	FndrOpaqueInfo  finderInfo;
	u_int32_t       reserved[4];
} HFS_ALIGNMENT;
typedef struct HFSCatalogFolder HFSCatalogFolder;

struct HFSPlusCatalogFolder {
	int16_t         recordType;
	u_int16_t       flags;
	u_int32_t       valence;
	u_int32_t       folderID;
	u_int32_t       createDate;
	u_int32_t       contentModDate;
	u_int32_t       attributeModDate;
	u_int32_t       accessDate;
	u_int32_t       backupDate;
	HFSPlusBSDInfo  bsdInfo;
	FndrDirInfo     userInfo;
	FndrOpaqueInfo  finderInfo;
	u_int32_t       textEncoding;
	u_int32_t       folderCount;
} HFS_ALIGNMENT;
typedef struct HFSPlusCatalogFolder HFSPlusCatalogFolder;

struct HFSCatalogFile {
	int16_t         recordType;
	u_int8_t        flags;
	int8_t          fileType;
	FndrFileInfo    userInfo;
	u_int32_t       fileID;
	u_int16_t       dataStartBlock;
	int32_t         dataLogicalSize;
	int32_t         dataPhysicalSize;
	u_int16_t       rsrcStartBlock;
	int32_t         rsrcLogicalSize;
	int32_t         rsrcPhysicalSize;
	u_int32_t       createDate;
	u_int32_t       modifyDate;
	u_int32_t       backupDate;
	FndrOpaqueInfo  finderInfo;
	u_int16_t       clumpSize;
	HFSExtentRecord dataExtents;
	HFSExtentRecord rsrcExtents;
	u_int32_t       reserved;
} HFS_ALIGNMENT;
typedef struct HFSCatalogFile HFSCatalogFile;

struct HFSPlusCatalogFile {
	int16_t         recordType;
	u_int16_t       flags;
	u_int32_t       reserved1;
	u_int32_t       fileID;
	u_int32_t       createDate;
	u_int32_t       contentModDate;
	u_int32_t       attributeModDate;
	u_int32_t       accessDate;
	u_int32_t       backupDate;
	HFSPlusBSDInfo  bsdInfo;
	FndrFileInfo    userInfo;
	FndrOpaqueInfo  finderInfo;
	u_int32_t       textEncoding;
	u_int32_t       reserved2;

	HFSPlusForkData dataFork;
	HFSPlusForkData resourceFork;
} HFS_ALIGNMENT;
typedef struct HFSPlusCatalogFile HFSPlusCatalogFile;

struct HFSCatalogThread {
	int16_t   recordType;
	int32_t   reserved[2];
	u_int32_t parentID;
	u_int8_t  nodeName[kHFSMaxFileNameChars + 1];
} HFS_ALIGNMENT;
typedef struct HFSCatalogThread HFSCatalogThread;

struct HFSPlusCatalogThread {
	int16_t      recordType;
	int16_t      reserved;
	u_int32_t    parentID;
	HFSUniStr255 nodeName;
} HFS_ALIGNMENT;
typedef struct HFSPlusCatalogThread HFSPlusCatalogThread;

#ifdef __APPLE_API_UNSTABLE

enum {
	kHFSPlusAttrInlineData = 0x10,
	kHFSPlusAttrForkData   = 0x20,
	kHFSPlusAttrExtents    = 0x30
};

struct HFSPlusAttrForkData {
	u_int32_t       recordType;
	u_int32_t       reserved;
	HFSPlusForkData theFork;
} HFS_ALIGNMENT;
typedef struct HFSPlusAttrForkData HFSPlusAttrForkData;

struct HFSPlusAttrExtents {
	u_int32_t           recordType;
	u_int32_t           reserved;
	HFSPlusExtentRecord extents;
} HFS_ALIGNMENT;
typedef struct HFSPlusAttrExtents HFSPlusAttrExtents;

struct HFSPlusAttrData {
	u_int32_t recordType;
	u_int32_t reserved[2];
	u_int32_t attrSize;
	u_int8_t  attrData[2];
} HFS_ALIGNMENT;
typedef struct HFSPlusAttrData HFSPlusAttrData;

struct HFSPlusAttrInlineData {
	u_int32_t recordType;
	u_int32_t reserved;
	u_int32_t logicalSize;
	u_int8_t  userData[2];
} HFS_ALIGNMENT;
typedef struct HFSPlusAttrInlineData HFSPlusAttrInlineData;

union HFSPlusAttrRecord {
	u_int32_t             recordType;
	HFSPlusAttrInlineData inlineData;
	HFSPlusAttrData       attrData;
	HFSPlusAttrForkData   forkData;
	HFSPlusAttrExtents    overflowExtents;
};
typedef union HFSPlusAttrRecord HFSPlusAttrRecord;

enum { kHFSMaxAttrNameLen = 127 };
struct HFSPlusAttrKey {
	u_int16_t keyLength;
	u_int16_t pad;
	u_int32_t fileID;
	u_int32_t startBlock;
	u_int16_t attrNameLen;
	u_int16_t attrName[kHFSMaxAttrNameLen];
} HFS_ALIGNMENT;
typedef struct HFSPlusAttrKey HFSPlusAttrKey;

#define kHFSPlusAttrKeyMaximumLength (sizeof (HFSPlusAttrKey) - sizeof (u_int16_t))
#define kHFSPlusAttrKeyMinimumLength (kHFSPlusAttrKeyMaximumLength - kHFSMaxAttrNameLen*sizeof (u_int16_t))

#endif

enum {
	kHFSPlusExtentKeyMaximumLength  = sizeof (HFSPlusExtentKey) - sizeof (u_int16_t),
	kHFSExtentKeyMaximumLength      = sizeof (HFSExtentKey) - sizeof (u_int8_t),
	kHFSPlusCatalogKeyMaximumLength = sizeof (HFSPlusCatalogKey) - sizeof (u_int16_t),
	kHFSPlusCatalogKeyMinimumLength = kHFSPlusCatalogKeyMaximumLength - sizeof (HFSUniStr255) + sizeof (u_int16_t),
	kHFSCatalogKeyMaximumLength     = sizeof (HFSCatalogKey) - sizeof (u_int8_t),
	kHFSCatalogKeyMinimumLength     = kHFSCatalogKeyMaximumLength - (kHFSMaxFileNameChars + 1) + sizeof (u_int8_t),
	kHFSPlusCatalogMinNodeSize      = 4096,
	kHFSPlusExtentMinNodeSize       = 512,
	kHFSPlusAttrMinNodeSize         = 4096
};

enum {

	kHFSVolumeHardwareLockBit      =  7,
	kHFSVolumeUnmountedBit         =  8,
	kHFSVolumeSparedBlocksBit      =  9,
	kHFSVolumeNoCacheRequiredBit   = 10,
	kHFSBootVolumeInconsistentBit  = 11,
	kHFSCatalogNodeIDsReusedBit    = 12,
	kHFSVolumeJournaledBit         = 13,
	kHFSVolumeInconsistentBit      = 14,
	kHFSVolumeSoftwareLockBit      = 15,

	kHFSVolumeHardwareLockMask     = 1 << kHFSVolumeHardwareLockBit,
	kHFSVolumeUnmountedMask        = 1 << kHFSVolumeUnmountedBit,
	kHFSVolumeSparedBlocksMask     = 1 << kHFSVolumeSparedBlocksBit,
	kHFSVolumeNoCacheRequiredMask  = 1 << kHFSVolumeNoCacheRequiredBit,
	kHFSBootVolumeInconsistentMask = 1 << kHFSBootVolumeInconsistentBit,
	kHFSCatalogNodeIDsReusedMask   = 1 << kHFSCatalogNodeIDsReusedBit,
	kHFSVolumeJournaledMask        = 1 << kHFSVolumeJournaledBit,
	kHFSVolumeInconsistentMask     = 1 << kHFSVolumeInconsistentBit,
	kHFSVolumeSoftwareLockMask     = 1 << kHFSVolumeSoftwareLockBit,
	kHFSMDBAttributesMask          = 0x8380
};

struct HFSMasterDirectoryBlock {
	u_int16_t           drSigWord;
	u_int32_t           drCrDate;
	u_int32_t           drLsMod;
	u_int16_t           drAtrb;
	u_int16_t           drNmFls;
	u_int16_t           drVBMSt;
	u_int16_t           drAllocPtr;
	u_int16_t           drNmAlBlks;
	u_int32_t           drAlBlkSiz;
	u_int32_t           drClpSiz;
	u_int16_t           drAlBlSt;
	u_int32_t           drNxtCNID;
	u_int16_t           drFreeBks;
	u_int8_t            drVN[kHFSMaxVolumeNameChars + 1];
	u_int32_t           drVolBkUp;
	u_int16_t           drVSeqNum;
	u_int32_t           drWrCnt;
	u_int32_t           drXTClpSiz;
	u_int32_t           drCTClpSiz;
	u_int16_t           drNmRtDirs;
	u_int32_t           drFilCnt;
	u_int32_t           drDirCnt;
	u_int32_t           drFndrInfo[8];
	u_int16_t           drEmbedSigWord;
	HFSExtentDescriptor drEmbedExtent;
	u_int32_t           drXTFlSize;
	HFSExtentRecord     drXTExtRec;
	u_int32_t           drCTFlSize;
	HFSExtentRecord     drCTExtRec;
} HFS_ALIGNMENT;
typedef struct HFSMasterDirectoryBlock	HFSMasterDirectoryBlock;

#ifdef __APPLE_API_UNSTABLE
#define SET_HFS_TEXT_ENCODING(hint)  \
	(0x656e6300 | ((hint) & 0xff))
#define GET_HFS_TEXT_ENCODING(hint)  \
	(((hint) & 0xffffff00) == 0x656e6300 ? (hint) & 0x000000ff : 0xffffffffU)
#endif

struct HFSPlusVolumeHeader {
	u_int16_t	signature;
	u_int16_t	version;
	u_int32_t	attributes;
	u_int32_t	lastMountedVersion;
	u_int32_t	journalInfoBlock;

	u_int32_t	createDate;
	u_int32_t	modifyDate;
	u_int32_t	backupDate;
	u_int32_t	checkedDate;

	u_int32_t	fileCount;
	u_int32_t	folderCount;

	u_int32_t	blockSize;
	u_int32_t	totalBlocks;
	u_int32_t	freeBlocks;

	u_int32_t	nextAllocation;
	u_int32_t	rsrcClumpSize;
	u_int32_t	dataClumpSize;
	u_int32_t	nextCatalogID;

	u_int32_t	writeCount;
	u_int64_t	encodingsBitmap;

	u_int8_t	finderInfo[32];

	HFSPlusForkData	 allocationFile;
	HFSPlusForkData  extentsFile;
	HFSPlusForkData  catalogFile;
	HFSPlusForkData  attributesFile;
	HFSPlusForkData	 startupFile;
} HFS_ALIGNMENT;
typedef struct HFSPlusVolumeHeader HFSPlusVolumeHeader;

enum BTreeKeyLimits{
	kMaxKeyLength	= 520
};

union BTreeKey{
	u_int8_t	length8;
	u_int16_t	length16;
	u_int8_t	rawData [kMaxKeyLength+2];
};
typedef union BTreeKey BTreeKey;

struct BTNodeDescriptor {
	u_int32_t	fLink;
	u_int32_t	bLink;
	int8_t		kind;
	u_int8_t	height;
	u_int16_t	numRecords;
	u_int16_t	reserved;
} HFS_ALIGNMENT;
typedef struct BTNodeDescriptor BTNodeDescriptor;

enum {
	kBTLeafNode	  = -1,
	kBTIndexNode  =  0,
	kBTHeaderNode =  1,
	kBTMapNode    =  2
};

struct BTHeaderRec {
	u_int16_t	treeDepth;
	u_int32_t	rootNode;
	u_int32_t	leafRecords;
	u_int32_t	firstLeafNode;
	u_int32_t	lastLeafNode;
	u_int16_t	nodeSize;
	u_int16_t	maxKeyLength;
	u_int32_t	totalNodes;
	u_int32_t	freeNodes;
	u_int16_t	reserved1;
	u_int32_t	clumpSize;
	u_int8_t	btreeType;
	u_int8_t	keyCompareType;
	u_int32_t	attributes;
	u_int32_t	reserved3[16];
} HFS_ALIGNMENT;
typedef struct BTHeaderRec BTHeaderRec;

enum {
	kBTBadCloseMask          = 0x00000001,
	kBTBigKeysMask           = 0x00000002,
	kBTVariableIndexKeysMask = 0x00000004
};

enum {
	kHFSCaseFolding   = 0xCF,
	kHFSBinaryCompare = 0xBC
};

struct JournalInfoBlock {
	u_int32_t flags;
	u_int32_t device_signature[8];
	u_int64_t offset;
	u_int64_t size;
	u_int32_t reserved[32];
} HFS_ALIGNMENT;
typedef struct JournalInfoBlock JournalInfoBlock;

enum {
    kJIJournalInFSMask          = 0x00000001,
    kJIJournalOnOtherDeviceMask = 0x00000002,
    kJIJournalNeedInitMask      = 0x00000004
};

#ifdef __cplusplus
}
#endif
#ifdef _MSC_VER
# pragma pack(pop)
#endif

#endif
