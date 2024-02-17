#pragma once

#include "defines.h"
#include "memory/etmemory.h"

#define DYNARRAY_DEFAULT_CAPACITY 1

// TODO: Trim function

/*
This is a dynamic array imlementation that stores the details needed in a
struct placed before the returned value to the caller. So the caller can call 
functions on the array and access the data like a normal array.

An issue with this implementation is that when the array is resized, any outside 
references to the array are invalid as they are pointing to the old location of
the array and not the newly allocated location.

If this becomes an issue: Create implementation of something like a shared pointer.
*/

/* 
 * Creates a dynamic array of (length) elements with a size of
 * (stride) each.
 * The end index used for pushing and popping is set to length
 */
void* dynarray_create(u64 capacity, u64 stride);

void* dynarray_create_tagged(u64 capacity, u64 stride, memory_tag tag);

/**
 * @brief Create a new dynamic array from an existing source of data.
 * 
 * @param capacity The amount of elements the underlying array should contain
 * @param stride The size in bytes of each array element
 * @param length The amount of elements in data
 * @param data The data to be transferred to the new dynamic array
 * @return void* 
 */
void* dynarray_create_data(u64 capacity, u64 stride, u64 length, const void* data);

void* dynarray_create_data_tagged(u64 capacity, u64 stride, u64 length, memory_tag tag, const void* data);

/**
 * @brief Creates a deep copy of a dynarray. This function allocates memory
 * and the returned dynarray needs to be destroyed
 * 
 * @param dynarray Dynarray to copy
 * @return void* Copy of dynarray
 */
void* dynarray_copy(void* src);

void dynarray_destroy(void* array);

// Currently also sets the length variable
void dynarray_resize(void** array_ptr, u64 length);

void dynarray_reserve(void** array_ptr, u64 capacity);

/*
 * To use this function the address of the array (pointer). &arr
 * must be cast to (void**). 
 */
void dynarray_push(void** array_ptr, const void* element);

void dynarray_pop(void* array, void* dest);

b8 dynarray_is_empty(void* array);

/**
 * @brief Retrieves the index in the underlying container that the end of the
 * dynamic array is in.
 * Used for dynarray_push & dynarray_pop.
 * 
 * @param array The dynamic array for the function to operate on
 * @return u64
 */
u64 dynarray_length(void* array);

/**
 * @brief Clear's the dynamic array's contents.
 * NOTE: Does not change the underlying data struct's capacity
 * 
 * @param array Dynarray to clear
 */
void dynarray_clear(void* array);

/**
 * @brief Inserts the value located at element into array_ptr[index] & shifts
 * all the data after that point to the right.
 * 
 * NOTE: A pointer to the array is used becasue this funciton can cause a resize
 * if the total number of elements exceeds the capacity of the underlying arrray.
 * 
 * @param array_ptr A pointer to the dynmaic array to operate on
 * @param element A pointer to the value to enter into the array
 * @param index The index in the dynamic array to insert the element
 * @return void 
 */
void dynarray_insert(void** array_ptr, void* element, u64 index);

/**
 * @brief Removes the element at array[index] from the array and shifts the data 
 * after it to the left. 
 * 
 * @param array The dynamic array to operate on
 * @param dest The location to place the removed data
 * @param index The index of the data to remove
 * @return void 
 */
void dynarray_remove(void* array, void* dest, u64 index);

void dynarray_swap(void* array, void* transfer, u64 index);