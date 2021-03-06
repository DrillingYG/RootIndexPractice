//https://blog.naver.com/empty7792/221380762099
//https://github.com/DrillingYG/MFTParser
/*
1. StartClusterforMFT는 왜 자동으로 Little Endian으로 바뀌는가?
2. 여러개의 NTFS FileSystem 중에서 내 C:인 것은 어떻게 찾을 수 있는가?($Volume에서 확인 가능할 것이라고 추측)
3. static 변수를 사용해서 MFT Entry size를 하드코딩 하지 않고 사용할 수 있겠는가?
*/
#include "NTFSHeader.h"


int main(U32 argc, const char * argv[])
{
	if (argc != 2) {
		fprintf(stderr, "usage : RootIndexPractice.exe [filename]");
		return 1;
	}

	U8 * HDDdump = new U8[sizeof(U8) * SECTORSIZE];
	U8 pType;
	VBR vbr;
	vector<U32> PartitionTypes(4, 0);
	MFTEntry mEntry(5);
	IndexRoot idxRoot;
	IndexAttribute idxAttr;

	wchar_t filename[1000];
	mbstowcs_s(NULL, filename, 1000, argv[1], 1000);

	if (!getHDD(HDDdump, 0, SECTORSIZE)) {
		return 1;
	}

	memset(&vbr, 0, sizeof(VBR));
	pType = BIOSUEFI(HDDdump, PartitionTypes);
	switch (pType) {
	case GPT:
		getHDD(HDDdump, SECTORSIZE, SECTORSIZE);
		setVBRFromGPT(HDDdump, vbr);
		if (vbr.VBR_LBA == -1) errorMsg("Cannot find NTFS VBR");
		break;
	case NTFS:
		setVBRFromBIOS(HDDdump, vbr);
		break;
	}

	findRootEntry(vbr, mEntry);
	findIndexRootAttr(reinterpret_cast<const U8*>(&mEntry), idxRoot);
	findIndexAttrAttr(reinterpret_cast<const U8*>(&mEntry), idxAttr);
	
	analyzeIndex(idxRoot, idxAttr, wstring(filename), vbr);
	return 0;
}

void errorMsg(string error) {
	cerr << error << endl;
	exit(1);
}

void setVBRFromGPT(U8 * HDDdump, VBR &vbr) {
	GPTHeader Gheader;
	GPTable PT;
	U64 PToffset;
	U32 GUIDoffset;
	memset(&Gheader, 0, sizeof(GPTHeader));
	memcpy(&Gheader, HDDdump, sizeof(U8) * SECTORSIZE);

	PToffset = Gheader.StartingLBAParitionTable * SECTORSIZE;

	delete[] HDDdump;
	HDDdump = new U8[sizeof(U8) * SECTORSIZE * 2];

	getHDD(HDDdump, PToffset, SECTORSIZE * 2);

	GUIDoffset = 0;

#define FIRST_PARTITION_LBA 32
#define GUID_LEN 16
#define NO_GUID "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
#define MY_CDRIVE "\x00\xb0\x11\x00"

	while (true) {
		memcpy(&PT, HDDdump + GUIDoffset, sizeof(U8) * Gheader.SizeOfEntries);
		if (!memcmp(&(PT.PartitionStartingLBA), MY_CDRIVE, sizeof(U8) * 4)) {
			getHDD((U8*)(&vbr), PT.PartitionStartingLBA * SECTORSIZE, SECTORSIZE);
			vbr.VBR_LBA = PT.PartitionStartingLBA;
			delete[] HDDdump;
			return;
		}
		GUIDoffset += sizeof(GPTable);
	}
}

void setVBRFromBIOS(U8 * HDDdump, VBR &vbr) {
	U32 LBA;
	U32 offset = MBR_PARTITIONTABLEOFFSET + 4;

	while (*(HDDdump + offset) != NTFS) offset += PARTITION_ENTRY_SIZE;
	
	offset += 4;

	memcpy_s(&LBA, sizeof(U32), HDDdump + offset, sizeof(U32));
	delete[] HDDdump;
	HDDdump = new U8[SECTORSIZE];
	getHDD(HDDdump, LBA * SECTORSIZE, SECTORSIZE);

	memcpy_s(&vbr, sizeof(VBR), HDDdump, sizeof(VBR));

	vbr.VBR_LBA = LBA;
}

void findRootEntry(const VBR &vbr, MFTEntry & mEntry) {
	U64 lbaOffset;
	U8 tmpBuf[1024];
	lbaOffset = vbr.VBR_LBA * SECTORSIZE + vbr.StartClusterforMFT * CLUSTERSIZE;
	for (U32 i = 0; i < mEntry.getEntryNum(); i++) lbaOffset += 1024;
	getHDD(tmpBuf, lbaOffset, 1024);
	mEntry.setMFTEntry(tmpBuf, sizeof(MFTEntry));
}

