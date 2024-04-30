#include "transforms.h"
#include "data_structures/dynarray.h"
#include "core/asserts.h"

// NOTE: Transforms data structure keeps a flat transforms array
// where each transforms is located after its parent transform in memory
// This means the array can be traversed linearly using the already calculated
// global transform of the parent transform

// TODO: Remove dummy root node logic

/** TODO: Once multithreading is used in the engine
 * Keep track of children for each entry in the transform array
 * Use the children to multithread the global transform calculation
 */

transforms transforms_init(u32 capacity) {
    ETASSERT(capacity > 0);
    transforms tforms = {
        .capacity = capacity + 1,
        .parents = etallocate(sizeof(u32) * (capacity + 1), MEMORY_TAG_TRANSFORM),
        .tforms = etallocate(sizeof(transform) * (capacity + 1), MEMORY_TAG_TRANSFORM),
    };
    etset_memory(tforms.parents, (u8)0xFF, sizeof(u32) * (capacity + 1));
    etzero_memory(tforms.tforms, sizeof(transform) * (capacity + 1));
    tforms.tforms[0] = TRANSFORM_INIT_DEFAULT;
    return tforms;
}

void transforms_deinit(transforms transforms) {
    etfree(transforms.parents, sizeof(u32) * transforms.capacity, MEMORY_TAG_TRANSFORM);
    etfree(transforms.tforms, sizeof(transform) * transforms.capacity, MEMORY_TAG_TRANSFORM);
}

// returns index of transform in transform
u32 transforms_add_root(transforms transforms, transform transform) {
    for (u32 i = 1; i < transforms.capacity; ++i) {
        if (transforms.parents[i] == (u32)-1) {
            transforms.parents[i] = 0;
            transforms.tforms[i] = transform;
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
            transforms.tforms[i] = transform;
            return i;
        }
    }
    ETASSERT_MESSAGE(false, "Resizing transforms not implemented yet");
    return (u32)-1;
}

// NOTE: Function assumes global is allocated and has count 
void transforms_recompute_global(transforms transforms, m4s* global) {
    global[0] = transform_to_matrix(transforms.tforms[0]);
    for (u32 i = 1; i < transforms.capacity; ++i) {
        if (transforms.parents[i] == (u32)-1) {
            continue;
        }

        m4s parent_global = global[transforms.parents[i]];
        m4s transform_local = transform_to_matrix(transforms.tforms[i]);
        global[i] = glms_mat4_mul(parent_global, transform_local);
    }
}