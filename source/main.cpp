/*
 *  Luma's FST Dumper
 *  Based on Reggie Dumper
 *  Copyright (C) 2012,2018 AerialX, megazig, aplumafreak500
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <wiiuse/wpad.h>

#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <fat.h>

#include <string>
using std::string;

#include "main.h"
#include "wdvd.h"

#define min(a,b) ((a)>(b) ? (b) : (a))
#define max(a,b) ((a)>(b) ? (a) : (b))

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

struct DiscNode;
static DiscNode* fst = NULL;
struct DiscNode {
	u8 Type;
	u32 NameOffset:24;
	u32 DataOffset;
	u32 Size;
	const char* GetName() {
		return (const char*)(fst + fst->Size) + NameOffset;
	}
} __attribute__((packed));

static Partition* partition_data;

static DiscNode* RVL_FindNode(const char* name, DiscNode* root, bool recursive) {
	const char* nametable = (const char*)(fst + fst->Size);
	int offset = root - fst;
	DiscNode* node = root;
	while ((void*)node < (void*)nametable) {
		if (!strcasecmp(nametable + node->NameOffset, name))
			return node;

		if (recursive || node->Type == 0)
			node++;
		else
			node = root + node->Size - offset;
	}

	return NULL;
}

DiscNode* RVL_FindNode(const char* fstname) {
	if (fstname[0] != '/')
		return RVL_FindNode(fstname, fst, true);

	char namebuffer[MAXPATHLEN];
	char* name = namebuffer;
	strcpy(name, fstname + 1);

	DiscNode* root = fst;

	while (root) {
		char* slash = strchr(name, '/');
		if (!slash)
			return RVL_FindNode(name, root + 1, false);

		*slash = '\0';
		root = RVL_FindNode(name, root + 1, false);
		name = slash + 1;
	}

	return NULL;
}

string PathCombine(string path, string file) {
	if (path.empty())
		return file;
	if (file.empty())
		return path;

	bool flag = true;
	while (flag) {
		flag = false;
		if (!strncmp(file.c_str(), "../", 3)) {
			string::size_type pos = path.find_last_of('/', path.size() - 1);
			if (pos != string::npos)
				path = path.substr(0, pos);

			file = file.substr(3);

			flag = true;
		} else if (!strncmp(file.c_str(), "./", 2)) {
			file = file.substr(2);

			flag = true;
		}
	}

	if (path[path.size() - 1] == '/' && file[0] == '/')
		return path + file.substr(1);

	if (path[path.size() - 1] == '/' || file[0] == '/')
		return path + file;

	return path + "/" + file;
}

void DumpDiscHeader(string path) {

	// header
	
	DiscHeader* hdr = NULL;

	WDVD_LowUnencryptedRead(hdr, 0x80, 0);
	
	FILE *hdr_bin = fopen(PathCombine(path, "header.bin").c_str(), "wb");
	if (!hdr_bin)
	{
		printf("Failed to open %s for write\n", PathCombine(path, "header.bin").c_str());
	}
	u32 written = fwrite(&hdr, 1, 0x80, hdr_bin);
	if (written != 0x80) {
		printf("Expected %d got %ld\n", 0x80, written);
		fclose(hdr_bin);
	}
	fclose(hdr_bin);
	free(hdr);
	printf("Dumped header.bin\n");
	
	// region.bin
	
	RegionSettings* rgn = NULL;

	WDVD_LowUnencryptedRead(rgn, 0x20, 0x4e000);
	
	FILE *rgn_bin = fopen(PathCombine(path, "region.bin").c_str(), "wb");
	if (!rgn_bin)
	{
		printf("Failed to open %s for write\n", PathCombine(path, "region.bin").c_str());
	}
	written = fwrite(&rgn, 1, 0x20, rgn_bin);
	if (written != 0x20) {
		printf("Expected %d got %ld\n", 0x20, written);
		fclose(rgn_bin);
	}
	fclose(rgn_bin);
	free(rgn);
	printf("Dumped header.bin\n");
}

bool ReadPartitionTable(string path) {
	PartitionTable partitionTableEntries[4];
	if (WDVD_LowUnencryptedRead(partitionTableEntries, 0x20, 0x40000) || partitionTableEntries[0].PartitionEntryCount == 0)
		return false;
	PartitionTableEntry* partitionTables[4];
	for (u32 i = 0; i < 4; i++) {
		if (WDVD_LowUnencryptedRead(partitionTables, (u64) partitionTableEntries[i].PartitionEntryOffset << 2, (sizeof(PartitionTableEntry)*partitionTableEntries[i].PartitionEntryCount)));
		return false;
	}
	u32 i;
	for (i = 0; i < partitionTableEntries[0].PartitionEntryCount; i++)
		if (partitionTables[i]->PartitionID == 0)
			break;
	if (i >= partitionTableEntries[i].PartitionEntryCount)
		return false;
		
	// read in ticket, tmd, cert, and h3 before calling WDVD_LowOpenPartition

	PartitionHeader* part = NULL;
	WDVD_LowUnencryptedRead(part, 0x2c0, partitionTables[i]->PartitionOffset>>2);
	
	// ticket
	
	FILE *tik_bin = fopen(PathCombine(path, "ticket.bin").c_str(), "wb");
	if (!tik_bin)
	{
		printf("Failed to open %s for write\n", PathCombine(path, "ticket.bin").c_str());
	}
	u32 written = fwrite(&part->ticket, 1, 0x2a4, tik_bin);
	if (written != 0x2a4) {
		printf("Expected %d got %ld\n", 0x2a4, written);
		fclose(tik_bin);
	}
	fclose(tik_bin);
	printf("Dumped ticket.bin\n");
	
	// TMD
	
	FILE *tmd_bin = fopen(PathCombine(path, "tmd.bin").c_str(), "wb");
	if (!tmd_bin)
	{
		printf("Failed to open %s for write\n", PathCombine(path, "tmd.bin").c_str());
	}
	
	u32* tmd = NULL; // tmd can be variable size
	
	WDVD_LowUnencryptedRead(tmd, part->tmdSize, (u64) part->tmdOffset << 2);
	
	written = fwrite(tmd, 1, part->tmdSize, tmd_bin);
	if (written != part->tmdSize) {
		printf("Expected %ld got %ld\n", part->tmdSize, written);
		fclose(tmd_bin);
	}
	fclose(tmd_bin);
	free(tmd);
	printf("Dumped tmd.bin\n");
	
	// Certs
	
	FILE *crt_bin = fopen(PathCombine(path, "cert.bin").c_str(), "wb");
	if (!crt_bin)
	{
		printf("Failed to open %s for write\n", PathCombine(path, "cert.bin").c_str());
	}
	
	u32* crt = NULL; // certs can be variable size
	
	WDVD_LowUnencryptedRead(crt, part->certSize, (u64) part->certOffset << 2);
	
	written = fwrite(tmd, 1, part->certSize, crt_bin);
	if (written != part->certSize) {
		printf("Expected %ld got %ld\n", part->certSize, written);
		fclose(crt_bin);
	}
	fclose(crt_bin);
	free(crt);
	printf("Dumped cert.bin\n");
	
	// H3
	
	FILE *h3_bin = fopen(PathCombine(path, "h3.bin").c_str(), "wb");
	if (!h3_bin)
	{
		printf("Failed to open %s for write\n", PathCombine(path, "h3.bin").c_str());
	}
	
	u32* h3 = NULL; 
	
	WDVD_LowUnencryptedRead(h3, 0x18000, (u64) part->h3Offset << 2);
	
	written = fwrite(h3, 1, 0x18000, h3_bin);
	if (written != 0x18000) {
		printf("Expected %d got %ld\n", 0x18000, written);
		fclose(h3_bin);
	}
	fclose(h3_bin);
	free(h3);
	printf("Dumped h3.bin\n");
	
	free(part);
	
	if (WDVD_LowOpenPartition((u64) partitionTables[i]->PartitionOffset << 2))
		return false;
	free(partitionTableEntries);
	free(partitionTables);
	return true;
}

bool Launcher_ReadFST(string path) {

	// partition header

	if (WDVD_LowRead(partition_data, 0x2440, 0))
		return false;
	
	FILE *boot_bin = fopen(PathCombine(path, "boot.bin").c_str(), "wb");
	if (!boot_bin)
	{
		printf("Failed to open %s for write\n", PathCombine(path, "boot.bin").c_str());
	}
	u32 written = fwrite(&partition_data->boot_bin, 1, 0x420, boot_bin); // reads past boot.bin, but memory was set anyway by above WDVD_LowRead call, so no worrying about leaks, etc.
	if (written != 0x420) {
		printf("Expected %d got %ld\n", 0x420, written);
		fclose(boot_bin);
	}
	fclose(boot_bin);
	printf("Dumped boot.bin\n");
	
	// bi2
	
	FILE *bi2_bin = fopen(PathCombine(path, "bi2.bin").c_str(), "wb");
	if (!bi2_bin)
	{
		printf("Failed to open %s for write\n", PathCombine(path, "bi2.bin").c_str());
	}
	written = fwrite(&partition_data->bi2_bin, 1, 0x2000, bi2_bin);
	if (written != 0x2000) {
		printf("Expected %d got %ld\n", 0x2000, written);
		fclose(bi2_bin);
	}
	fclose(bi2_bin);
	printf("Dumped bi2.bin\n");
	
	// fst

	fst = (DiscNode*)memalign(32, (u64) partition_data->fst_size << 2);
	if (WDVD_LowRead(fst, partition_data->fst_size, (u64) partition_data->fst_offset << 2))
		return false;
	
	FILE *fst_bin = fopen(PathCombine(path, "fst.bin").c_str(), "wb");
	if (!fst_bin)
	{
		printf("Failed to open %s for write\n", PathCombine(path, "fst.bin").c_str());
	}
	written = fwrite(fst, 1, partition_data->fst_size, fst_bin);
	if (written != partition_data->fst_size) {
		printf("Expected %ld got %ld\n", partition_data->fst_size, written);
		fclose(fst_bin);
	}
	fclose(fst_bin);
	printf("Dumped fst.bin\n");
	
	return true;
}

bool Launcher_DiscInserted() {
	bool cover;
	if (!WDVD_VerifyCover(&cover))
		return cover;
	return false;
}

#define HOME_EXIT() { \
	WPAD_ScanPads(); \
	if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME) { \
		if (*(vu32*)0x80001804 != 0x53545542) \
			SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0); \
		else \
			return 0; \
	} \
	VIDEO_WaitVSync(); \
}

bool DumpFolder(DiscNode* node, string path) {
	if (!node)
		return false;

	if (!node->Type)
		return false;

	mkdir(path.c_str(), 0777);
	printf("\nDumping to %s...", path.c_str());

	for (DiscNode* file = node + 1; file < fst + node->Size; ) {
		string name = PathCombine(path, file->GetName());
		if (file->Type) {
			bool ret = DumpFolder(file, name);
			if (!ret)
				return false;
		} else {
			FILE* fd = fopen(name.c_str(), "wb");
			if (fd) {
				static u8 buffer[0x8000] ATTRIBUTE_ALIGN(32);
				memset(buffer, 0, 0x8000);
				u32 written = 0;
				u32 toRead = (file->Size > 0x8000) ? 0x8000 : file->Size;
				while (written < file->Size) {
					if (WDVD_LowRead(buffer, toRead, ((u64)file->DataOffset << 2) + written))
						return false;
					int towrite = MIN(sizeof(buffer), file->Size - written);
					fwrite(buffer, 1, towrite, fd);
					written += towrite;
					if ( file->Size - written < 0x8000 ) toRead = file->Size - written;
				}
				fclose(fd);
				printf(".");
			} else {
				return false;
			}
		}
		file = file->Type ? fst + file->Size : file + 1;
	}

	return true;
}

bool DumpFolder(const char* disc, string path) {
	DiscNode* node = !strcmp(disc, "/") ? fst : RVL_FindNode(disc);
	return DumpFolder(node, path);
}

bool DumpEarlyMemory(string path) {
	FILE *header_nfo = fopen(PathCombine(path, "earlymem.bin").c_str(), "wb");
	if (!header_nfo)
		return false;

	u32 written = fwrite((void*)0x80000000, 1, 0x4000, header_nfo);
	if (written != 0x4000) {
		printf("Expected %d got %ld\n", 0x4000, written);
		fclose(header_nfo);
		return false;
	}
	fclose(header_nfo);
	printf("Dumped earlymem.bin\n");
	return true;
}

bool DumpApploader(string path) {
	ApploaderHeader* apl_nfo = NULL;
	WDVD_LowRead(apl_nfo, 0x20, 0x2440);
	if (!apl_nfo->Size1) return false;
	u32 max = 0;
        u32 offset = 0x20;
        u32 size = apl_nfo->Size1 + apl_nfo -> Size2;
        if ((offset + size) > max) max = offset + size;
	free(apl_nfo);

	void * apl = memalign(32, max);
	if (!apl)
		return false;

	if (WDVD_LowRead(apl, max, 0x2440))
	{
		printf("Failed to read apploader.img\n");
		free(apl);
		return false;
	}

	FILE *appl_img = fopen(PathCombine(path, "apploader.img").c_str(), "wb");
	if (!appl_img)
	{
		printf("Failed to open %s for write\n", PathCombine(path, "apploader.img").c_str());
		free(apl);
		return false;
	}

	bool ret = true;
	u32 written = fwrite(apl, 1, max, appl_img);
	if (written != max)
	{
		printf("Expected %ld got %ld\n", max, written);
		ret = false;
	}
	else
	{
		printf("Dumped apploader.img\n");
	}
	fclose(appl_img);
	free(apl);
	return ret;
}

bool DumpMainDol(string path) {
	DolHeader* dol_nfo = NULL;
	WDVD_LowRead(dol_nfo, 0x100, (u64) partition_data->dol_offset << 2);
	if (!dol_nfo->TextSectionOffsets[0]) return false;
	u32 max = 0;
	for (u32 i = 0; i < 7; i++) {
		u32 offset = dol_nfo->TextSectionOffsets[i];
		u32 size = dol_nfo->TextSectionSizes[i];
		if ((offset + size) > max) max = offset + size;
	}
	for (u32 i = 0; i < 11; i++) {
		u32 offset = dol_nfo->DataSectionOffsets[i];
		u32 size = dol_nfo->DataSectionSizes[i];
		if ((offset + size) > max) max = offset + size;
	}
	free(dol_nfo);

	void * dol = memalign(32, max);
	if (!dol)
		return false;

	if (WDVD_LowRead(dol, max, (u64) partition_data->dol_offset << 2))
	{
		printf("Failed to read main.dol\n");
		free(dol);
		return false;
	}

	FILE *main_dol = fopen(PathCombine(path, "main.dol").c_str(), "wb");
	if (!main_dol)
	{
		printf("Failed to open %s for write\n", PathCombine(path, "main.dol").c_str());
		free(dol);
		return false;
	}

	bool ret = true;
	u32 written = fwrite(dol, 1, max, main_dol);
	if (written != max)
	{
		printf("Expected %ld got %ld\n", max, written);
		ret = false;
	}
	else
	{
		printf("Dumped main.dol\n");
	}
	fclose(main_dol);
	free(dol);
	return ret;
}

int main(void) {
	VIDEO_Init();
	WPAD_Init();
	rmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	CON_Init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	VIDEO_WaitVSync();

	printf("\x1b[2;0H");
	printf("Luma's FST Dumper v1.0\nBased on Reggie! Dumper\n\n");
	printf("Press Home at any time to exit.\n");

	printf("Looking for SD/USB... ");
	while (!fatInitDefault())
		HOME_EXIT();

	printf("Found!\n");

	WDVD_Init();

	printf("Insert disc... ");

	WDVD_Reset();

	while (!Launcher_DiscInserted())
		HOME_EXIT();

	printf("Done.\n");

	printf("Checking disc...\n");

	WDVD_LowReadDiskId();

	printf("Press A to dump the entire disc.\n");
	int select = -1;
	while (select < 0) {
		HOME_EXIT();
		if (WPAD_ButtonsDown(0) & WPAD_BUTTON_A)
			select = 0;
	}

	printf("Dumping files off of the disc...\n");
	
	string GameID;
	memcpy(&GameID, (u32*)0x80000000, 4);
	string DumpDirectory = PathCombine("/FSTDump", GameID);
	mkdir("/FSTDump", 0777);
	mkdir(DumpDirectory.c_str(), 0777);
	mkdir(PathCombine(DumpDirectory, "disc").c_str(), 0777);
	mkdir(PathCombine(DumpDirectory, "files").c_str(), 0777);
	mkdir(PathCombine(DumpDirectory, "sys").c_str(), 0777);
	
	DumpDiscHeader(PathCombine(DumpDirectory, "disc")); // has to be done here
	
	if (!(ReadPartitionTable(DumpDirectory))) {
		printf("Could not open the game partition. Press Home to exit, and try again.\n");
		while (true)
			HOME_EXIT();
	}
	
	if (!Launcher_ReadFST(PathCombine(DumpDirectory, "sys"))) {
		printf("There was an error reading the disc. Press Home to exit, and try again.\n");
		while (true)
			HOME_EXIT();
	}

	DumpEarlyMemory(DumpDirectory);
	DumpApploader(PathCombine(DumpDirectory, "sys"));
	DumpMainDol(PathCombine(DumpDirectory, "sys"));

	if (!DumpFolder("/",PathCombine(DumpDirectory, "files"))) {
		printf("\nThe files could not be read from disc. Press Home to exit, and try again.\n");
		while (true)
			HOME_EXIT();
	}

	fatUnmount("sd:");
	fatUnmount("usb:");

	printf("\nWe're all done.\n");
	printf("Press Home to exit.\n\n\n");
	while (true)
		HOME_EXIT();

	return 0;
}

