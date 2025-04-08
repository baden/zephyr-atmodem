#pragma once

#include <zephyr/kernel.h>
#include "../simcom-a7682e.h"

int simcom_files_meminfo(const struct device *dev);
int simcom_files_cd(const struct device *dev, const char *path);
enum {
    SIMCOM_FILES_LS_BOTH = 0,
    SIMCOM_FILES_LS_DIRS = 1,
    SIMCOM_FILES_LS_FILES = 2,
};
int simcom_files_ls(const struct device *dev, int type);
int simcom_files_mkdir(const struct device *dev, const char *dir);
int simcom_files_delete(const struct device *dev, const char *filename);

enum simcom_fs_move_direction {
    SIMCOM_FS_MOVE_TO_ROOT = 0,
    SIMCOM_FS_MOVE_TO_USER = 1,
};
int simcom_fs_moveto(const struct device *dev, const char *filename, enum simcom_fs_move_direction direction);

#if 0
enum simcom_fs_open_mode {
    SIMCOM_FS_OPEN_CREATE_IF_NOT_EXIST_RW = 0,
    SIMCOM_FS_OPEN_CREATE_OR_OVERWRITE_RW = 1,
    SIMCOM_FS_OPEN_IF_EXIST_ONLY = 2,
};

int simcom_fs_open(const struct device *dev, const char *filename, int mode);
#endif

int simcom_fs_transfer(const struct device *dev,
    const char *filepath, int location, int size,
    char *buf, size_t buf_len, size_t *bytes_read);
