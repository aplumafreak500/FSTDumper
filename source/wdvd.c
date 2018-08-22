/*
 *  Wii DVD interface API
 *  Copyright (C) 2008 Jeff Epler <jepler@unpythonic.net>
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
 *
 *  Modified by aplumafreak500 for Luma's FST Dumper and Luma's CTGP Launcher
 *
 */

#include <ogc/ipc.h>
#include <string.h>
#include "wdvd.h"

static int di_fd = -1;

// Apparently GCC is too cool for casting.
static u32 inbuffer[0x10] ATTRIBUTE_ALIGN(32);

int WDVD_Init() {
    if (di_fd >= 0)
		return di_fd;

   	di_fd = IOS_Open("/dev/di", 0);

    return di_fd;
}

bool WDVD_Reset() {
	if (di_fd < 0)
		return false;

	inbuffer[0x00] = 0x8A000000;
	inbuffer[0x01] = 1;
	return (IOS_Ioctl(di_fd, 0x8A, inbuffer, 0x20, inbuffer + 0x08, 0x20)==1);
}

int WDVD_LowUnencryptedRead(void *buf, u32 len, u64 offset) {
	int result;

	if (di_fd<0)
		return -1;

	inbuffer[0] = 0x8d000000;
	inbuffer[1] = len;
	inbuffer[2] = offset >> 2;

	result = IOS_Ioctl(di_fd, 0x8d, inbuffer, 0x20, buf, len);

	return (result==1) ? 0 : -result;
}

int WDVD_LowRead(void *buf, u32 len, u64 offset) {
	int result;

	if (di_fd<0 || (u32)buf&0x1F)
		return -1;

	memset(inbuffer, 0, 0x20);
	inbuffer[0] = 0x71000000;
	inbuffer[1] = len;
	inbuffer[2] = offset >> 2;

	result = IOS_Ioctl(di_fd, 0x71, inbuffer, 0x20, buf, len);

	return (result==1) ? 0 : -result;
}

bool WDVD_LowClosePartition() {

	if (di_fd<0)
		return false;

	inbuffer[0x00] = 0x8C000000;

	return !!IOS_Ioctl(di_fd, 0x8C, inbuffer, 0x20, 0, 0);
}

int WDVD_LowReadDiskId() {
	int result;
	void *outbuf = (void*)0x80000000;

	if (di_fd<0)
		return -1;

	memset(outbuf, 0, 0x20);
	inbuffer[0] = 0x70000000;

	result = IOS_Ioctl(di_fd, 0x70, inbuffer, 0x20, outbuf, 0x20);
	return (result==1) ? 0 : -result;
}

int WDVD_LowReadBCA(void *buf) {
	int result;

	if (di_fd<0)
		return -1;

	inbuffer[0] = 0xda000000;

	result = IOS_Ioctl(di_fd, 0xda, inbuffer, 0x20, buf, 64);

	return (result==1) ? 0 : -result;
}

int WDVD_Eject() {
	int result;

	if (di_fd<0)
		return -1;

	inbuffer[0] = 0xe3000000;
	inbuffer[1] = 1;
	inbuffer[2] = 0;

	result = IOS_Ioctl(di_fd, 0xe3, inbuffer, 0x20, 0, 0x20);

	return (result==1) ? 0 : -result;
}

int WDVD_StopMotor() {
	int result;

	if (di_fd<0)
		return -1;

	inbuffer[0] = 0xe3000000;
	inbuffer[1] = 0;
	inbuffer[2] = 0;

	result = IOS_Ioctl(di_fd, 0xe3, inbuffer, 0x20, 0, 0x20);

	return (result==1) ? 0 : -result;
}

int WDVD_LowOpenPartition(u64 offset) {
	int result;
	ioctlv iovector[0x08] ATTRIBUTE_ALIGN(32);
	u8 errorbuffer[0x40] ATTRIBUTE_ALIGN(32);
	u8 tmd_data[0x49e4] ATTRIBUTE_ALIGN(32);

	if (di_fd<0)
		return -1;

	WDVD_LowClosePartition();

	inbuffer[0] = 0x8b000000;
	inbuffer[1] = offset >> 2;
	inbuffer[2] = 0;
	inbuffer[3] = 0;

	iovector[0].data = inbuffer;
	iovector[0].len = 0x20;
	iovector[1].data = 0;
	iovector[1].len = 0x24a;
	iovector[2].data = 0;
	iovector[2].len = 0;
	iovector[3].data = tmd_data;
	iovector[3].len = 0x49e4;
	iovector[4].data = errorbuffer;
	iovector[4].len = 0x20;
	iovector[5].data = 0;
	iovector[5].len = 0;

	result = IOS_Ioctlv(di_fd, 0x8B, 3, 2, iovector);

	return (result==1) ? 0 : -result;
}
void WDVD_Close() {
	if (di_fd < 0)
		return;

	IOS_Close(di_fd);
	di_fd = -1;
}
int WDVD_VerifyCover(bool* cover) {
	*cover = false;
	if (di_fd<0)
		return -1;

	inbuffer[0] = 0xdb000000;
	if (IOS_Ioctl(di_fd, 0xdb, inbuffer, 0x20, inbuffer + 0x08, 0x20) != 1)
		return -1;

	*cover = !inbuffer[0x08];
	return 0;
}
