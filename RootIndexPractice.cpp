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
	VBR vbr;
	vector<U32> PartitionType(4, 0);
	MFTEntry mEntry(5);
	IndexRoot idxRoot;
	IndexAttribute idxAttr;
	wchar_t filename[1000];
	mbstowcs_s(NULL, filename, 1000, argv[1], 1000);

	if (!getHDD(HDDdump, 0, SECTORSIZE)) {
		return 1;
	}

	if (BIOSUEFI(HDDdump, PartitionType) == GPT) {
		getHDD(HDDdump, SECTORSIZE, SECTORSIZE);
		vbr.VBR_LBA = setVBR(HDDdump, vbr);
		if (vbr.VBR_LBA == -1) errorMsg("Cannot find NTFS VBR");
	}

	findRootEntry(vbr, mEntry);
	findIndexRootAttr(reinterpret_cast<const U8*>(&mEntry), idxRoot);
	findIndexAttrAttr(reinterpret_cast<const U8*>(&mEntry), idxAttr);

	
	return 0;
}

void errorMsg(string error) {
	cerr << error << endl;
	exit(1);
}

U64 setVBR(U8 * HDDdump, VBR &vbr) {
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
			return PT.PartitionStartingLBA;
		}

		GUIDoffset += Gheader.SizeOfEntries;
	}
	delete[] HDDdump;

	return 0xFFFFFFFFFFFFFFFF;
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
		U32 offset = PARTITIONTABLEOFFSET + a * PARTITIONTABLESIZE;
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
	wstring filename(L"\\\\.\\PhysicalDrive0");
	U32 nRead;
	U32 pointerRet;
	LONG addrHigh = (offset & 0xFFFFFFFF00000000) >> 32;
	LONG addrLow = (offset & 0xFFFFFFFF);
	handle = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
	if (handle == INVALID_HANDLE_VALUE) {
		puts("invalid handle");
		return 0;
	}

	pointerRet = SetFilePointer(handle, addrLow, &addrHigh, FILE_BEGIN);
	if (pointerRet == INVALID_SET_FILE_POINTER) {
		puts("invalid handle");
		GetLastError();
		return 0;
	}

	if (!ReadFile(handle, HDDdump, size, (DWORD*)&nRead, NULL)) {
		return 0;
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
	ret = ((num & 0x00FF) << 8)
		| ((num & 0xFF00) >> 8);
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
	U8 * tmpBuf = nullptr;
	memcpy_s(&comHdr, sizeof(comHdr), mEntry + offset, sizeof(comHdr));
	tmpBuf = new U8[comHdr.lenOfAttr];
	while (typeId != INDEX_ATTRIBUTE_TYPEID) {
		typeId = const_cast<U8 *>(mEntry)[offset];
		if (typeId == INDEX_ATTRIBUTE_TYPEID) break;
		offset += comHdr.lenOfAttr;
		memcpy_s(&comHdr, sizeof(comHdr), mEntry + offset, sizeof(comHdr));
	}

	memcpy_s(tmpBuf, comHdr.lenOfAttr, mEntry + offset, comHdr.lenOfAttr);
	
	delete[] tmpBuf;
}

MFTEntry::MFTEntry(void) = default;
MFTEntry::MFTEntry(U32 targetNum) :mftNum(targetNum) {}

const U32 MFTEntry::getEntryNum(void) const { return mftNum; }

void MFTEntry::setMFTEntry(void * buf, uint32_t MFTSize) {
	memset(this->Buf, 0, MFTSize);
	memcpy_s(this->Buf, MFTSize, buf, MFTSize);
}

U8 * MFTEntry::getBuf(void) { return Buf; }
U32 MFTEntry::getEntrySize(void) { return this->mftHdr.AllocatedSizeofMFTEntry; }

void MFTEntry::printMftInfo() {
	cout << "<<<<<<<<<<<<<<<<<<<<<<<<  MFT Entry Header >>>>>>>>>>>>>>>>>>>>>>>>" << endl;
	cout << "LSN = " << mftHdr.LSN << endl;
	cout << "Sequence Number = " << mftHdr.SeqNum << endl;
	cout << "Link Count = " << mftHdr.HardlinkCnt << endl;
	cout << "First Attr Offset = " << mftHdr.FileAttrOffset << endl;
	cout << "Flags = " << mftHdr.Flags << endl;
	cout << "Used Sizeof MFT = " << mftHdr.RealSizeofMFTEntry << endl;
	cout << "Allocated size of MFT = " << mftHdr.AllocatedSizeofMFTEntry << endl;
	cout << "File Reference to base record = " << mftHdr.FileReference << endl;
	cout << "Next Attr ID = " << mftHdr.NextAttrID << endl;
	cout << endl << "<<<<<<<<<<<<<<<<<<<<<<<<  Fixup Array >>>>>>>>>>>>>>>>>>>>>>>>" << endl;

	for (int a = 0; a < 4; a = a + 1) cout << "0x" << hex << setw(4) << setfill('0') << betole16(mftHdr.fixupArr.arrEntries[a]) << ' ';
	cout << endl << endl;
}

