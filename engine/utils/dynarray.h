#pragma once
#include "defines.h"
#include "memory/etmemory.h"
#include "math/math_types.h"


/** NOTE: Dynarray impl notes
 * This is a dynamic array imlementation that stores the details needed in a
 * struct placed before the returned value to the caller. So the caller can call 
 * functions on the array and access the data like a normal array.
 * 
 * NOTE: If a function takes array_ptr** as an argument 
 * dynarray pointer needs to be cast to (void**) for the compiler to not complain
 * 
 * An issue with this implementation is that when the array is resized, any outside 
 * references to the array are invalid as they are pointing to the old location of
 * the array and not the newly allocated location.
 * 
 * If this becomes an issue: Create implementation of something like a shared pointer.
 */

#define DYNARRAY_RESIZE_FACTOR 2
#define RESIZE_CAPACITY(curr_capacity) ((curr_capacity + 1) * DYNARRAY_RESIZE_FACTOR)

// Hidden alloc for dynamic array
// 32 Bytes so that v4s & m4s alignment is not disrupted by hidden alloc
typedef struct dynarray {
    u64 capacity;
    u64 length;
    u64 stride;
    memory_tag tag;
} dynarray;

void* dynarray_create(u64 capacity, u64 stride);
void* dynarray_create_data(u64 capacity, u64 stride, u64 length, const void* data);

void* dynarray_create_tagged(u64 capacity, u64 stride, memory_tag tag);
void* dynarray_create_data_tagged(u64 capacity, u64 stride, u64 length, memory_tag tag, const void* data);

void* dynarray_copy(void* src);

void dynarray_destroy(void* array);

// Sets the length variable
void dynarray_resize(void** array_ptr, u64 length);
// Does not set the length variable
void dynarray_reserve(void** array_ptr, u64 capacity);

// NOTE: Assumes the byte count of the array is a multiple of the new types size. 
// Otherwise memory tracking on dynarray_destroy would be innacurate
// NOTE: Be careful of reinterpreting to types with required alignment greater than 16 bytes
#define _DYNARRAY_REINTERPRET_TYPE_AS_TYPE_FUNC(old_type, new_type)                       \
static inline new_type* dynarray_reinterpret_##old_type##_to_##new_type(old_type* array) {          \
    dynarray* header = (dynarray*)array - 1;                                                        \
    dynarray reinterpret_header = {                                                                 \
        .capacity = (header->capacity * sizeof(old_type)) / sizeof(new_type),                       \
        .length = (header->length * sizeof(old_type)) / sizeof(new_type),                           \
        .stride = sizeof(new_type),                                                                 \
        .tag = header->tag,                                                                         \
    };                                                                                              \
    *header = reinterpret_header;                                                                   \
    return (new_type*)(header + 1);                                                                 \
}

#define DEFINE_DYNARRAY_REINTERPRET_TYPE_AS_TYPE_FUNCTIONS(type1, type2)    \
_DYNARRAY_REINTERPRET_TYPE_AS_TYPE_FUNC(type1, type2)                       \
_DYNARRAY_REINTERPRET_TYPE_AS_TYPE_FUNC(type2, type1)                       \

DEFINE_DYNARRAY_REINTERPRET_TYPE_AS_TYPE_FUNCTIONS(f32, v4s)
DEFINE_DYNARRAY_REINTERPRET_TYPE_AS_TYPE_FUNCTIONS(f32, v3s)
DEFINE_DYNARRAY_REINTERPRET_TYPE_AS_TYPE_FUNCTIONS(f32, v2s)

#define _DYANRRAY_COPY_TYPE_AS_TYPE_FUNC(old_type, new_type)                                                    \
static inline new_type* dynarray_copy_##old_type##_as_##new_type(old_type* array) {                             \
    dynarray* header = (dynarray*)array - 1;                                                                    \
    dynarray reinterpret_header = {                                                                             \
        .capacity = (header->capacity * sizeof(old_type)) / sizeof(new_type),                                   \
        .length = (header->length * sizeof(old_type)) / sizeof(new_type),                                       \
        .stride = sizeof(new_type),                                                                             \
        .tag = header->tag,                                                                                     \
    };                                                                                                          \
    u64 data_copy_size = reinterpret_header.capacity * sizeof(new_type);                                        \
    dynarray* copy_header = etallocate(data_copy_size + sizeof(dynarray), header->tag);                         \
    etcopy_memory(copy_header, &reinterpret_header, sizeof(dynarray));                                          \
    etcopy_memory(copy_header + 1, array, data_copy_size);                                                      \
    return (new_type*)(copy_header + 1);                                                                        \
}

#define DEFINE_DYNARRAY_COPY_TYPE_AS_TYPE_FUNCTIONS(type1, type2)   \
_DYANRRAY_COPY_TYPE_AS_TYPE_FUNC(type1, type2)                      \
_DYANRRAY_COPY_TYPE_AS_TYPE_FUNC(type2, type1)                      \

DEFINE_DYNARRAY_COPY_TYPE_AS_TYPE_FUNCTIONS(f32, v4s)
DEFINE_DYNARRAY_COPY_TYPE_AS_TYPE_FUNCTIONS(f32, v3s)
DEFINE_DYNARRAY_COPY_TYPE_AS_TYPE_FUNCTIONS(f32, v2s)

// returns the length before growing
static inline u64 dynarray_grow(void** array_ptr, u64 count) {
    u64 array_length = ((dynarray*)(*array_ptr) - 1)->length;
    dynarray_resize((void**)array_ptr, array_length + count);
    return array_length;
}

// TODO: Rename dynarray_concat
#define _DYNARRAY_APPEND_TYPED_FUNC(type)                                               \
static inline void dynarray_append_##type(type** dst_ptr, type* src) {                  \
    u64 src_length = ((dynarray*)src - 1)->length;                                      \
    u64 dst_length = ((dynarray*)(*dst_ptr) - 1)->length;                               \
    dynarray_resize((void**)dst_ptr, dst_length + src_length);                          \
    etcopy_memory((*dst_ptr) + dst_length, src, src_length * sizeof(type));             \
}

_DYNARRAY_APPEND_TYPED_FUNC(vertex)
_DYNARRAY_APPEND_TYPED_FUNC(u32)

void dynarray_push(void** array_ptr, const void* element);
void dynarray_pop(void* array, void* dest);

void dynarray_insert(void** array_ptr, void* element, u64 index);
void dynarray_remove(void* array, void* dest, u64 index);

void dynarray_clear(void* array);

u64 dynarray_length(void* array);
b8 dynarray_is_empty(void* array);
