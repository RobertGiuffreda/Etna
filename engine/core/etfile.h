#pragma once

#include "defines.h"

// TODO: Use asserts to check for issues & use fprintf to write in some cases

typedef enum etfile_flag_bits {
    FILE_READ_FLAG = 0x1,       // Specifies read access for file
    FILE_WRITE_FLAG = 0x2,      // Specifies write access for file
    FILE_BINARY_FLAG = 0x4,     // Specifies the file is binary
} etfile_flag_bits;
typedef u32 etfile_flags;

typedef struct etfile etfile;

b8 file_exists(const char* path);

b8 file_open(const char* path, etfile_flags flags, etfile** out_file);

void file_close(etfile* file);

b8 file_size(etfile* file, u64* out_size);

b8 file_copy(etfile* file, etfile* copy);

b8 file_read_bytes(etfile* file, u8* out_bytes, u64 byte_count);

b8 file_write(etfile* file, u64 data_size, const void* data, u64* out_bytes_written);