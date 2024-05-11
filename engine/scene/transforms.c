#include "transforms.h"
#include "utils/dynarray.h"
#include "core/asserts.h"

// NOTE: Transforms data structure keeps a flat transforms array
// where each transforms is located after its parent transform in memory
// This means the array can be traversed linearly using the already calculated
// global transform of the parent transform

/** Todos
 * TODO: Remove dummy root node logic; capacity + 1 in init
 * TODO: Once multithreading is used in the engine
 * Keep track of children for each entry in the transform array
 * Use the children to multithread the global transform calculation
 * TODO: Have globals be a CPU cached GPU buffer to immediatly transfer upon completion
 */


transforms transforms_init(u32 capacity) {
    ETASSERT(capacity > 0);
    transforms tforms = {
        .capacity = capacity + 1,
        .parents = etallocate(sizeof(u32) * (capacity + 1), MEMORY_TAG_TRANSFORM),
        .global = etallocate(sizeof(m4s) * (capacity + 1), MEMORY_TAG_TRANSFORM),
        .local = etallocate(sizeof(transform) * (capacity + 1), MEMORY_TAG_TRANSFORM),
    };
    etset_memory(tforms.parents, (u8)0xFF, sizeof(u32) * (capacity + 1));
    etzero_memory(tforms.local, sizeof(transform) * (capacity + 1));
    tforms.local[0] = TRANSFORM_INIT_DEFAULT;
    return tforms;
}

void transforms_deinit(transforms transforms) {
    etfree(transforms.parents, sizeof(u32) * transforms.capacity, MEMORY_TAG_TRANSFORM);
    etfree(transforms.local, sizeof(transform) * transforms.capacity, MEMORY_TAG_TRANSFORM);
    etfree(transforms.global, sizeof(m4s) * transforms.capacity, MEMORY_TAG_TRANSFORM);
}

// returns index of transform in transform
u32 transforms_add_root(transforms transforms, transform transform) {
    for (u32 i = 1; i < transforms.capacity; ++i) {
        if (transforms.parents[i] == (u32)-1) {
            transforms.parents[i] = 0;
            transforms.local[i] = transform;
            return i;
        }
    }
    ETASSERT_MESSAGE(false, "Resizing transforms not implemented yet");
    return (u32)-1;
}

u32 transforms_add_child(transforms transforms, u32 parent_index, transform transform) {
    for (u32 i = parent_index + 1; i < transforms.capacity; ++i) {
        if (transforms.parents[i] == (u32)-1) {
            transforms.parents[i] = parent_index;
            transforms.local[i] = transform;
            return i;
        }
    }
    ETASSERT_MESSAGE(false, "Resizing transforms not implemented yet");
    return (u32)-1;
}

// NOTE: Function assumes global is allocated and has count
void transforms_recompute_global(transforms transforms) {
    transforms.global[0] = transform_to_matrix(transforms.local[0]);
    for (u32 i = 1; i < transforms.capacity; ++i) {
        if (transforms.parents[i] == (u32)-1) {
            continue;
        }

        transforms.global[i] = glms_mat4_mul(
            transforms.global[transforms.parents[i]],
            transform_to_matrix(transforms.local[i])
        );
    }
}

void transforms_grow(transforms* transforms, u32 count) {
    if (count == 0) return;
    u32 new_capacity = transforms->capacity + count;

    u32* parents_new = etallocate(sizeof(u32) * new_capacity, MEMORY_TAG_TRANSFORM);
    etset_memory(&parents_new[transforms->capacity], 0xFF, count * sizeof(u32));
    transform* local_new = etallocate(sizeof(transform) * new_capacity, MEMORY_TAG_TRANSFORM);
    etzero_memory(&local_new[transforms->capacity], sizeof(transform) * count);
    m4s* global_new = etallocate(sizeof(m4s) * new_capacity, MEMORY_TAG_TRANSFORM);

    etcopy_memory(parents_new, transforms->parents, sizeof(u32) * transforms->capacity);
    etcopy_memory(local_new, transforms->local, sizeof(transform) * transforms->capacity);
    etcopy_memory(global_new, transforms->global, sizeof(m4s) * transforms->capacity);

    etfree(transforms->parents, sizeof(u32) * transforms->capacity, MEMORY_TAG_TRANSFORM);
    etfree(transforms->local, sizeof(transform) * transforms->capacity, MEMORY_TAG_TRANSFORM);
    etfree(transforms->global, sizeof(m4s) * transforms->capacity, MEMORY_TAG_TRANSFORM);

    transforms->parents = parents_new;
    transforms->local = local_new;
    transforms->global = global_new;
    transforms->capacity = new_capacity;
}