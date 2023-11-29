#include "dynarray.h"
#include "core/logger.h"
#include "core/asserts.h"
#include "core/etmemory.h"

// TODO: IMPLEMENT SAFETY CHECKS FOR THE ARRAY:
// Out of bounds reading, impossible values, etc...
// Code assumes all indices passed to remove from and insert to
// are within the bounds of the array. This may not be the case
// Debug asserts?? Fatal Errors?? Regular Errors??

#define DYNARRAY_RESIZE_FACTOR 2
#define RESIZE_CAPACITY(curr_capacity) (curr_capacity + 1) * DYNARRAY_RESIZE_FACTOR

// TODO: Change index & length names to reflect role better.
// length --> capacity
// index --> end_index || index --> length
typedef struct dynarray {
    u64 capacity;
    u64 length;
    u64 stride;
} dynarray;

static const u64 header_size = sizeof(dynarray);

static inline dynarray* _dynarray_create(u64 capacity, u64 stride);
static inline dynarray* _dynarray_resize(dynarray* header, u64 capacity);

// TODO: Test and ensure correct behavoir
static inline dynarray* _dynarray_resize_index(dynarray* header, u64 length, u64 index);
static inline void _dynarray_shift_index(dynarray* header, u64 index);

void* dynarray_create(u64 length, u64 stride)
{
    dynarray* header = _dynarray_create(length, stride);
    header->length = 0;
    header->capacity = length;
    header->stride = stride;

    // Move forward by a value of 1 * sizeof(dynarray).
    return (void *)(header + 1);
}

void dynarray_destroy(void* array)
{
    if (array)
    {
        dynarray* to_free = (dynarray*)array - 1;
        etfree(to_free, header_size + to_free->capacity * to_free->stride, MEMORY_TAG_DYNARRAY);
    }
}

// TODO: Warn the user that adding to array they resized will cause
// a resize on the next push unless index is moved
void dynarray_resize(void** array_ptr, u64 length)
{
    dynarray* header = (dynarray*)(*array_ptr) - 1;
    header = _dynarray_resize(header, length);
    if (header->length >= header->capacity) 
    {
        // If the index of the array is larger than the length
        header->length = header->capacity;
    }
    *array_ptr = (void*)(header + 1);
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

u64 dynarray_length(void* array)
{
    return ((dynarray*)array - 1)->length;
}

//TODO: Decide behavoir when index == header->index
void dynarray_insert(void** array_ptr, void* element, u64 index)
{
    dynarray* header = (dynarray*)(*array_ptr) - 1;
    if (index <= header->length) {
        // Resize array. Below resize function shifts the memory 
        // to the right in anticipation of the insertion at array[index].
        if (header->length >= header->capacity) // header->length should never be larger but just in case
        {
            header = _dynarray_resize_index(header, RESIZE_CAPACITY(header->capacity), index);
            (*array_ptr) = (void*)(header + 1);
        }
        // The insertion index is not the end of the array
        // shift array elements to the right starting at index
        else if (index != header->length)
        {
            _dynarray_shift_index(header, index);
        }
        // Copy the element into array[index]
        etcopy_memory((u8*)(*array_ptr) + index * header->stride, element, header->stride);

        // Increment ending index as an element has been added
        header->length++;
    } else {
        ETERROR("Index not in bounds of dynamic array.");
    }
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

// static inline here????
dynarray* _dynarray_create(u64 capacity, u64 stride)
{
   return (dynarray*)etallocate(header_size + (capacity * stride), MEMORY_TAG_DYNARRAY);
}

// static inline here????
dynarray* _dynarray_resize(dynarray* header, u64 capacity)
{
    dynarray* resized_array = _dynarray_create(capacity, header->stride);

    // Copy data from previous array to new array except:
    etcopy_memory(
        resized_array,
        header,
        header_size + header->capacity * header->stride
    );

    // Set new array length as the old one was copied from
    // the previous array header
    resized_array->capacity = capacity;

    // Free old memory staring from header of dynarray
    etfree(header, header_size + header->capacity * header->stride, MEMORY_TAG_DYNARRAY);

    return resized_array;
}

// As resize is occuring we do not have to check if shifting data to the right
// one block is going to access memory not in the array.
dynarray* _dynarray_resize_index(dynarray* header, u64 capacity, u64 index)
{
    dynarray* resized_array = _dynarray_create(capacity, header->stride);

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

    etfree(header, header_size + header->capacity * header->stride, MEMORY_TAG_DYNARRAY);
    return resized_array;
}

// Shifts array entries to the right
// Assumes header->index != index
void _dynarray_shift_index(dynarray* header, u64 index)
{
    u8* index_address = (u8*)(header + 1) + header->stride * index;
    etmove_memory(
        index_address + header->stride,
        index_address,
        (header->length - index) * header->stride
    );
}