U32 BIOSUEFI(U8 * HDDdump, vector<U32> & PartitionType) {
	if (memcmp(HDDdump + SECTORSIZE - 2, "\x55\xAA", sizeof(U8) * 2)) {
		cerr << "error : FileSystem Signature" << endl;
		exit(1);
	}

#define BOOTABLEFLAG_OFFSET 0
#define UNBOOTABLE 0

#define FSTYPE_OFFSET 4

	for (U32 a = 0; a < 4; a = a + 1) {
		U32 offset = MBR_PARTITIONTABLEOFFSET + a * PARTITION_ENTRY_SIZE;
		PartitionType[a] = HDDdump[offset + FSTYPE_OFFSET];
		if (HDDdump[offset + FSTYPE_OFFSET] == NO_FS) {
			continue;
		}

		if (HDDdump[offset + BOOTABLEFLAG_OFFSET] == UNBOOTABLE) {
			printf("Unbootable file system #%d\n", a + 1);
		}

		switch (HDDdump[offset + FSTYPE_OFFSET]) {
		case NTFS:
			printf("Partition table #%d : NTFS\n", a + 1);
			return NTFS;
		case GPT:
			printf("Partition table #%d : GUID Partition Table\n", a + 1);
			return GPT;
		default:
			printf("Partition table #%d : UNKNOWN\n", a + 1);
			break;
		}
	}

	return NO_FS;
}

U32 getHDD(U8 * HDDdump, U64 offset, U32 size) {
	HANDLE handle;
	wstring filename(L"\\\\.\\PhysicalDrive1");
	U32 nRead;
	U32 pointerRet;
	LONG addrHigh = (offset & 0xFFFFFFFF00000000) >> 32;
	LONG addrLow = (offset & 0xFFFFFFFF);
	handle = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);

	if (handle == INVALID_HANDLE_VALUE) {
		puts("invalid handle");
		exit(1);
	}

	pointerRet = SetFilePointer(handle, addrLow, &addrHigh, FILE_BEGIN);
	if (pointerRet == INVALID_SET_FILE_POINTER) {
		puts("invalid set file pointer");
		exit(1);
	}

	if (!ReadFile(handle, HDDdump, size, (DWORD*)&nRead, NULL)) {
		cout << "Cannot Read File " << endl;
		cout << GetLastError() << endl;
		exit(1);
	}

	CloseHandle(handle);

	return 1;
}
  

void hexdump(void * buf, U32 size) {
	printf("        ");
	for (U32 a = 0; a < 16; a = a + 1) {
		printf("%02X ", a);
	}
	puts("");

	for (U32 a = 0; a < 55; a = a + 1) putchar((a < 8 ? ' ' : '-'));
	puts("");

	for (U32 a = 0; a < size; a = a + 1) {
		if (a % 16 == 0) printf("0x%04X  ", a);
		printf("%02X ", *((U8*)buf + a));
		if ((a + 1) % 16 == 0 || a == size - 1) puts("");
		if ((a + 1) % 512 == 0) {
			for (U32 b = 0; b < 55; b = b + 1) putchar('-');
			puts("");
		}
	}
	puts("");
}

U64 betole64(U64 num) {
	U64 ret;
	ret = ((num & 0x00000000000000FF) << 56)
		| ((num & 0x000000000000FF00) << 40)
		| ((num & 0x0000000000FF0000) << 24)
		| ((num & 0x00000000FF000000) << 8)
		| ((num & 0x000000FF00000000) >> 8)
		| ((num & 0x0000FF0000000000) >> 24)
		| ((num & 0x00FF000000000000) >> 40)
		| ((num & 0xFF00000000000000) >> 56);
	return ret;
}

U32 betole32(U32 num) {
	U32 ret;
	ret = ((num & 0x000000FF) << 24)
		| ((num & 0x0000FF00) << 8)
		| ((num & 0x00FF0000) >> 8)
		| ((num & 0xFF000000) >> 24);
	return ret;
}

U16 betole16(U16 num) {
	U16 ret;
	cout << "before : " << num << endl;
	ret = ((num & 0x00FF) << 8)
		| ((num & 0xFF00) >> 8);

	cout << "ret :" << ret << endl << endl;
	return ret;
}


void findIndexRootAttr(const U8 * mEntry, IndexRoot & idxRoot) {
#define INDEX_ROOT_TYPEID 0x90
	U32 offset = sizeof(MFTHeader);
	U32 typeId = 0x10;
	attrCommonHeader comHdr;
	memcpy_s(&comHdr, sizeof(comHdr), mEntry + offset, sizeof(comHdr));
	while (typeId != INDEX_ROOT_TYPEID) {
		typeId = const_cast<U8 *>(mEntry)[offset];
		if (typeId == INDEX_ROOT_TYPEID) break;
		offset += comHdr.lenOfAttr;
		memcpy_s(&comHdr, sizeof(comHdr), mEntry + offset, sizeof(comHdr));
	}

	idxRoot.setIndexRoot(const_cast<U8*>(mEntry) + offset);
}

