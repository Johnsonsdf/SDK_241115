/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <kernel.h>
#include <zephyr/types.h>
#include <errno.h>
#include <init.h>
#include <fs/fs.h>
#include <fs/fs_sys.h>
#include <sys/__assert.h>
#include <ff.h>

/* Quick truncate: do not fill data */
#define FATFS_QUICK_TRUNCATE

#define FATFS_MAX_FILE_NAME 12 /* Uses 8.3 SFN */

/* Memory pool for FatFs directory objects */
K_MEM_SLAB_DEFINE(fatfs_dirp_pool, sizeof(DIR),
			CONFIG_FS_FATFS_NUM_DIRS, 4);

/* Memory pool for FatFs file objects */
K_MEM_SLAB_DEFINE(fatfs_filep_pool, sizeof(FIL),
			CONFIG_FS_FATFS_NUM_FILES, 4);

static int translate_error(int error)
{
	switch (error) {
	case FR_OK:
		return 0;
	case FR_NO_FILE:
	case FR_NO_PATH:
	case FR_INVALID_NAME:
		return -ENOENT;
	case FR_DENIED:
		return -EACCES;
	case FR_EXIST:
		return -EEXIST;
	case FR_INVALID_OBJECT:
		return -EBADF;
	case FR_WRITE_PROTECTED:
		return -EROFS;
	case FR_INVALID_DRIVE:
	case FR_NOT_ENABLED:
	case FR_NO_FILESYSTEM:
		return -ENODEV;
	case FR_NOT_ENOUGH_CORE:
		return -ENOMEM;
	case FR_TOO_MANY_OPEN_FILES:
		return -EMFILE;
	case FR_INVALID_PARAMETER:
		return -EINVAL;
	case FR_LOCKED:
	case FR_TIMEOUT:
	case FR_MKFS_ABORTED:
	case FR_DISK_ERR:
	case FR_INT_ERR:
	case FR_NOT_READY:
		return -EIO;
	}

	return -EIO;
}

static uint8_t translate_flags(fs_mode_t flags)
{
	uint8_t fat_mode = 0;

	fat_mode |= (flags & FS_O_READ) ? FA_READ : 0;
	fat_mode |= (flags & FS_O_WRITE) ? FA_WRITE : 0;
	fat_mode |= (flags & FS_O_CREATE) ? FA_OPEN_ALWAYS : 0;
	/* NOTE: FA_APPEND is not translated because FAT FS does not
	 * support append semantics of the Zephyr, where file position
	 * is forwarded to the end before each write, the fatfs_write
	 * will be tasked with setting a file position to the end,
	 * if FA_APPEND flag is present.
	 */

	return fat_mode;
}

static int fatfs_open(struct fs_file_t *zfp, const char *file_name,
		      fs_mode_t mode)
{
	FRESULT res;
	uint8_t fs_mode;
	void *ptr;

	if (k_mem_slab_alloc(&fatfs_filep_pool, &ptr, K_NO_WAIT) == 0) {
		(void)memset(ptr, 0, sizeof(FIL));
		zfp->filep = ptr;
	} else {
		return -ENOMEM;
	}

	fs_mode = translate_flags(mode);

	res = f_open(zfp->filep, &file_name[1], fs_mode);

	if (res != FR_OK) {
		k_mem_slab_free(&fatfs_filep_pool, &ptr);
		zfp->filep = NULL;
	}

	return translate_error(res);
}

static int fatfs_close(struct fs_file_t *zfp)
{
	FRESULT res;

	res = f_close(zfp->filep);

	/* Free file ptr memory */
	k_mem_slab_free(&fatfs_filep_pool, &zfp->filep);
	zfp->filep = NULL;

	return translate_error(res);
}

static int fatfs_unlink(struct fs_mount_t *mountp, const char *path)
{
	int res = -ENOTSUP;

#if !defined(CONFIG_FS_FATFS_READ_ONLY)
	res = f_unlink(&path[1]);

	res = translate_error(res);
#endif

	return res;
}

static int fatfs_rename(struct fs_mount_t *mountp, const char *from,
			const char *to)
{
	int res = -ENOTSUP;

#if !defined(CONFIG_FS_FATFS_READ_ONLY)
	FILINFO fno;

	/* Check if 'to' path exists; remove it if it does */
	res = f_stat(&to[1], &fno);
	if (FR_OK == res) {
		res = f_unlink(&to[1]);
		if (FR_OK != res)
			return translate_error(res);
	}

	res = f_rename(&from[1], &to[1]);
	res = translate_error(res);
#endif

	return res;
}

static ssize_t fatfs_read(struct fs_file_t *zfp, void *ptr, size_t size)
{
	FRESULT res;
	unsigned int br;

	res = f_read(zfp->filep, ptr, size, &br);
	if (res != FR_OK) {
		return translate_error(res);
	}

	return br;
}

