#include "importer.h"

#include "utils/dynarray.h"

#include "core/logger.h"
#include "core/asserts.h"

#include "core/etstring.h"

#include "resources/importers/gltfimporter.h"
#include "resources/importers/pmximporter.h"

import_payload import_files(u32 file_count, const char* const* paths) {
    ETASSERT(paths);
    import_payload payload = {
        .pipelines = dynarray_create_data(
            IMPORT_PIPELINE_TYPE_COUNT,
            sizeof(import_pipeline),
            IMPORT_PIPELINE_TYPE_COUNT,
            default_import_pipelines
        ),
        
        .images = dynarray_create(1, sizeof(import_image)),
        .textures = dynarray_create(RESERVED_TEXTURE_INDEX_COUNT, sizeof(import_texture)),
        .samplers = dynarray_create(1, sizeof(import_sampler)),

        .vertices.statics = dynarray_create(1, sizeof(vertex)),
        .vertices.skinned = dynarray_create(1, sizeof(skin_vertex)),
        .indices = dynarray_create(1, sizeof(u32)),
        .geometries = dynarray_create(1, sizeof(import_geometry)),
        .meshes = dynarray_create(1, sizeof(import_mesh)),

        .skins = dynarray_create(1, sizeof(import_skin)),
        .animations = dynarray_create(1, sizeof(import_animation)),

        .nodes = dynarray_create(1, sizeof(import_node)),
        .root_nodes = dynarray_create(1, sizeof(u32)),

        .skin_instances = dynarray_create(1, sizeof(import_skin_instance)),
        .transforms = transforms_init(file_count),
    };

    for (u8 i = 0; i < IMPORT_PIPELINE_TYPE_COUNT; ++i) {
        payload.pipelines[i].instances = dynarray_create(0, payload.pipelines[i].inst_size);
        dynarray_push((void**)&payload.pipelines[i].instances, default_pipeline_default_instances[i]);
    }

    // HACK: Place default dummy positions in the texture array for importing
    dynarray_resize((void**)&payload.textures, RESERVED_TEXTURE_INDEX_COUNT);
    // HACK: END

    u32 failure_count = 0;
    for (u32 i = 0; i < file_count; ++i) {
        const char* path = paths[i];
        const char* ext = rev_str_char_search(path, '.');
        if (strs_equal(ext, ".gltf") || strs_equal(ext, ".glb")) {
            if (!import_gltf(&payload, path)) {
                ETWARN("Unable to import gltf file %s.", path);
                failure_count++;
            }
        } else if (strs_equal(ext, ".pmx")) {
            ETASSERT(false);
            if (!import_pmx(&payload, path)) {
                ETWARN("Unable to import pmx file %s.", path);
                failure_count++;
            }
        } else {
            ETWARN("Attempting to load file %s with unsupported file extension %s.", path, ext);
        }
    }
    ETASSERT(failure_count == 0);

    u32 node_count = dynarray_length(payload.nodes);
    for (u32 i = 0; i < node_count; ++i) {
        if (!payload.nodes[i].has_parent) {
            dynarray_push((void**)&payload.root_nodes, &i);
        }
    }

    return payload;
}

void import_payload_destroy(import_payload* payload) {
    u32 pipeline_count = dynarray_length(payload->pipelines);
    for (u32 i = 0; i < pipeline_count; ++i) {
        dynarray_destroy(payload->pipelines[i].instances);
    }
    dynarray_destroy(payload->pipelines);
    dynarray_destroy(payload->textures);
    dynarray_destroy(payload->samplers);

    u32 image_count = dynarray_length(payload->images);
    for (u32 i = 0; i < image_count; ++i) {
        stbi_image_free(payload->images[i].data);
        str_duplicate_free(payload->images[i].name);
    }
    dynarray_destroy(payload->images);

    u32 mesh_count = dynarray_length(payload->meshes);
    for (u32 i = 0; i < mesh_count; ++i) {
        // TODO: Free mesh allocations
    }
    dynarray_destroy(payload->meshes);

    u32 node_count = dynarray_length(payload->nodes);
    for (u32 i = 0; i < node_count; ++i) {
        dynarray_destroy(payload->nodes[i].children_indices);
    }
    dynarray_destroy(payload->nodes);

    u32 skin_count = dynarray_length(payload->skins);
    for (u32 i = 0; i < skin_count; ++i) {
        dynarray_destroy(payload->skins[i].inverse_binds);
        dynarray_destroy(payload->skins[i].joint_indices);
    }
    dynarray_destroy(payload->skins);

    u32 animation_count = dynarray_length(payload->animations);
    for (u32 i = 0; i < animation_count; ++i) {
        dynarray_destroy(payload->animations[i].channels);
        u32 anim_sampler_count = dynarray_length(payload->animations[i].samplers);
        for (u32 j = 0; j < anim_sampler_count; ++j) {
            dynarray_destroy(payload->animations[i].samplers[j].timestamps);
            dynarray_destroy(payload->animations[i].samplers[j].data);
        }
        dynarray_destroy(payload->animations[i].samplers);
    }
    dynarray_destroy(payload->animations);

    dynarray_destroy(payload->root_nodes);
}