void findIndexAttrAttr(const U8 * mEntry, IndexAttribute & idxAttr) {
#define INDEX_ATTRIBUTE_TYPEID 0xA0
	U32 offset = sizeof(MFTHeader);
	U32 typeId = 0x10;
	attrCommonHeader comHdr;
	memcpy_s(&comHdr, sizeof(comHdr), mEntry + offset, sizeof(comHdr));
	while (typeId != INDEX_ATTRIBUTE_TYPEID) {
		typeId = const_cast<U8 *>(mEntry)[offset];
		if (typeId == INDEX_ATTRIBUTE_TYPEID) break;
		offset += comHdr.lenOfAttr;
		memcpy_s(&comHdr, sizeof(comHdr), mEntry + offset, sizeof(comHdr));
	}

	idxAttr.setIndexAttribute(const_cast<U8*>(mEntry) + offset);
}

void analyzeIndex(const IndexRoot & idxRoot, const IndexAttribute & idxAttr, wstring targetFileName, const VBR & vbr) {
	for (auto & nodeEntry : idxRoot.nodeEntries) {
		if (nodeEntry.contentLen == 0) continue;
		
		if (nodeEntry.fileNameAttrContent.lenName > 0 && nodeEntry.filename == targetFileName) {
			return;
		}
	}

	for (auto & runList : idxAttr.recordRunList) {
		U64 offset = runList.runOffsetVal * CLUSTERSIZE;
		U64 len = runList.runLenVal;
		U32 vcnCnt = 0;
		while (!analyzeIndexRecord(offset + vbr.VBR_LBA * SECTORSIZE, len, targetFileName, vbr)) {
			offset += 4096;
		}
	}
}

bool analyzeIndexRecord(U64 offset, U64 len, wstring targetFileName, const VBR & vbr) {
	IndexRecord indexrcd;
	U8 HDDdump[CLUSTERSIZE];

	getHDD(HDDdump, offset, CLUSTERSIZE);
	indexrcd.setIndexRecord(HDDdump);
	
	for (auto & entry : indexrcd.nodeEntries) {
		if (entry.filename == targetFileName) {
			wcout << "found " << endl;
			analyzeDataAttr(entry.fileReferenceAddr, vbr);
			return true;
		}
	}
	
	return false;
}

void analyzeDataAttr(U64 fileReferenceAddr, const VBR & vbr) {
#define DATA_TYPE 128
	U64 offset = vbr.VBR_LBA * SECTORSIZE + vbr.StartClusterforMFT * CLUSTERSIZE + (fileReferenceAddr & 0xFFFFFFFFFFFF) * MFTENTRYSIZE;
	MFTEntry targetMft;
	U8 * HDDdump = new U8[MFTENTRYSIZE];
	getHDD(HDDdump, offset, MFTENTRYSIZE);
	targetMft.setMFTEntry(HDDdump, MFTENTRYSIZE);
	offset = sizeof(MFTHeader);

	attrCommonHeader cmnHdr;
	memcpy_s(&cmnHdr, sizeof(attrCommonHeader), HDDdump + offset, sizeof(attrCommonHeader));
	while (cmnHdr.AttrtypeID != DATA_TYPE) {
		offset += cmnHdr.lenOfAttr;
		memcpy_s(&cmnHdr, sizeof(attrCommonHeader), HDDdump + offset, sizeof(attrCommonHeader));
	}

	
	if (!(cmnHdr.Nregflag)) {
		U8 * DataAttr = new U8[cmnHdr.lenOfAttr];
		memcpy_s(DataAttr, cmnHdr.lenOfAttr, HDDdump + offset, cmnHdr.lenOfAttr);
		hexdump(DataAttr, cmnHdr.lenOfAttr);

		delete[] DataAttr;
		delete[] HDDdump;
	}
	else {
		runList runlist;

		offset += sizeof(attrCommonHeader);
		offset += sizeof(nonResidentAttrHdr);
		if (cmnHdr.nameLen) offset += cmnHdr.nameLen * 2;

		if ((*(HDDdump + offset) & 0x0F)) {
			runlist.runLen = (*(HDDdump + offset) & 0x0F);
			runlist.runOffset = ((*(HDDdump + offset) & 0xF0) >> 4);
			offset += 1;
			memcpy_s(&runlist.runLenVal, runlist.runLen, HDDdump + offset, runlist.runLen);
			offset += runlist.runLen;
			memcpy_s(&runlist.runOffsetVal, runlist.runOffset, HDDdump + offset, runlist.runOffset);
			offset += runlist.runOffset;

			U64 realDataOffset = runlist.runOffsetVal * CLUSTERSIZE + vbr.VBR_LBA * SECTORSIZE;
			U8 * dataDump = new U8[runlist.runLenVal * CLUSTERSIZE];
			U32 len;
			getHDD(dataDump, realDataOffset, runlist.runLenVal * CLUSTERSIZE);
			
			len = wcslen((const wchar_t*)dataDump);

			wchar_t * content = new wchar_t[len + 1];
			memcpy_s(content, sizeof(wchar_t) *len, dataDump, sizeof(wchar_t) * len);
			
			content[len] = L'\0';
			wstring temp(content);

			wprintf(L"%s", content);

			delete[] content;
			delete[] dataDump;	
		}
		else delete[] HDDdump;
	}
}