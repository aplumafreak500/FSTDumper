/*
 *  Luma's FST Dumper
 *  Based on Reggie Dumper
 *  Copyright (C) 2018 aplumafreak500
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

struct Game_ID4 {
	u8 disc_type;
	u16 disc_id;
	u8 region;
}; // 0x4
struct Game_ID6 {
	Game_ID4 id4;
	u16 license_code;
}; // 0x6
struct DiscHeader {
	Game_ID6 id6;
	u8 multidisc_number;
	u8 version;
	u8 hasAudioStreaming;
	u8 audioStreamBufferSize;
	u8 padding[0xE];
	u64 discMagic;
	u8 gameInternalName[0x60];
	u8 disableHashVerification;
	u8 disableEncryption;
	u8 padding2[0x9f];
}; // 0x100
struct Signature {
	u32 SignatureType;
	union {
		u8 RSA2048Key[0x100];
		u8 RSA4096Key[0x200];
		u8 ECKey[0x40];
	} RawSignature;
	u32 publicExponent;
	u8 padding[0x38];
	u8 Issuer[0x40];
};
struct NandTitleID {
	u32 titleIDHi;
	union {
		Game_ID4 titleID;
		u32 IOS;
	} titleIDLo;
}; // 0x8
struct Ticket {
	Signature signature;
	u8 ECDH[0x3c];
	u32 padding1:24;
	u8 TitleKey[0x10];
	u8 unknown;
	u64 TicketID;
	u32 ConsoleID;
	NandTitleID titleID;
	u16 padding2;
	u16 version;
	u32 permittedTitlesMask;
	u32 permitMask;
	u8 canExportWithPRNGKey;
	u8 keyIndex; // 0: Common 1: Korean 2: vWii
	u8 padding3[0x30]; // VC titles have last byte set to 0x1
	u8 accessPermissions[0x40];
	u16 padding4;
	struct {
		u32 enableTimeLimit;
		u32 timeLimit;
	} timeLimits[8];
};
struct TMDContent {
	u32 contentID;
	u16 contentIndex;
	u16 contentType; // 0x0001, set top bit for shared content (nand/shared1) TODO: dlc contents
	u64 size; // why u64?
	u8 sha1Hash[0x14];
};
struct TMD {
	Signature signature;
	u8 version;
	u8 ca_crl_version;
	u8 signer_crl_version;
	u8 unknown;
	NandTitleID requiredIOS;
	NandTitleID titleID;
	u32 titleType; // 1: default 4: DLC 8: Data 16: unknown 32: wfs? 64: unknown CT
	u16 license_code; // last two chars of ID6
	u8 padding[0x3E];
	u32 accessFlags;
	u16 titleVersion;
	u16 contentCount;
	u16 bootIndex;
	u16 padding2;
	// TMDContent contents[contentCount];
};
struct Certificate {
	Signature signature;
	u32 keyType;
	u8 cci[0x40];
	union {
		u8 RSA2048Key[0x100];
		u8 RSA4096Key[0x200];
		u8 ECKey[0x40];
	} PublicKey;
};
struct PartitionHeader {
	Ticket ticket;
	u32 tmdSize;
	u32 tmdOffset;
	u32 certSize;
	u32 certOffset;
	u32 h3Offset;
	u32 DataSize;
	u32 DataOffset;
}; // 0x2c0
struct Partition {
	DiscHeader boot_bin;
	u8 padding[0x320];
	u32 dol_offset;
	u32 fst_offset;
	u32 fst_size;
	u32 fst_size2; // yagcd: multiple DVDs must use it, to properly reside all FSTs
	u8 padding2[0x10];
	u8 bi2_bin[0x2000];
}; // 0x2440
struct PartitionTable {
	u32 PartitionEntryCount;
	u32 PartitionEntryOffset;
} __attribute__((packed)); // 0x8
struct PartitionTableEntry {
	u32 PartitionOffset;
	u32 PartitionID; // 0: Game 1: Update 2: Channel else: Brawl VC
} __attribute__((packed)); // 0x8
struct AgeRatings {
	u8 Cero; // Japan
	u8 ESRB; // US
	u8 unk2;
	u8 Germany;
	u8 Pegi; // Europe
	u8 Finland;
	u8 Portugal;
	u8 England;
	u8 Australia;
	u8 Korea;
}; // 0xB
struct RegionSettings {
	u32 discRegion;
	u8 padding[0xc];
	AgeRatings ageRatings;
	u8 padding2[0x6];
}; // 0x20
struct Wiidisc {
	DiscHeader header_bin;
	u8 padding[0x3ff00];
	union {
		PartitionTableEntry _partitionTableEntries[4];
		u8 padding[0xe000]; // partition table sub-entries here
	} partitionTable;
	RegionSettings region_bin;
	u8 padding2[0x1fdc];
	u32 magic2;
}; // 0x50000
struct ApploaderHeader {
	char BuildDateString[0x10];
	u32 Entrypoint;
	u32 Size1;
	u32 Size2;
	u32 padding;
}; // 0x20
struct DolHeader {
	u32 TextSectionOffsets[7];
	u32 DataSectionOffsets[11];
	u32 TextSectionLoadAddresses[7];
	u32 DataSectionLoadAddresses[11];
	u32 TextSectionSizes[7];
	u32 DataSectionSizes[11];
	u32 BSSAddress;
	u32 BSSSize;
	u32 Entrypoint;
	u32 padding[7];
}; // 0x100
