#pragma once

#include "defines.h"
#include "memory/etmemory.h"

#define DYNARRAY_DEFAULT_CAPACITY 1

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
// TODO: Define dynarray in this header file. 
#define DYN_HEAD(dyn) ((dynarray*)dyn - 1);

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

void dynarray_push(void** array_ptr, const void* element);
void dynarray_pop(void* array, void* dest);

void dynarray_insert(void** array_ptr, void* element, u64 index);
void dynarray_remove(void* array, void* dest, u64 index);

void dynarray_clear(void* array);

u64 dynarray_length(void* array);
b8 dynarray_is_empty(void* array);
