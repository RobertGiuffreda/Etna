#include "gltfloader.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include "core/logger.h"
#include "core/etstring.h"

#include "renderer/src/utilities/vkinit.h"

static void recurse_print_nodes(cgltf_node* node, u32 depth);
static cgltf_accessor* get_accessor_fron_attributes(cgltf_attribute* attributes, cgltf_size attributes_count, const char* name);

static u64 node_count = 0;

mesh_asset* load_gltf_meshes(const char* path, renderer_state* state) {
    cgltf_options options = {0};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, path, &data);
    if (result != cgltf_result_success) {
        ETERROR("Error loading gltf file %s.", path);
        return 0;
    }

    result = cgltf_load_buffers(&options, data, 0);
    if (result != cgltf_result_success) {
        ETERROR("Error loading gltf file %s's buffers.", path);
    }

    mesh_asset* meshes = dynarray_create(data->meshes_count, sizeof(mesh_asset));

    ETINFO("Mesh count: %lu", data->meshes_count);

    // Use the same vectors for all meshes so that the memory doesnt 
    // reallocate as often
    vertex* vertices = dynarray_create(1, sizeof(vertex));
    u32* indices = dynarray_create(1, sizeof(u32));
    for (u32 i = 0; i < data->meshes_count; i++) {
        // Examine each mesh
        cgltf_mesh* mesh = &data->meshes[i];
        
        mesh_asset new_mesh;
        new_mesh.name = mesh->name;
        new_mesh.surfaces = dynarray_create(mesh->primitives_count, sizeof(geo_surface));

        dynarray_clear(indices);
        dynarray_clear(vertices);

        // Examine each primitive in each mesh
        for (u32 j = 0; j < mesh->primitives_count; ++j) {
            cgltf_primitive* primitive = &mesh->primitives[j];

            geo_surface new_surface;
            new_surface.start_index = dynarray_length(indices);
            new_surface.count = (u32)primitive->indices->count;

            u64 initial_vertex = dynarray_length(vertices);

            // load indices
            cgltf_accessor* index_accessor = primitive->indices;
            dynarray_resize((void**)&indices, dynarray_length(indices) + index_accessor->count);

            for (u32 k = 0; k < index_accessor->count; ++k) {
                u32 index = initial_vertex + cgltf_accessor_read_index(index_accessor, k);
                dynarray_push((void**)&indices, &index);
            }

            // Get accessors for below attributes:

            // load vertex positions
            cgltf_accessor* position = get_accessor_fron_attributes(
                primitive->attributes, primitive->attributes_count, "POSITION");
            if (position) {
                dynarray_resize((void**)&vertices, dynarray_length(vertices) + position->count);
                for (u32 k = 0; k < position->count; k++) {
                    cgltf_size element_size = cgltf_calc_size(position->type, position->component_type);

                    // Default values in case of absence in gltf
                    vertex new_v = {
                        .position = (v3s){.raw = {0.f, 0.f, 0.f}},
                        .normal = (v3s){.raw = {1.f, 0.f, 0.f}},
                        .color = (v4s){.raw = {1.f, 1.f, 1.f, 1.f}},
                        .uv_x = 0,
                        .uv_y = 0,
                    };
                    f32* pos = new_v.position.raw;
                    if (!(cgltf_accessor_read_float(position, k, pos, element_size))) {
                        ETERROR("Error attempting to read position value from %s.", path);
                        dynarray_destroy(meshes);
                        return 0;
                    }
                    dynarray_push((void**)&vertices, &new_v);
                }
            } else {
                ETERROR("Attempted to load mesh without vertex positions");
                dynarray_destroy(meshes);
                return 0;
            }

            // load vertex normals
            cgltf_accessor* normal = get_accessor_fron_attributes(
                primitive->attributes, primitive->attributes_count, "NORMAL");
            if (normal) {
                cgltf_size element_size = cgltf_calc_size(normal->type, normal->component_type);
                for (u32 k = 0; k < normal->count; ++k) {
                    vertex* v = &vertices[initial_vertex + k];
                    if (!cgltf_accessor_read_float(normal, k, v->normal.raw, element_size)) {
                        ETERROR("Error attempting to read normal value from %s.", path);
                        dynarray_destroy(meshes);
                        return 0;
                    }
                }
            } else {
                ETERROR("Attempted to load mesh without vertex normals");
                dynarray_destroy(meshes);
                return 0;
            }

            // load UV's
            cgltf_accessor* uv = get_accessor_fron_attributes(
                primitive->attributes, primitive->attributes_count, "TEXCOORD_0");
            if (uv) {
                cgltf_size element_size = cgltf_calc_size(uv->type, uv->component_type);
                for (u32 k = 0; k < uv->count; ++k) {
                    vertex* v = &vertices[initial_vertex + k];
                    v2s uvs = {.raw = {0, 0}};
                    if (!cgltf_accessor_read_float(uv, k, uvs.raw, element_size)) {
                        ETERROR("Error attempting to read normal value from %s.", path);
                        dynarray_destroy(meshes);
                        return 0;
                    }
                    v->uv_x = uvs.x;
                    v->uv_y = uvs.y;
                }
            } else {
                ETERROR("Attempted to load mesh without vertex texture coords");
                dynarray_destroy(meshes);
                return 0;
            }

            // Load vertex colors
            cgltf_accessor* color = get_accessor_fron_attributes(
                primitive->attributes, primitive->attributes_count, "COLOR_0");
            if (color) {
                cgltf_size element_size = cgltf_calc_size(color->type, color->component_type);
                for (u32 k = 0; k < color->count; ++k) {
                    vertex* v = &vertices[initial_vertex + k];
                    if (!cgltf_accessor_read_float(color, k, v->color.raw, element_size)) {
                        ETERROR("Error attempting to read normal value from %s.", path);
                        dynarray_destroy(meshes);
                        return 0;
                    }
                }
            }

            // Push the new surface into the mesh asset dynarray of surfaces
            dynarray_push((void**)&new_mesh.surfaces, &new_surface);
        }
        for (u32 i = 0; i < dynarray_length(vertices); ++i) {
            vertices[i].color.r = vertices[i].normal.r;
            vertices[i].color.g = vertices[i].normal.g;
            vertices[i].color.b = vertices[i].normal.b;
        } 
        // Upload mesh data to the GPU
        new_mesh.mesh_buffers = upload_mesh(state,
            dynarray_length(indices), indices,
            dynarray_length(vertices), vertices);

        // Place the new mesh into the meshes dynamic array
        dynarray_push((void**)&meshes, &new_mesh);
    }

    dynarray_destroy(vertices);
    dynarray_destroy(indices);

    cgltf_free(data);
    return meshes;
}

static cgltf_accessor* get_accessor_fron_attributes(cgltf_attribute* attributes, cgltf_size attributes_count, const char* name) {
    for (cgltf_size i = 0; i < attributes_count; ++i) {
        if (strings_equal(attributes[i].name, name)) {
            return attributes[i].data;
        }
    }
    return (void*)0;
}

static void recurse_print_nodes(cgltf_node* node, u32 depth) {
    if (!node) return;
    node_count++;

    for (u32 i = 0; i < node->children_count; ++i) {
        recurse_print_nodes(node->children[i], depth + 1);
    }
    ETINFO("Node %s: depth: %lu", node->name, depth);
}