static ssize_t fatfs_write(struct fs_file_t *zfp, const void *ptr, size_t size)
{
	int res = -ENOTSUP;

#if !defined(CONFIG_FS_FATFS_READ_ONLY)
	unsigned int bw;
	off_t pos = f_size((FIL *)zfp->filep);
	res = FR_OK;

	/* FA_APPEND flag means that file has been opened for append.
	 * The FAT FS write does not support the POSIX append semantics,
	 * to always write at the end of file, so set file position
	 * at the end before each write if FA_APPEND is set.
	 */
	if (zfp->flags & FS_O_APPEND) {
		res = f_lseek(zfp->filep, pos);
	}

	if (res == FR_OK) {
		res = f_write(zfp->filep, ptr, size, &bw);
	}

	if (res != FR_OK) {
		res = translate_error(res);
	} else {
		res = bw;
	}
#endif

	return res;
}

static int fatfs_seek(struct fs_file_t *zfp, off_t offset, int whence)
{
	FRESULT res = FR_OK;
	off_t pos;

	switch (whence) {
	case FS_SEEK_SET:
		pos = offset;
		break;
	case FS_SEEK_CUR:
		pos = f_tell((FIL *)zfp->filep) + offset;
		break;
	case FS_SEEK_END:
		pos = f_size((FIL *)zfp->filep) + offset;
		break;
	default:
		return -EINVAL;
	}

	if ((pos < 0) || (pos > f_size((FIL *)zfp->filep))) {
		return -EINVAL;
	}

	res = f_lseek(zfp->filep, pos);

	return translate_error(res);
}

static off_t fatfs_tell(struct fs_file_t *zfp)
{
	return f_tell((FIL *)zfp->filep);
}

static int fatfs_truncate(struct fs_file_t *zfp, off_t length)
{
	int res = -ENOTSUP;

#if !defined(CONFIG_FS_FATFS_READ_ONLY)
	off_t cur_length = f_size((FIL *)zfp->filep);

#ifdef FATFS_QUICK_TRUNCATE
	/* Optimize: No need to do any operation */
	if (cur_length == length) {
		return translate_error(res);
	}
#endif

	/* f_lseek expands file if new position is larger than file size */
	res = f_lseek(zfp->filep, length);
	if (res != FR_OK) {
		return translate_error(res);
	}

	if (length < cur_length) {
		res = f_truncate(zfp->filep);
	} else {
#ifdef FATFS_QUICK_TRUNCATE
		/*
		 * Optimize: Do sync instead of filling the whole
		 * expanded region with zeroes.
		 */
		 res = f_sync(zfp->filep);
#else
		/*
		 * Get actual length after expansion. This could be
		 * less if there was not enough space in the volume
		 * to expand to the requested length
		 */
		length = f_tell((FIL *)zfp->filep);

		res = f_lseek(zfp->filep, cur_length);
		if (res != FR_OK) {
			return translate_error(res);
		}

		/*
		 * The FS module does caching and optimization of
		 * writes. Here we write 1 byte at a time to avoid
		 * using additional code and memory for doing any
		 * optimization.
		 */
		unsigned int bw;
		uint8_t c = 0U;

		for (int i = cur_length; i < length; i++) {
			res = f_write(zfp->filep, &c, 1, &bw);
			if (res != FR_OK) {
				break;
			}
		}
#endif
	}

	res = translate_error(res);
#endif

	return res;
}

static int fatfs_sync(struct fs_file_t *zfp)
{
	int res = -ENOTSUP;

#if !defined(CONFIG_FS_FATFS_READ_ONLY)
	res = f_sync(zfp->filep);
	res = translate_error(res);
#endif
	return res;
}

static int fatfs_mkdir(struct fs_mount_t *mountp, const char *path)
{
	int res = -ENOTSUP;

#if !defined(CONFIG_FS_FATFS_READ_ONLY)
	res = f_mkdir(&path[1]);
	res = translate_error(res);
#endif

	return res;
}

static int fatfs_opendir(struct fs_dir_t *zdp, const char *path)
{
	FRESULT res;
	void *ptr;

	if (k_mem_slab_alloc(&fatfs_dirp_pool, &ptr, K_NO_WAIT) == 0) {
		(void)memset(ptr, 0, sizeof(DIR));
		zdp->dirp = ptr;
	} else {
		return -ENOMEM;
	}

	res = f_opendir(zdp->dirp, &path[1]);

	if (res != FR_OK) {
		k_mem_slab_free(&fatfs_dirp_pool, &ptr);
		zdp->dirp = NULL;
	}

	return translate_error(res);
}

static int fatfs_readdir(struct fs_dir_t *zdp, struct fs_dirent *entry)
{
	FRESULT res;
	FILINFO fno;

	res = f_readdir(zdp->dirp, &fno);
	if (res == FR_OK) {
		strcpy(entry->name, fno.fname);
		if (entry->name[0] != 0) {
			entry->type = ((fno.fattrib & AM_DIR) ?
			       FS_DIR_ENTRY_DIR : FS_DIR_ENTRY_FILE);
			entry->size = fno.fsize;
		}
	}

	return translate_error(res);
}

static int fatfs_closedir(struct fs_dir_t *zdp)
{
	FRESULT res;

	res = f_closedir(zdp->dirp);

	/* Free file ptr memory */
	k_mem_slab_free(&fatfs_dirp_pool, &zdp->dirp);

	return translate_error(res);
}

