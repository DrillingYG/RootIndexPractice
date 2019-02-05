#include "NTFSHeader.h"

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

void IndexAttribute::setIndexAttribute(U8 * buf) {
	U32 offset = 0;

	memcpy_s(&(this->comHdr), sizeof(attrCommonHeader), buf, sizeof(attrCommonHeader));
	offset = sizeof(attrCommonHeader);

	memcpy_s(&(this->nonresHdr), sizeof(nonResidentAttrHdr), buf + offset, sizeof(nonResidentAttrHdr));
	offset += sizeof(nonResidentAttrHdr);
	if (this->comHdr.nameLen) {
		this->attrName = wstring(reinterpret_cast<wchar_t*>(buf + offset), this->comHdr.nameLen);
		offset += this->comHdr.nameLen * sizeof(wchar_t);
	}

	U32 curSize = offset;
	while (curSize < comHdr.lenOfAttr) {
		runList RL;
		RL.runLen = *(buf + curSize) & 0x0F;
		RL.runOffset = (*(buf + curSize) & 0xF0) >> 4;
		if (RL.runLen) {
			curSize += 1;
			U64 power = 1;
			for (int a = 0; a < RL.runLen; a = a + 1) {
				RL.runLenVal += power * *(buf + curSize);
				power *= 256;
				curSize += 1;
			}
			power = 1;
			for (int a = 0; a < RL.runOffset; a = a + 1) {
				RL.runOffsetVal += power * *(buf + curSize);
				power *= 256;
				curSize += 1;
			}
			this->recordRunList.emplace_back(RL);
		}
		else break;
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

		memcpy_s(&(nEntry.fileNameAttrContent), sizeof(fileNameAttr), buf + curSize, sizeof(fileNameAttr));
		curSize += sizeof(fileNameAttr);

		if (nEntry.fileNameAttrContent.lenName > 0) {
			wchar_t * tmp = new wchar_t[nEntry.fileNameAttrContent.lenName];
			memcpy_s(tmp, nEntry.fileNameAttrContent.lenName * sizeof(wchar_t), buf + curSize, nEntry.fileNameAttrContent.lenName * sizeof(wchar_t));
			nEntry.filename = tmp;
			curSize += nEntry.fileNameAttrContent.lenName * 2;
			delete[] tmp;
			wcout << "filename : " << nEntry.filename << endl;
		}

		memcpy_s(&(nEntry.VCNofChild), sizeof(U64), buf + curSize, sizeof(U64));
		curSize += sizeof(U64);

		this->nodeEntries.emplace_back(nEntry);
	}
}
void IndexRecord::setIndexRecord(U8 * buf) {
#define ENDNODE "\x00\x00\x00\x00\x00\x00\x00\x00\x10\x00\x00\x00\x02\x00\x00\x00"
	U32 cur = 0, offset = 0;
	memcpy_s(&(this->recordHdr), sizeof(indexRecordHdr), buf + cur, sizeof(indexRecordHdr));
	cur += sizeof(indexRecordHdr);

	memcpy_s(&(this->nodeHdr), sizeof(nodeHeader), buf + cur, sizeof(nodeHeader));
	cur += sizeof(nodeHeader);

	U32 fixupArrayNum = this->nodeHdr.firstEntryOffset - sizeof(nodeHeader);
	if (fixupArrayNum) {
		FixupArray = new U8[fixupArrayNum];
		memcpy_s((this->FixupArray), fixupArrayNum, buf + cur, fixupArrayNum);
		cur += fixupArrayNum;
	}

	bool entryEnd = false;

	offset = cur;
	while (!entryEnd) {
		nodeEntry nEntry;

		cur = offset;

		memcpy_s(&(nEntry.fileReferenceAddr), sizeof(U64), buf + cur, sizeof(U64));
		cur += sizeof(U64);


		memcpy_s(&(nEntry.entrySize), sizeof(U16), buf + cur, sizeof(U16));
		cur += sizeof(U16);

		if (nEntry.entrySize == 0) break;

		memcpy_s(&(nEntry.contentLen), sizeof(U16), buf + cur, sizeof(U16));
		cur += sizeof(U16);

		memcpy_s(&(nEntry.flag), sizeof(U32), buf + cur, sizeof(U32));
		cur += sizeof(U32);

		if (!memcmp(&nEntry, ENDNODE, 16)) break;

		memcpy_s(&(nEntry.fileNameAttrContent), sizeof(fileNameAttr), buf + cur, sizeof(fileNameAttr));
		cur += sizeof(fileNameAttr);

		if (nEntry.fileNameAttrContent.lenName > 0) {
			wchar_t * tmp = new wchar_t[nEntry.fileNameAttrContent.lenName + 1];
			memcpy_s(tmp, nEntry.fileNameAttrContent.lenName * sizeof(wchar_t), buf + cur, nEntry.fileNameAttrContent.lenName * sizeof(wchar_t));
			tmp[nEntry.fileNameAttrContent.lenName] = '\x00';
			nEntry.filename = tmp;
			delete[] tmp;
			cur += nEntry.fileNameAttrContent.lenName * 2;
			while (cur & 7) cur += 1;
		}

		if ((this->nodeHdr.flag) & 0x01) {
			memcpy_s(&(nEntry.VCNofChild), sizeof(U64), buf + cur, sizeof(U64));
			cur += sizeof(U64);
		}


		this->nodeEntries.emplace_back(nEntry);

		offset += nEntry.entrySize;
	}
}

IndexRecord::~IndexRecord() {
	delete[] FixupArray;
}