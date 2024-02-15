#include "etfile.h"

#include "core/logger.h"
#include "memory/etmemory.h"
#include "core/etstring.h"

#include <sys/stat.h>
#include <stdio.h>

//TODO: Platform specific code for filesytem
//TODO: Replace if checks with asserts to pass responsibility to the caller

struct etfile {
    char* path;
    FILE* handle;
};

static inline void _file_size(FILE* file, u64* out_size);

b8 file_exists(const char* path) {
#ifdef _MSC_VER
    struct _stat buffer;
    return _stat(path, &buffer) == 0;
#else
    struct stat buffer;
    return stat(path, &buffer) == 0;
#endif
}

b8 file_open(const char* path, etfile_flags flags, etfile** out_file) {
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

    *out_file = etallocate(sizeof(struct etfile), MEMORY_TAG_FILE);
    (*out_file)->handle = file;
    (*out_file)->path = str_duplicate_allocate(path);
    return true;
}

void file_close(etfile* file) {
    if (file && file->handle) {
        fclose(file->handle);
        str_duplicate_free(file->path);
        etfree(file, sizeof(struct etfile), MEMORY_TAG_FILE);
    }
}

// TODO: Check if works for not binary mode files
b8 file_size(etfile* file, u64* out_size) {
    if (file && file->handle) {
        _file_size(file->handle, out_size);
        return true;
    }
    return false;
}

b8 file_copy(etfile* file, etfile* copy) {
    u64 file_byte_count = 0;
    _file_size(file->handle, &file_byte_count);
    u8* file_bytes = etallocate(sizeof(u8) * file_byte_count, MEMORY_TAG_FILE);

    if (!file_read_bytes(file, file_bytes, file_byte_count)) {
        ETERROR("Error reading file contents.");
        etfree(file_bytes, sizeof(u8) * file_byte_count, MEMORY_TAG_FILE);
        return false;
    }

    u64 bytes_copied = 0;
    if (!file_write(copy, file_byte_count, file_bytes, &bytes_copied)) {
        ETERROR("Error copying file contents.");
        etfree(file_bytes, sizeof(u8) * file_byte_count, MEMORY_TAG_FILE);
        return false;
    }

    etfree(file_bytes, sizeof(u8) * file_byte_count, MEMORY_TAG_FILE);
    return true;
}

// TODO: Custom allocator for file reading
// TODO: Check if works for not binary mode files
b8 file_read_bytes(etfile* file, u8* out_bytes, u64 byte_count) {
    if (file && file->handle) {
        u64 bytes_read = fread(out_bytes, 1, byte_count, file->handle);
        return bytes_read == byte_count;
    }
    return false;
}

b8 file_write(etfile* file, u64 data_size, const void* data, u64* out_bytes_written) {
    if (file && file->handle) {
        *out_bytes_written = fwrite(data, 1, data_size, file->handle);
        if (*out_bytes_written != data_size) {
            return false;
        }
        fflush(file->handle);
        return true;
    }
    return false;
}

// Static helper method.
//NOTE: Sets the file position indicator to the beginning of the file
void _file_size(FILE* file, u64* out_size) {
    fseek(file, 0, SEEK_END);
    *out_size = ftell(file);
    rewind(file);
}