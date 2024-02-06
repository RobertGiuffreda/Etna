#include "dynarray.h"
#include "core/logger.h"
#include "core/asserts.h"
#include "memory/etmemory.h"

// TODO: IMPLEMENT SAFETY CHECKS FOR THE ARRAY:
// Out of bounds reading, impossible values, etc...
// Code assumes all indices passed to remove from and insert to
// are within the bounds of the array. This may not be the case
// Debug asserts?? Fatal Errors?? Regular Errors??

#define DYNARRAY_RESIZE_FACTOR 2
#define RESIZE_CAPACITY(curr_capacity) ((curr_capacity + 1) * DYNARRAY_RESIZE_FACTOR)

typedef struct dynarray {
    u64 capacity;
    u64 length;
    u64 stride;
    memory_tag tag;
} dynarray;

static const u64 header_size = sizeof(dynarray);

static inline dynarray* _dynarray_create(u64 capacity, u64 stride, memory_tag tag);
static inline dynarray* _dynarray_create_data(u64 capacity, u64 stride, u64 length, memory_tag tag, const void* data);
static inline dynarray* _dynarray_resize(dynarray* header, u64 capacity);

static inline dynarray* _dynarray_resize_index(dynarray* header, u64 length, u64 index);
static inline void _dynarray_shift_index(dynarray* header, u64 index);

void* dynarray_create(u64 capacity, u64 stride)
{
    dynarray* header = _dynarray_create(capacity, stride, MEMORY_TAG_DYNARRAY);
    header->length = 0;
    header->capacity = capacity;
    header->stride = stride;
    header->tag = MEMORY_TAG_DYNARRAY;

    // Move forward by a value of 1 * sizeof(dynarray).
    return (void*)(header + 1);
}

void* dynarray_create_tagged(u64 capacity, u64 stride, memory_tag tag) {
    dynarray* header = _dynarray_create(capacity, stride, tag);
    header->length = 0;
    header->capacity = capacity;
    header->stride = stride;
    header->tag = tag;

    // Move forward by a value of 1 * sizeof(dynarray).
    return (void*)(header + 1);
}

void* dynarray_create_data(u64 capacity, u64 stride, u64 length, const void* data) {
    // If the given capacity is too small to hold all the data. Cut it off
    u64 new_length = (capacity < length) ? capacity : length;
    dynarray* header = _dynarray_create_data(capacity, stride, new_length, MEMORY_TAG_DYNARRAY, data);
    header->capacity = capacity;
    header->length = new_length;
    header->stride = stride;
    header->tag = MEMORY_TAG_DYNARRAY;
    return (void*)(header + 1);
}

void* dynarray_create_data_tagged(u64 capacity, u64 stride, u64 length, memory_tag tag, const void* data) {
    // If the given capacity is too small to hold all the data. Cut it off
    u64 new_length = (capacity < length) ? capacity : length;
    dynarray* header = _dynarray_create_data(capacity, stride, new_length, tag, data);
    header->capacity = capacity;
    header->length = new_length;
    header->stride = stride;
    header->tag = tag;
    return (void*)(header + 1);
}

void* dynarray_copy(void* src) {
    dynarray* source = (dynarray*)src - 1;
    dynarray* dest = _dynarray_create(source->capacity, source->stride, source->tag);
    etcopy_memory(dest, source, (source->length * source->stride) + header_size);
    return (void*)(dest + 1);
}

void dynarray_destroy(void* array)
{
    dynarray* to_free = (dynarray*)array - 1;
    etfree(to_free, header_size + to_free->capacity * to_free->stride, to_free->tag);
}

// TODO: Take a value to copy into the array for uninitialized memory??
void dynarray_resize(void** array_ptr, u64 length) {
    dynarray* header = (dynarray*)(*array_ptr) - 1;
    if (length > header->capacity) {
        header = _dynarray_resize(header, length);
    }
    header->length = length;
    *array_ptr = (void*)(header + 1);
}

void dynarray_reserve(void** array_ptr, u64 capacity) {
    dynarray* header = (dynarray*)(*array_ptr) - 1;
    if (capacity > header->capacity) {
        header = _dynarray_resize(header, capacity);
    }
    *array_ptr = (void*)(header + 1);
}

void dynarray_length_set(void* array, u64 length) {
    ((dynarray*)array - 1)->length = length;
}