static int fatfs_stat(struct fs_mount_t *mountp,
		      const char *path, struct fs_dirent *entry)
{
	FRESULT res;
	FILINFO fno;

	res = f_stat(&path[1], &fno);
	if (res == FR_OK) {
		entry->type = ((fno.fattrib & AM_DIR) ?
			       FS_DIR_ENTRY_DIR : FS_DIR_ENTRY_FILE);
		strcpy(entry->name, fno.fname);
		entry->size = fno.fsize;
	}

	return translate_error(res);
}

static int fatfs_statvfs(struct fs_mount_t *mountp,
			 const char *path, struct fs_statvfs *stat)
{
	int res = -ENOTSUP;
#if !defined(CONFIG_FS_FATFS_READ_ONLY)
	FATFS *fs;

	res = f_getfree(&mountp->mnt_point[1], &stat->f_bfree, &fs);
	if (res != FR_OK) {
		return -EIO;
	}

	/*
	 * _MIN_SS holds the sector size. It is one of the configuration
	 * constants used by the FS module
	 */
	stat->f_bsize = _MIN_SS;
	stat->f_frsize = fs->csize * stat->f_bsize;
	stat->f_blocks = (fs->n_fatent - 2);

	res = translate_error(res);
#endif
	return res;
}

static int fatfs_mount(struct fs_mount_t *mountp)
{
	FRESULT res;

	res = f_mount((FATFS *)mountp->fs_data, &mountp->mnt_point[1], 1);

#if defined(CONFIG_FS_FATFS_MOUNT_MKFS)
	if (res == FR_NO_FILESYSTEM &&
	    (mountp->flags & FS_MOUNT_FLAG_READ_ONLY) != 0) {
		return -EROFS;
	}
	/* If no file system found then create one */
	if (res == FR_NO_FILESYSTEM &&
	    (mountp->flags & FS_MOUNT_FLAG_NO_FORMAT) == 0) {
		uint8_t work[_MAX_SS];

		res = f_mkfs(&mountp->mnt_point[1],
				(FM_FAT | FM_SFD), 0, work, sizeof(work));
		if (res == FR_OK) {
			res = f_mount((FATFS *)mountp->fs_data,
					&mountp->mnt_point[1], 1);
		}
	}
#endif /* CONFIG_FS_FATFS_MOUNT_MKFS */

	return translate_error(res);

}

static int fatfs_unmount(struct fs_mount_t *mountp)
{
	FRESULT res;

	res = f_mount(NULL, &mountp->mnt_point[1], 0);

	return translate_error(res);
}
/* detect disk add or remove */
static int fatfs_disk_detect(struct fs_mount_t *mountp, const char *path, uint8_t *state)
{
	return f_disk_detect(path + 1, state);/*filter'/'*/
}

static int fatfs_open_cluster(struct fs_file_t *zfp, char *dir, uint32_t cluster, uint32_t blk_ofs)
{
	FRESULT res;
	uint8_t fs_mode;
	void *ptr;

	if (k_mem_slab_alloc(&fatfs_filep_pool, &ptr, K_NO_WAIT) == 0) {
		(void)memset(ptr, 0, sizeof(FIL));
		zfp->filep = ptr;
	} else {
		return -ENOMEM;
	}

	fs_mode = FA_READ | FA_WRITE | FA_OPEN_ALWAYS;

	res = f_open_cluster(zfp->filep, dir + 1, cluster, blk_ofs, fs_mode);/*filter'/'*/

	return translate_error(res);
}

static int fatfs_opendir_cluster(struct fs_dir_t *zdp, const char *path, unsigned int cluster, unsigned int blk_ofs)
{
	FRESULT res;
	void *ptr;

	if (k_mem_slab_alloc(&fatfs_dirp_pool, &ptr, K_NO_WAIT) == 0) {
		(void)memset(ptr, 0, sizeof(DIR));
		zdp->dirp = ptr;
	} else {
		return -ENOMEM;
	}

	res = f_opendir_cluster(zdp->dirp, path + 1, cluster, blk_ofs);/*filter'/'*/

	return translate_error(res);
}

/* File system interface */
static const struct fs_file_system_t fatfs_fs = {
	.open = fatfs_open,
	.close = fatfs_close,
	.read = fatfs_read,
	.write = fatfs_write,
	.lseek = fatfs_seek,
	.tell = fatfs_tell,
	.truncate = fatfs_truncate,
	.sync = fatfs_sync,
	.opendir = fatfs_opendir,
	.readdir = fatfs_readdir,
	.closedir = fatfs_closedir,
	.mount = fatfs_mount,
	.unmount = fatfs_unmount,
	.unlink = fatfs_unlink,
	.rename = fatfs_rename,
	.mkdir = fatfs_mkdir,
	.stat = fatfs_stat,
	.statvfs = fatfs_statvfs,
	.disk_detect = fatfs_disk_detect,
	.open_cluster = fatfs_open_cluster,
	.opendir_cluster = fatfs_opendir_cluster,
};

static int fatfs_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	return fs_register(FS_FATFS, &fatfs_fs);
}

SYS_INIT(fatfs_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
