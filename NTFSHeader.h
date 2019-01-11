#pragma once
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <string>
#include <Windows.h>
#include <vector>
#include <cwchar>
#include <iomanip>

using namespace std;

#define SECTORSIZE 512
#define CLUSTERSIZE 4096
#define PARTITIONTABLESIZE 16
#define PARTITIONTABLEOFFSET 446
#define PARTITION_ENTRY_SIZE 128

#define NTFS 0x07
#define GPT 0xEE
#define NO_FS 0x00
#define LINE_SEP "------------------------------------------"

typedef uint64_t U64;
typedef uint32_t U32;
typedef uint16_t U16;
typedef uint8_t U8;
typedef int64_t S64;
typedef int32_t S32;
typedef int16_t S16;
typedef int8_t S8;

#pragma pack(1)
typedef struct __GPTHeader {
	U8 Signature[8];
	U8 Revision[4];
	U32 HeaderSize;
	U32 HEaderChksum;
	U32 Reserved;
	U64 LBAofGPTHeader;
	U64 LBAofbkpGPTHeader;
	U64 StartingLBAforPartitions;
	U64 EndingLBAforPratitions;
	U8 GUID[16];
	U64 StartingLBAParitionTable;
	U32 NumOfPartitionEntries;
	U32 SizeOfEntries;
	U32 PartitionTableChksum;
	U8 Padding[420];
} GPTHeader;

typedef struct __GPTable {
	U8 PartitonTypeGUID[16];
	U8 PartitionGUID[16];
	U64 PartitionStartingLBA;
	U64 PartitionEndeingLBA;
	U64 PartitionAtts;
	U8 PartitonName[72];
} GPTable;

typedef struct __BootRecord {
	U8 JumpBootCode[3];
	U8 OEM_ID[8];
	U16 BytesPerSector;
	U8 SecPerClus;
	U16 ReservedSectors;
	U8 Unused1[5];
	U8 Media;
	U8 Unused2[18];
	U64 TotalSectors;
	U64 StartClusterforMFT;
	U64 StartClusterforMFTMirr;
	U32 ClusPerEntry;
	U32 ClusPerIndex;
	U64 VolumeSerialNumber;
	U8 Unused[430];
	U16 Siganture;
	U64 BytesPerClus;
	U64 VBR_LBA;
} VBR;

typedef struct __FixupArr {
	U16 arrEntries[4];
} FixupArr;

typedef struct __MFTHeader {
	U8 Signature[4];
	U16 FixupArrOffset;
	U16 FixupArrEntries;
	U64 LSN;
	U16 SeqNum;
	U16 HardlinkCnt;
	U16 FileAttrOffset;
	U16 Flags;
	U32 RealSizeofMFTEntry;
	U32 AllocatedSizeofMFTEntry;
	U64 FileReference;
	U16 NextAttrID;
	U8 Unused[6];
	FixupArr fixupArr;
} MFTHeader;


typedef struct __attrCommonHeader {
	U32 AttrtypeID;
	U32 lenOfAttr;
	U8 Nregflag;
	U8 nameLen;
	U16 OffsettoName;
	U16 Flags;
	U16 attrID;
} attrCommonHeader;

typedef struct __residentAttrHdr {
	U32 sizeOfContent;
	U16 offsetToContent;
	U8 idxedFlag;
	U8 Padding;
} residentAttrHdr;

typedef struct __nonResidentAttr {
	U64 startVcn;
	U64 endVcn;
	U16 runListOffset;
	U16 compUnitSize;
	U32 padding;
	U64 allocatedSize;
	U64 realSize;
	U64 initSize;
} nonResidentAttrHdr;

typedef struct __runList {
	U8 runOffset : 4;
	U8 runLen : 4;
	U64 runOffsetVal = 0;
	U64 runLenVal = 0;
} runList;

typedef struct __STDINFO {
	U64 createTime;
	U64 modifiedTime;
	U64 mftModifiedTime;
	U64 lastAccessedTime;
	U32 Flags;
	U32 MaxVersionNum;
	U32 VersionNum;
	U32 ClassID;
	U32 OwnerID;
	U32 SecID;
	U64 QuotaCharged;
	U64 UCN;
} stdInfo;

typedef struct __FILENAME {
	U64 fileReferenceAddrofParent;
	U64 createTime;
	U64 modifiedTime;
	U64 mftModifiedTime;
	U64 lastAccessedTime;
	U64 allocatedSize;
	U64 realsSize;
	U32 flags;
	U32 reparseValue;
	U8 lenName;
	U8 nameSpace;
} fileNameAttr;

typedef struct __INDEX_ROOT_HEADER {
	U32 typeOfAttr;
	U32 CollationRule;
	U32 recordByteSize;
	U8 recordClusterSize;
	U8 Unused[3];
} indexRootHdr;

typedef struct __INDEX_RECORD_HEADER {
	U8 signature[8];
	U16 fixupArrOffset;
	U16 fixupArrEntries;
	U64 LSN;
	U64 VCN;
} indexRecordHdr;

typedef struct __NODE_HEADER {
	U32 firstEntryOffset;
	U32 totalSize;
	U32 allocatedSize;
	U32 flag;
} nodeHeader;

typedef struct __NODE_ENTRY {
	U64 fileReferenceAddr;
	U16 entrySize;
	U16 contentLen;
	U32 flag;
	U8 * filenameContent;
	U64 VCNofChild;
} nodeEntry;

class IndexRoot {
public:
	void setIndexRoot(U8 * buf);
	~IndexRoot();
private:
	attrCommonHeader comHdr;
	residentAttrHdr resHdr;
	wstring attrName;
	indexRootHdr IRH;
	nodeHeader nodeHdr;
	vector<nodeEntry> nodeEntries;
};

class IndexAttribute {
public:
	void setIndexAttribute(U8 * buf);
private:
	attrCommonHeader comHdr;
	nonResidentAttrHdr nonresHdr;
	wstring attrName;
	vector<runList> recordRunList;
};

class IndexRecord {

private:
	indexRecordHdr recordHdr;
	nodeHeader nodeHdr;
	U8 * FixupArray;
	vector<nodeEntry> nodeEntries;
};

class MFTEntry {

public:
	MFTEntry(void);
	MFTEntry(U32 targetNum);

	const U32 getEntryNum(void) const;
	U32 getEntrySize(void);
	U8 * getBuf(void);

	void setMFTEntry(void * buf, uint32_t MFTSize);

	void printMftInfo();
	void printStdInfo();
	void printFileNameInfo();

private:
	union {
		U8 Buf[1024];
		MFTHeader mftHdr;
	};
	U32 curPtr;
	U32 mftNum;
	wchar_t * filename;
};
#pragma pack()

void errorMsg(string error);
void hexdump(void * buf, U32 size);
U32 getHDD(U8 * HDDdump, U64 offset, U32 size);
U32 BIOSUEFI(U8 * HDDdump, vector<U32> & PartitionType);
U64 setVBR(U8 * HDDdump, VBR &vbr);
void findRootEntry(const VBR & vbr, MFTEntry & mEntry);
U16 betole16(U16 num);
U32 betole32(U32 num);
U64 betole64(U64 num);
void findIndexRootAttr(const U8 * mEntry, IndexRoot & idxRoot);
void findIndexAttrAttr(const U8 * mEntry, IndexAttribute & idxRoot);