void MFTEntry::printStdInfo() {
	this->curPtr = mftHdr.FixupArrOffset;
	this->curPtr += 8;

	attrCommonHeader cmnHdr;
	memcpy_s(static_cast<void*>(&cmnHdr), sizeof(attrCommonHeader), Buf + curPtr, sizeof(attrCommonHeader));
	this->curPtr += sizeof(attrCommonHeader);

	residentAttrHdr resHdr;
	memcpy_s(static_cast<void*>(&resHdr), sizeof(residentAttrHdr), Buf + curPtr, sizeof(residentAttrHdr));
	this->curPtr += sizeof(residentAttrHdr);

	stdInfo STDINFO;
	memcpy_s(static_cast<void*>(&STDINFO), resHdr.sizeOfContent, Buf + curPtr, resHdr.sizeOfContent);
	this->curPtr += resHdr.sizeOfContent;

	cout << "<<<<<<<<<<<<<<<<<<<<<<<<  STANDARD INFO >>>>>>>>>>>>>>>>>>>>>>>>" << endl;
	cout << "Created Time : " << STDINFO.createTime << endl;
	cout << "Modified Time : " << STDINFO.modifiedTime << endl;
	cout << "MFT Modified Time : " << STDINFO.mftModifiedTime << endl;
	cout << "Accessed Time : " << STDINFO.lastAccessedTime << endl;

}

void MFTEntry::printFileNameInfo() {
	attrCommonHeader cmnHdr;
	memcpy_s(static_cast<void*>(&cmnHdr), sizeof(attrCommonHeader), Buf + curPtr, sizeof(attrCommonHeader));
	this->curPtr += sizeof(attrCommonHeader);

	residentAttrHdr resHdr;
	memcpy_s(static_cast<void*>(&resHdr), sizeof(residentAttrHdr), Buf + curPtr, sizeof(residentAttrHdr));
	this->curPtr += sizeof(residentAttrHdr);

	fileNameAttr filenameattr;
	memcpy_s(static_cast<void*>(&filenameattr), sizeof(fileNameAttr), Buf + curPtr, sizeof(fileNameAttr));
	this->curPtr += sizeof(fileNameAttr);

	filename = new wchar_t[filenameattr.lenName + 1];
	memcpy_s(static_cast<void*>(filename), sizeof(wchar_t) * filenameattr.lenName,
		Buf + curPtr, sizeof(wchar_t) * filenameattr.lenName);
	filename[filenameattr.lenName] = '\0';

	cout << endl << "<<<<<<<<<<<<<<<<<<<<<<<<  FILENAME >>>>>>>>>>>>>>>>>>>>>>>>" << endl;
	cout << "Flags : " << filenameattr.flags << endl;
	printf("Name length : %d\n", filenameattr.lenName);
	printf("Name space : %d\n", filenameattr.nameSpace);
	wcout << "file Name : " << filename << endl;

	delete[] filename;
}

//U32 IndexAttribute::findFirstEntryOffset() { return sizeof(IndexRoot) + this->nodeHdr.firstEntryOffset; }

void IndexAttribute::setIndexAttribute(U8 * buf) {
	U32 offset = 0;

	memcpy_s(&(this->comHdr), sizeof(attrCommonHeader), buf, sizeof(attrCommonHeader));
	offset = sizeof(attrCommonHeader);

	memcpy_s(&(this->nonresHdr), sizeof(nonResidentAttrHdr), buf + offset, sizeof(nonResidentAttrHdr));
	offset += sizeof(residentAttrHdr);
	if (this->comHdr.nameLen) {
		this->attrName = wstring(reinterpret_cast<wchar_t*>(buf + offset), this->comHdr.nameLen);
		wcout << "attrName : " << this->attrName << endl;
		offset += this->comHdr.nameLen * sizeof(wchar_t);
	}

	
}


void IndexRoot::setIndexRoot(U8 * buf) {
	U32 offset = 0;
	
	memcpy_s(&(this->comHdr), sizeof(attrCommonHeader), buf, sizeof(attrCommonHeader));
	offset = sizeof(attrCommonHeader);
	
	memcpy_s(&(this->resHdr), sizeof(residentAttrHdr), buf + offset, sizeof(residentAttrHdr));
	offset += sizeof(residentAttrHdr);
	if (this->comHdr.nameLen) {
		this->attrName = wstring(reinterpret_cast<wchar_t*>(buf + offset), this->comHdr.nameLen);
		wcout << "attrName : " << this->attrName << endl;
		offset += this->comHdr.nameLen * sizeof(wchar_t);
	}
	
	memcpy_s(&(this->IRH), sizeof(indexRootHdr), buf + offset, sizeof(indexRootHdr));
	offset += sizeof(indexRootHdr);

	memcpy_s(&(this->nodeHdr), sizeof(nodeHeader), buf + offset, sizeof(nodeHeader));
	offset += sizeof(nodeHeader);

	U32 curSize = offset;
	while (curSize < comHdr.lenOfAttr) {
		nodeEntry nEntry;
		memcpy_s(&(nEntry.fileReferenceAddr), sizeof(U64), buf + curSize, sizeof(U64));
		curSize += sizeof(U64);
		
		memcpy_s(&(nEntry.entrySize), sizeof(U16), buf + curSize, sizeof(U16));
		curSize += sizeof(U16);
		
		memcpy_s(&(nEntry.contentLen), sizeof(U16), buf + curSize, sizeof(U16));
		curSize += sizeof(U16);
		
		memcpy_s(&(nEntry.flag), sizeof(U32), buf + curSize, sizeof(U32));
		curSize += sizeof(U32);

		nEntry.filenameContent = new U8[nEntry.contentLen];
		memcpy_s(nEntry.filenameContent, nEntry.contentLen, buf + curSize, nEntry.contentLen);
		curSize += nEntry.contentLen;
		
		memcpy_s(&(nEntry.VCNofChild), sizeof(U64), buf + curSize, sizeof(U64));
		curSize += sizeof(U64);

		this->nodeEntries.emplace_back(nEntry);

		hexdump(reinterpret_cast<void*>(&nEntry), nEntry.entrySize - 8);
		hexdump(reinterpret_cast<void*>(&nEntry.VCNofChild), 8);
	}
}

IndexRoot::~IndexRoot() {
	for (auto &a : this->nodeEntries) {
		delete[] a.filenameContent;
	}
}