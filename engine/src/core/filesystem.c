#include "filesystem.h"

#include "logger.h"
#include "etmemory.h"

#include <sys/stat.h>
#include <stdio.h>

//TODO: Platform specific code for filesytem
//TODO: Replace checks with asserts or pass responsibility to the caller

static inline void _filesystem_size(FILE* file, u64* out_size);

b8 filesystem_exists(const char* path) {
#ifdef _MSC_VER
    struct _stat buffer;
    return _stat(path, &buffer) == 0;
#else
    struct stat buffer;
    return stat(path, &buffer) == 0;
#endif
}

b8 filesystem_open(const char* path, etfile_flags flags, etfile* out_file) {
    const char* mode_str;
    b8 is_read = flags & FILE_READ_FLAG;
    b8 is_write = flags & FILE_WRITE_FLAG;
    b8 is_binary = flags & FILE_BINARY_FLAG;
    
    if (is_read && is_write) mode_str = (is_binary) ? "w+b" : "w+";
    else if (is_read && !is_write) mode_str = (is_binary) ? "rb" : "r";
    else if (!is_read && is_write) mode_str = (is_binary) ? "wb" : "w";
    else {
        ETERROR("Invalid etfile_flags combination passed while trying to open file: '%s'", path);
        return false;
    }

    FILE* file = fopen(path, mode_str);
    if (!file) {
        ETERROR("Error opening File: '%s'.", path);
        return false;
    }

    out_file->handle = (void*)file;
    return true;
}

void filesystem_close(etfile* file) {
    if (file->handle) {
        fclose((FILE*)file->handle);
        file->handle = 0;
    }
}

// TODO: Check if works for not binary mode files
b8 filesystem_size(etfile* file, u64* out_size) {
    if (file->handle) {
        _filesystem_size((FILE*)file->handle, out_size);
        return true;
    }
    return false;
}

// TODO: Custom allocator for file reading
// TODO: Check if works for not binary mode files
b8 filesystem_read_all_bytes(etfile* file, u8* out_bytes, u64* out_bytes_read) {
    if (file->handle) {
        FILE* file_handle = (FILE*)file->handle;

        u64 size = 0;
        _filesystem_size(file->handle, &size);

        *out_bytes_read = fread(out_bytes, 1, size, file_handle);
        return *out_bytes_read == size;
    }
    return false;
}

b8 filesystem_write(etfile* file, u64 data_size, const void* data, u64* out_bytes_written) {
    if (file->handle) {
        FILE* file_handle = (FILE*)file->handle;
        *out_bytes_written = fwrite(data, 1, data_size, file_handle);
        if (*out_bytes_written != data_size) {
            return false;
        }
        fflush(file_handle);
        return true;
    }
    return false;
}

// Static helper method.
//NOTE: Sets the file position indicator to the beginning of the file
void _filesystem_size(FILE* file, u64* out_size) {
    fseek(file, 0, SEEK_END);
    *out_size = ftell(file);
    rewind(file);
}