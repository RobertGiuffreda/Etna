#pragma once

#include "defines.h"

#define DYNARRAY_DEFAULT_CAPACITY 1

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
 * Calling with a length of zero is fine as we add one to length on resize
 */
void* dynarray_create(u64 length, u64 stride);

void dynarray_destroy(void* array);

void dynarray_resize(void** array_ptr, u64 length);

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
 * @return void
 */
u64 dynarray_length(void* array);

/**
 * @brief Changes the index that functions use to determine the end of the array.
 * 
 * NOTE: This function does not change the size of the data structure housing the array.
 * Setting the index to higher than the underlying struct's capacity does not cause
 * a resize. It is undefined behavoir.
 * 
 * @param array The dynamic array for the function to operate on
 * @param index Value to set the index of array
 * @return void
 */
void dynarray_set_length(void* array, u64 index);

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