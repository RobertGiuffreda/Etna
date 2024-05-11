#pragma once
#include "defines.h"
#include "math/math_types.h"

typedef struct transform {
    v3s translation;
    quat rotation;
    v3s scale;
} transform;

#define TRANSFORM_INIT_DEFAULT (transform) { .translation = GLMS_VEC3_ZERO, .rotation = GLMS_QUAT_IDENTITY, .scale = GLMS_VEC3_ONE,}
#define TRANSFORM_INIT(t, r, s) (transform) { .translation = t, .rotation = r, .scale = s, }

// NOTE: Dynarrays
typedef struct transforms {
    u32 capacity;
    u32* parents;
    m4s* global;
    transform* local;
} transforms;

transforms transforms_init(u32 capacity);
void transforms_deinit(transforms transforms);

// returns index of transform in transform
u32 transforms_add_root(transforms transforms, transform transform);
u32 transforms_add_child(transforms transforms, u32 parent_index, transform transform);

void transforms_recompute_global(transforms transforms);

static inline void transforms_set_transform(transforms transforms, u32 index, transform transform) {
    transforms.local[index] = transform;
}

static inline void transforms_set_translation(transforms transforms, u32 index, v3s translation) {
    transforms.local[index].translation = translation;
}

static inline void transforms_set_rotation(transforms transforms, u32 index, quat rotation) {
    transforms.local[index].rotation = rotation;
}

static inline void transforms_set_scale(transforms transforms, u32 index, v3s scale) {
    transforms.local[index].scale = scale;
}

static inline m4s transform_to_matrix(transform transform) {
    m4s translation_matrix = glms_translate_make(transform.translation);
    m4s rotation_matrix = glms_quat_rotate(translation_matrix, transform.rotation);
    return glms_mat4_mul(rotation_matrix, glms_scale_make(transform.scale));
}

static inline transform matrix_to_transform(m4s matrix) {
    v4s translation;
    mat4s rotate_mat;
    v3s scale;
    glms_decompose(matrix, &translation, &rotate_mat, &scale);
    return TRANSFORM_INIT(glms_vec3(translation), glms_mat4_quat(rotate_mat), scale);
}