void dynarray_push(void** array_ptr, const void* element)
{
    dynarray* header = (dynarray*)(*array_ptr) - 1;
    if (header->length >= header->capacity)
    {
        // Resize the dynarray: Address has changed so:
        header = _dynarray_resize(header, RESIZE_CAPACITY(header->capacity));
        // Update caller's pointer to array.
        *array_ptr = (void*)(header + 1);
    }

    // Adding the element to the array in a type agnostic way
    etcopy_memory((u8*)(*array_ptr) + header->length * header->stride, element, header->stride);
    header->length++;
}

// Will throw fatal error if 
void dynarray_pop(void* array, void* dest)
{
    dynarray* header = (dynarray*)array - 1;
    ETASSERT_MESSAGE(header->length > 0, "Tried to pop from an empty (index is zero) dynarray.")
    etcopy_memory(dest, (u8*)array + (--header->length) * header->stride, header->stride);
}

b8 dynarray_is_empty(void* array)
{
    dynarray* header = ((dynarray*)array - 1);
    return header->length == 0;
}

u64 dynarray_length(void* array) {
    return ((dynarray*)array - 1)->length;
}

void dynarray_clear(void* array) {
    ((dynarray*)array - 1)->length = 0;
}

//NOTE: this function cannot insert at the end of an array.
void dynarray_insert(void** array_ptr, void* element, u64 index)
{
    dynarray* header = (dynarray*)(*array_ptr) - 1;
    if (index >= header->length) {
        ETERROR("Index not in bounds of dynamic array.");
        return;
    }
    if (header->length == header->capacity) {
        // _dynarray_resize_index also shifts the memory to make room for
        //  the new element when copying for the resize
        header = _dynarray_resize_index(header, RESIZE_CAPACITY(header->capacity), index);
        (*array_ptr) = (void*)(header + 1);
    } else {
        // Shifts the memory to make room for the new element
        _dynarray_shift_index(header, index);
    }
    etcopy_memory((u8*)(*array_ptr) + index * header->stride, element, header->stride);
    header->length++;
}

void dynarray_remove(void* array, void* dest, u64 index)
{
    dynarray* header = (dynarray*)array - 1;
    u64 stride = header->stride;
    u64 end_index = header->length;
    u8* index_addr = (u8*)array + index * stride;

    etcopy_memory(dest, index_addr, stride);

    // Shift memory after the index to the left
    // |1|2|3|4|5| -> |1|3|4|5|5|
    if (index != end_index - 1)
    {
        etmove_memory(
            index_addr,
            index_addr + stride,
            ((end_index - 1) - index) * stride
        );
    }

    // Decrement the index to the end
    header->length--;
}

static inline dynarray* _dynarray_create(u64 capacity, u64 stride, memory_tag tag) {
   return (dynarray*)etallocate(header_size + (capacity * stride), tag);
}

static inline dynarray* _dynarray_create_data(u64 capacity, u64 stride, u64 length, memory_tag tag, const void* data) {
    dynarray* new_dynarray = (dynarray*)etallocate(header_size + (capacity * stride), tag);
    etcopy_memory((new_dynarray + 1), data, stride * length);
    return new_dynarray;
}

static inline dynarray* _dynarray_resize(dynarray* header, u64 capacity)
{
    dynarray* resized_array = _dynarray_create(capacity, header->stride, header->tag);

    // Copy data from previous array to new array except:
    etcopy_memory(
        resized_array,
        header,
        header_size + header->capacity * header->stride
    );

    // Set new array capacity as the old one was copied from
    // the previous array header
    resized_array->capacity = capacity;

    // Free old memory staring from header of dynarray
    etfree(header, header_size + header->capacity * header->stride, header->tag);
    return resized_array;
}

static inline dynarray* _dynarray_resize_index(dynarray* header, u64 capacity, u64 index)
{
    dynarray* resized_array = _dynarray_create(capacity, header->stride, header->tag);

    // Copy memory up to the specified index for insertion
    etcopy_memory(
        resized_array,
        header,
        header_size + (index - 1) * header->stride
    );

    // Copy memory after the specified index for insertion
    etcopy_memory(
        (u8*)resized_array + header_size + (index + 1) * header->stride,
        (u8*)header + header_size + index * header->stride,
        (header->length - index) * header->stride
    );

    resized_array->capacity = capacity;

    // Free old memory staring from header of dynarray
    etfree(header, header_size + header->capacity * header->stride, header->tag);
    return resized_array;
}

// Shifts array entries to the right
// Assumes header->index != index
static inline void _dynarray_shift_index(dynarray* header, u64 index)
{
    u8* index_address = (u8*)(header + 1) + header->stride * index;
    etmove_memory(
        index_address + header->stride,
        index_address,
        (header->length - index) * header->stride
    );
}