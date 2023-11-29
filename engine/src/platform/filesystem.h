#pragma once

#include "defines.h"

typedef enum etfile_flag_bits {
    FILE_READ_FLAG = 0x1,       // Specifies read access for file
    FILE_WRITE_FLAG = 0x2,      // Specifies write access for file
    FILE_BINARY_FLAG = 0x4,     // Specifies the file is binary
} etfile_flag_bits;

/**
 * @brief Combination of etfile_flag_bits.
 * 
 */
typedef u32 etfile_flags;

typedef struct etfile {
    void* handle;
} etfile;

/**
 * @brief Determines if the file exists
 * 
 * @param path The path string to the file
 * @return true if exists, false otherwise
 */
b8 filesystem_exists(const char* path);

/**
 * @brief Attempt to open file located at path.
 * 
 * @param path The path of the file to be opened.
 * @param modes A combination of etfile_mode_bits, specifying the modes to open the file in. (Read/Write)
 * @param out_file A pointer to an etfile struct, which holds the handle information. 
 * @return true if successful, false otherwise
 */
b8 filesystem_open(const char* path, etfile_flags flags, etfile* out_file);

/**
 * @brief Close the file using the handle in the file struct.
 * 
 * @param file A pointer to a file struct containing an opaque handle to the file to close.
 */
void filesystem_close(etfile* file);

/**
 * @brief Attempts to read the size of the file to which handle is attached.
 * 
 * @param file File to determine the size of
 * @param out_size A pointer to hold the file size.
 */
b8 filesystem_size(etfile* file, u64* out_size);

/**
 * @brief Reads all bytes from the provided file to here
 * 
 * @param file Pointer to the file to read from.
 * @param out_bytes A pointer to a block of memory to be populated by this function.
 * @param out_bytes_read A pointer to a number which will be populated with the number of bytes read from the file.
 * @return True if sucessful, false otherwise.
 */
b8 filesystem_read_all_bytes(etfile* file, u8* out_bytes, u64* out_bytes_read);

/** 
 * @brief Writes provided data to the file.
 * 
 * @param handle A pointer to a file_handle structure.
 * @param data_size The size of the data in bytes.
 * @param data The data to be written.
 * @param out_bytes_written A pointer to a number which will be populated with the number of bytes actually written to the file.
 * @returns True if successful, false otherwise.
 */
b8 filesystem_write(etfile* file, u64 data_size, const void* data, u64* out_bytes_written);