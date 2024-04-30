#include "importer.h"

#include "data_structures/dynarray.h"

#include "core/logger.h"
#include "core/asserts.h"

#include "core/etstring.h"

#include "resources/importers/gltfimporter.h"
#include "resources/importers/pmximporter.h"

// TODO: Place the default pipelines into the pipelines memeber of import_payload
import_payload import_files(u32 file_count, const char* const* paths) {
    ETASSERT(paths);
    import_payload payload = {
        .mat_index_to_mat_id = dynarray_create(1, sizeof(mat_id)),
        .pipelines = dynarray_create(1, sizeof(import_pipeline)),
        
        .images = dynarray_create(1, sizeof(import_image)),
        .textures = dynarray_create(RESERVED_TEXTURE_INDEX_COUNT, sizeof(import_texture)),
        .samplers = dynarray_create(1, sizeof(import_sampler)),
        
        .vertex_count = 0,
        .index_count = 0,
        .geometries = dynarray_create(1, sizeof(import_geometry)),
        .meshes = dynarray_create(1, sizeof(import_mesh)),
        .nodes = dynarray_create(1, sizeof(import_node)),

        .skins = dynarray_create(1, sizeof(import_skin)),
        .animations = dynarray_create(1, sizeof(import_animation)),

        .root_nodes = dynarray_create(1, sizeof(u32)),
    };

    // HACK: Place default dummy positions in the texture array for importing
    // NOTE: there should be a better way than this
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
            }
        } else {
            ETWARN("Attempting to load file %s with unsupported extension %s.", path, ext);
        }
    }
    ETASSERT(failure_count == 0);

    u32 node_count = dynarray_length(payload.nodes);
    for (u32 i = 0; i < node_count; ++i) {
        if (!payload.nodes[i].has_parent) {
            dynarray_push((void**)&payload.root_nodes, &i);
        }
    }

    // TODO: Vertex and index buffer placement here
    // TODO: Unused pipeline removal here
    // TODO: Flatten nodes to transforms here

    return payload;
}
// TODO: END

void import_payload_destroy(import_payload* payload) {
    dynarray_destroy(payload->mat_index_to_mat_id);

    u32 pipeline_count = dynarray_length(payload->pipelines);
    for (u32 i = 0; i < pipeline_count; ++i) {
        dynarray_destroy(payload->pipelines[i].instances);
    }
    dynarray_destroy(payload->pipelines);

    u32 geometry_count = dynarray_length(payload->geometries);
    for (u32 i = 0; i < geometry_count; ++i) {
        dynarray_destroy(payload->geometries[i].vertices);
        dynarray_destroy(payload->geometries[i].indices);
    }
    dynarray_destroy(payload->geometries);

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
        dynarray_destroy(payload->meshes[i].geometry_indices);
        dynarray_destroy(payload->meshes[i].material_indices);
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