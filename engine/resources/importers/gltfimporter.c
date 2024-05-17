#include "gltfimporter.h"
#include "importer_types.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "utils/dynarray.h"

#include "core/logger.h"
#include "core/asserts.h"
#include "core/etstring.h"
#include "core/etfile.h"

#include "memory/etmemory.h"
#include "renderer/src/vk_types.h"

// NOTE: Currently leaks memory on failure to load gltf file
// TODO: Create a function to call when importing a file fails
// that checks for memory allocations and frees them before returning

// TODO: Handle cases where parts of the gltf are missing/not present
// No materials or images, no tex coords, etc... 

static void recurse_print_nodes(cgltf_node* node, u32 depth, u64* node_count);
static cgltf_accessor* get_accessor_from_attributes(cgltf_attribute* attributes, cgltf_size attributes_count, const char* name);

static void* load_image_data(cgltf_image* in_image, const char* gltf_path, int* width, int* height, int* channels);

static void combine_paths(char* path, const char* base, const char* uri);

static VkFilter gltf_filter_to_vk_filter(cgltf_int filter);
static VkSamplerMipmapMode gltf_filter_to_vk_mipmap_mode(cgltf_int filter);

static m4s mat4s_add4(m4s m0, m4s m1, m4s m2, m4s m3);

/** TODO: Next:
 * Put vertex data into single vertex buffer on import
 * Put index data into single index buffer on import
 * Create transforms from nodes on import
 * 
 * Assumptions:
 * Textures: Sampler index is present
 * Textures: Image index is present
 * Skins:    Inverse Bind Matrices accessor is present
 * Meshes:   Logic to handle the case where primitives use the same 
 *           attribute accessors(same vertices and indices) but differ
 *           in material.
 */

b8 import_gltf(import_payload* payload, const char* path) {
    // Attempt to load gltf file data with cgltf library
    cgltf_options options = {0};
    cgltf_data* data = 0;
    cgltf_result result = cgltf_parse_file(&options, path, &data);
    if (result != cgltf_result_success) {
        ETERROR("Failed to load gltf file %s.", path);
        return false;
    }

    // Attempt to load the data from buffers
    result = cgltf_load_buffers(&options, data, path);
    if (result != cgltf_result_success) {
        ETERROR("Failed to load gltf file %s.", path);
        cgltf_free(data);
        return false;
    }

    u32 img_start = dynarray_grow((void**)&payload->images, data->images_count);
    for (u32 i = 0; i < data->images_count; ++i) {
        import_image* image = &payload->images[img_start + i];
        image->data = load_image_data(&data->images[i], path, &image->width, &image->height, &image->channels);
        image->name = (data->images[i].name) ? str_dup_alloc(data->images[i].name) : str_dup_alloc("NamelessImage");
    }

    u32 sampler_start = dynarray_grow((void**)&payload->samplers, data->samplers_count);
    for (u32 i = 0; i < data->samplers_count; ++i) {
        payload->samplers[sampler_start + i] = (import_sampler) {
            .mag_filter = gltf_filter_to_vk_filter(data->samplers[i].mag_filter),
            .min_filter = gltf_filter_to_vk_filter(data->samplers[i].min_filter),
            .mip_map_mode = gltf_filter_to_vk_mipmap_mode(data->samplers[i].min_filter),
        };
    }

    u32 tex_start = dynarray_grow((void**)&payload->textures, data->textures_count);
    for (u32 i = 0; i < data->textures_count; ++i) {
        payload->textures[tex_start + i].image_id = img_start + cgltf_image_index(data, data->textures[i].image);
        payload->textures[tex_start + i].sampler_id = sampler_start + cgltf_sampler_index(data, data->textures[i].sampler);
    }

    mat_id* mat_index_to_mat_id = etallocate(sizeof(mat_id) * data->materials_count, MEMORY_TAG_IMPORTER);
    for (u32 i = 0; i < data->materials_count; ++i) {
        cgltf_material mat = data->materials[i];
        cgltf_pbr_metallic_roughness mr = mat.pbr_metallic_roughness;
        pbr_mr_instance instance = {
            .color_factors = {
                .r = mr.base_color_factor[0],
                .g = mr.base_color_factor[1],
                .b = mr.base_color_factor[2],
                .a = mr.base_color_factor[3],
            },
            .color_tex_index = RESERVED_TEXTURE_WHITE_INDEX,
            .metalness = mr.metallic_factor,
            .roughness = mr.roughness_factor,
            .mr_tex_index = RESERVED_TEXTURE_WHITE_INDEX,
            .normal_index = RESERVED_TEXTURE_NORMAL_INDEX,
        };

        if (mr.base_color_texture.texture) {
            instance.color_tex_index = tex_start + cgltf_texture_index(data, mr.base_color_texture.texture);
        }

        if (mr.metallic_roughness_texture.texture) {
            instance.mr_tex_index = tex_start + cgltf_texture_index(data, mr.metallic_roughness_texture.texture);
        }
        
        if (mat.normal_texture.texture) {
            instance.normal_index = tex_start + cgltf_texture_index(data, mat.normal_texture.texture);
        }
        
        mat_id id;
        switch (data->materials[i].alpha_mode) {
            case cgltf_alpha_mode_blend:
                id.pipe_id = IMPORT_PIPELINE_TYPE_GLTF_TRANSPARENT;
                id.inst_id = dynarray_length(payload->pipelines[IMPORT_PIPELINE_TYPE_GLTF_TRANSPARENT].instances);
                dynarray_push((void**)&payload->pipelines[IMPORT_PIPELINE_TYPE_GLTF_TRANSPARENT].instances, &instance);
                break;
            default:
                ETWARN("Unknown alpha mode for material %lu in gltf file %s. Setting opaque.", i, path);
            case cgltf_alpha_mode_mask:
            case cgltf_alpha_mode_opaque: {
                id.pipe_id = IMPORT_PIPELINE_TYPE_GLTF_DEFAULT;
                id.inst_id = dynarray_length(payload->pipelines[IMPORT_PIPELINE_TYPE_GLTF_DEFAULT].instances);
                dynarray_push((void**)&payload->pipelines[IMPORT_PIPELINE_TYPE_GLTF_DEFAULT].instances, &instance);
                break;
            }
        }
        
        mat_index_to_mat_id[i] = id;
    }

    // NOTE: For import_skin_instance
    import_skin_instance* skin_insts_map = etallocate(
        sizeof(skin_insts_map[0]) * data->skins_count,
        MEMORY_TAG_IMPORTER);
    for (u32 i = 0; i < data->skins_count; ++i) {
        skin_insts_map[i].skin = (u32)-1;
    }

    u32 mesh_start = dynarray_grow((void**)&payload->meshes, data->meshes_count);
    u32 skin_start = dynarray_grow((void**)&payload->skins, data->skins_count);
    u32 node_start = dynarray_grow((void**)&payload->nodes, data->nodes_count);
    for (u32 i = 0; i < data->nodes_count; ++i) {
        import_node node = {0};
        if (data->nodes[i].parent != NULL) {
            node.has_parent = true;
            u32 local_parent_index = cgltf_node_index(data, data->nodes[i].parent);
            node.parent_index = node_start + local_parent_index;
        }
        if (data->nodes[i].mesh != NULL) {
            node.has_mesh = true;
            u32 local_mesh_index = cgltf_mesh_index(data, data->nodes[i].mesh);
            node.mesh_index = mesh_start + local_mesh_index;
            if (data->nodes[i].skin != NULL) {
                node.has_skin = true;
                u32 local_skin_index = cgltf_skin_index(data, data->nodes[i].skin);
                node.skin_index = skin_start + local_skin_index;

                if (skin_insts_map[local_skin_index].skin == (u32)-1) {
                    skin_insts_map[local_skin_index].skin = node.skin_index;
                    skin_insts_map[local_skin_index].meshes = dynarray_create(1, sizeof(u32));
                }
                dynarray_push((void**)&skin_insts_map[local_skin_index].meshes, &node.mesh_index);
            }
        }

        node.children_indices = dynarray_create(data->nodes[i].children_count, sizeof(u32));
        dynarray_resize((void**)&node.children_indices, data->nodes[i].children_count);
        for (u32 j = 0; j < data->nodes[i].children_count; ++j) {
            u32 local_node_index = cgltf_node_index(data, data->nodes[i].children[j]);
            node.children_indices[j] = node_start + local_node_index;
        }

        cgltf_node_transform_local(&data->nodes[i], (cgltf_float*)node.local_transform.raw);
        cgltf_node_transform_world(&data->nodes[i], (cgltf_float*)node.world_transform.raw);

        payload->nodes[node_start + i] = node;
    }

    struct {
        b8 skinned;
        u32 vertex_start;
        u32 vertex_count;
        u32 index_start;
        u32 index_count;
        u32 geometry_start;
        u32 geometry_count;
    }* mesh_info = etallocate(
        sizeof(mesh_info[0]) * data->meshes_count,
        MEMORY_TAG_IMPORTER);
    etzero_memory(mesh_info, sizeof(mesh_info[0]) * data->meshes_count);

    u32 static_vertex_start = dynarray_length(payload->vertices.statics);
    u32 skinned_vertex_start = dynarray_length(payload->vertices.skinned);
    u32 index_start = dynarray_length(payload->indices);
    u32 geometry_start = dynarray_length(payload->geometries);

    // TODO: Replace this passthrough to get grow counts by using custom
    // memory allocators to avoid the expensive allocations instead.
    u32 static_vertex_count = 0;
    u32 skinned_vertex_count = 0;
    u32 total_index_count = 0;
    u32 total_geometry_count = 0;
    for (u32 i = 0; i < data->meshes_count; ++i) {
        for (u32 j = 0; j < data->meshes[i].primitives[0].attributes_count; ++j) {
            switch (data->meshes[i].primitives[0].attributes[j].type) {
                case cgltf_attribute_type_joints:
                case cgltf_attribute_type_weights:
                    mesh_info[i].skinned = true;
                    break;
                default: {
                    break;
                }
            }
        }
        mesh_info[i].vertex_start = (mesh_info[i].skinned ?
            (skinned_vertex_start + skinned_vertex_count) :
            (static_vertex_start + static_vertex_count));
        mesh_info[i].index_start = index_start + total_index_count;
        mesh_info[i].geometry_start = geometry_start + total_geometry_count;
        mesh_info[i].geometry_count = data->meshes[i].primitives_count;
        total_geometry_count += data->meshes[i].primitives_count;

        for (u32 j = 0; j < data->meshes[i].primitives_count; ++j) {
            mesh_info[i].index_count += data->meshes[i].primitives[j].indices->count;
            mesh_info[i].vertex_count += data->meshes[i].primitives[j].attributes[0].data->count;
        }

        total_index_count += mesh_info[i].index_count;
        if (mesh_info[i].skinned) {
            skinned_vertex_count += mesh_info[i].vertex_count;
        } else {
            static_vertex_count += mesh_info[i].vertex_count;
        }
    }

    dynarray_grow((void**)&payload->vertices.statics, static_vertex_count);
    dynarray_grow((void**)&payload->vertices.skinned, skinned_vertex_count);
    dynarray_grow((void**)&payload->indices, total_index_count);
    dynarray_grow((void**)&payload->geometries, total_geometry_count);
    // TODO: END

    for (u32 i = 0; i < static_vertex_count; ++i) {
        payload->vertices.statics[static_vertex_start + i] = (vertex) {
            .position = (v3s){0, 0, 0},
            .normal = (v3s){1, 0, 0},
            .uv_x = 0.0f,
            .uv_y = 0.0f,
            .color = (v4s){1.f, 1.f, 1.f, 1.f},
        };
    }

    for (u32 i = 0; i < skinned_vertex_count; ++i) {
        payload->vertices.skinned[skinned_vertex_start + i] = (skin_vertex) {
            .vertex = {
                .position = (v3s){0, 0, 0},
                .normal = (v3s){1, 0, 0},
                .uv_x = 0.0f,
                .uv_y = 0.0f,
                .color = (v4s){1.f, 1.f, 1.f, 1.f},
            },
            .joints = (iv4s){0, 0, 0, 0},
            .weights = (v4s){0.f, 0.f, 0.f, 0.f},
        };
    }

    for (u32 i = 0; i < data->meshes_count; ++i) {
        import_mesh* mesh = &payload->meshes[mesh_start + i];
        
        // Get morph information
        mesh->morph_count = data->meshes[i].primitives[0].targets_count;
        mesh->morph_weights = etallocate(sizeof(f32) * mesh->morph_count, MEMORY_TAG_IMPORTER);
        if (data->meshes[i].weights) {
            etcopy_memory(
                mesh->morph_weights,
                data->meshes[i].weights,
                mesh->morph_count * sizeof(f32)
            );
        } else {
            etzero_memory(
                mesh->morph_weights,
                mesh->morph_count * sizeof(f32)
            );
        }

        u32 morph_stride = 0;
        if (mesh->morph_count) {
            for (u32 j = 0; j < data->meshes[i].primitives[0].targets[0].attributes_count; ++j) {
                switch (data->meshes[i].primitives[0].targets[0].attributes[j].type) {
                    case cgltf_attribute_type_position:
                    case cgltf_attribute_type_normal:
                        morph_stride += sizeof(v3s);
                        break;
                    default: {
                        break;
                    }
                }
            }
        }
        mesh->morph_targets = dynarray_create(mesh->morph_count * mesh_info[i].vertex_count, morph_stride);
        dynarray_resize((void**)&mesh->morph_targets, mesh->morph_count * mesh_info[i].vertex_count);

        // Get geometry information
        u32 geometry_count = data->meshes[i].primitives_count;
        mesh->geo_count = geometry_count;
        mesh->geometries = etallocate(sizeof(u32) * geometry_count, MEMORY_TAG_IMPORTER);
        mesh->materials = etallocate(sizeof(mat_id) * geometry_count, MEMORY_TAG_IMPORTER);

        u32 vertex_stride = mesh_info[i].skinned ? sizeof(skin_vertex) : sizeof(vertex);
        mesh->vertex_type = (mesh_info[i].skinned ?
            (mesh->morph_count ? VERTEX_SKIN_MORPH : VERTEX_SKINNED) :
            (mesh->morph_count ? VERTEX_MORPHING   : VERTEX_STATIC)
        );

        mesh->vertex_offset = mesh_info[i].vertex_start;
        mesh->vertex_count = mesh_info[i].vertex_count;

        u32 vertex_start = mesh_info[i].vertex_start;
        u32 index_start = mesh_info[i].index_start;
        for (u32 j = 0; j < geometry_count; ++j) {
            u32 vertex_count = data->meshes[i].primitives[j].attributes[0].data->count;

            v4s min_pos = GLMS_VEC4_ONE;
            v4s max_pos = GLMS_VEC4_ONE;
            void* accessor_data = dynarray_create(1, sizeof(u32));
            for (u32 k = 0; k < data->meshes[i].primitives[j].attributes_count; ++k) {
                cgltf_attribute* attribute = &data->meshes[i].primitives[j].attributes[k];

                if (attribute->type == cgltf_attribute_type_joints) {
                    dynarray_resize((void**)&accessor_data, 4 * vertex_count);
                    iv4s* acc_data = accessor_data;
                    for (u32 l = 0; l < vertex_count; ++l) {
                        cgltf_accessor_read_uint(
                            attribute->data,
                            l,
                            (u32*)&acc_data[l],
                            4
                        );
                    }
                } else {
                    if (attribute->type == cgltf_attribute_type_position) {
                        ETASSERT(attribute->data->has_min && attribute->data->has_max);
                        etcopy_memory(&min_pos, attribute->data->min, sizeof(v3s));
                        etcopy_memory(&max_pos, attribute->data->max, sizeof(v3s));
                    }
                    u64 float_count = cgltf_accessor_unpack_floats(attribute->data, NULL, 0);
                    dynarray_resize((void**)&accessor_data, float_count);
                    cgltf_accessor_unpack_floats(attribute->data, accessor_data, float_count);
                }

                void* vertices = (mesh->vertex_type >= VERTEX_SKINNED ? 
                    ((void*)&payload->vertices.skinned[vertex_start]) :
                    ((void*)&payload->vertices.statics[vertex_start])
                );
                for (u32 l = 0; l < vertex_count; ++l) {
                    void* vert = (u8*)vertices + (l * vertex_stride);
                    switch (attribute->type) {
                        case cgltf_attribute_type_position:
                            ((vertex*)vert)->position = ((v3s*)accessor_data)[l];
                            break;
                        case cgltf_attribute_type_normal:
                            ((vertex*)vert)->normal = ((v3s*)accessor_data)[l];
                            break;
                        case cgltf_attribute_type_texcoord:
                            // if (attribute->index != 0) break;
                            ((vertex*)vert)->uv_x = ((v2s*)accessor_data)[l].x;
                            ((vertex*)vert)->uv_y = ((v2s*)accessor_data)[l].y;
                            break;
                        case cgltf_attribute_type_color:
                            ((vertex*)vert)->color = ((v4s*)accessor_data)[l];
                            break;
                        case cgltf_attribute_type_joints:
                            ((skin_vertex*)vert)->joints = ((iv4s*)accessor_data)[l];
                            break;
                        case cgltf_attribute_type_weights:
                            ((skin_vertex*)vert)->weights = ((v4s*)accessor_data)[l];
                            break;
                        default: {
                            break;
                        }
                    }
                }
            }

            // Morph target information
            for (u32 k = 0; k < mesh->morph_count; ++k) {
                cgltf_morph_target* cgltf_target = &data->meshes[i].primitives[j].targets[k];
                for (u32 l = 0; l < cgltf_target->attributes_count; ++l) {
                    u64 float_count = cgltf_accessor_unpack_floats(cgltf_target->attributes[l].data, NULL, 0);
                    dynarray_resize((void**)&accessor_data, float_count);
                    cgltf_accessor_unpack_floats(
                        cgltf_target->attributes[l].data,
                        accessor_data,
                        float_count
                    );

                    for (u32 m = 0; m < vertex_count; ++m) {
                        u64 morph_offset = (morph_stride * m);
                        switch (cgltf_target->attributes[l].type) {
                            case cgltf_attribute_type_position:
                                morph_offset += k * sizeof(v3s);
                                *((v3s*)((u8*)mesh->morph_targets + morph_offset)) = ((v3s*)accessor_data)[m];
                                break;
                            case cgltf_attribute_type_normal:
                                morph_offset += (mesh->morph_count + k) * sizeof(v3s);
                                *((v3s*)((u8*)mesh->morph_targets + morph_offset)) = ((v3s*)accessor_data)[m];
                                break;
                            // TODO: Implement
                            case cgltf_attribute_type_tangent:
                                break;
                            case cgltf_attribute_type_texcoord:
                                break;
                            // TODO: END
                            default: {
                                break;
                            }
                        }
                    }
                }
            }

            u32 index_count = data->meshes[i].primitives[j].indices->count;
            u32 geo_index = mesh_info[i].geometry_start + j;
            payload->geometries[geo_index] = (import_geometry) {
                .index_start = index_start,
                .index_count = index_count,
                .origin = glms_vec4_scale(glms_vec4_add(max_pos, min_pos), 0.5f),
                .extent = glms_vec4_scale(glms_vec4_sub(max_pos, min_pos), 0.5f)};
            payload->geometries[geo_index].radius = glms_vec3_norm(glms_vec3(payload->geometries[geo_index].extent));

            mesh->geometries[j] = geo_index;

            u32 local_vertex_start = vertex_start - mesh_info[i].vertex_start;
            for (u32 k = 0; k < index_count; ++k) {
                payload->indices[index_start + k] = local_vertex_start + cgltf_accessor_read_index(
                    data->meshes[i].primitives[j].indices,
                    k
                );
            }

            mesh->materials[j] = (mat_id) {
                .pipe_id = IMPORT_PIPELINE_TYPE_GLTF_DEFAULT,
                .inst_id = 0
            };
            if (data->meshes[i].primitives[j].material) {
                mesh->materials[j] = mat_index_to_mat_id[
                    cgltf_material_index(
                        data,
                        data->meshes[i].primitives[j].material
                    )
                ];
            }

            vertex_start += vertex_count;
            index_start += index_count;
        }
    }

    etfree(mesh_info, sizeof(mesh_info[0]) * data->meshes_count, MEMORY_TAG_IMPORTER);

    for (u32 i = 0; i < data->skins_count; ++i) {
        ETASSERT_MESSAGE(data->skins[i].inverse_bind_matrices, "Inverse Bind Matrices Assumption Failure");

        u32 joint_count = data->skins[i].joints_count;
        payload->skins[skin_start + i] = (import_skin) {
            .inverse_binds = dynarray_create(joint_count, sizeof(m4s)),
            .joint_indices = dynarray_create(joint_count, sizeof(u32)),
        };
        dynarray_resize((void**)&payload->skins[skin_start + i].inverse_binds, joint_count);
        dynarray_resize((void**)&payload->skins[skin_start + i].joint_indices, joint_count);

        u32 float_count = joint_count * sizeof(m4s) / sizeof(f32);
        cgltf_accessor_unpack_floats(
            data->skins[i].inverse_bind_matrices,
            (f32*)payload->skins[skin_start + i].inverse_binds,
            float_count);
        for (u32 j = 0; j < joint_count; ++j) {
            payload->skins[skin_start + i].joint_indices[j] = node_start + cgltf_node_index(data, data->skins[i].joints[j]);
        }

        if (skin_insts_map[i].skin != (u32)-1) {
            dynarray_push((void**)&payload->skin_instances, &skin_insts_map[i]);
        }
    }

    etfree(
        skin_insts_map,
        sizeof(skin_insts_map[0]) * data->skins_count,
        MEMORY_TAG_IMPORTER
    );

    u32 animation_start = dynarray_grow((void**)&payload->animations, data->animations_count);
    for (u32 i = 0; i < data->animations_count; ++i) {
        payload->animations[animation_start + i] = (import_animation) {
            .duration = 0.0f,
            .channels = dynarray_create(data->animations[i].channels_count, sizeof(import_anim_channel)),
            .samplers = dynarray_create(data->animations[i].samplers_count, sizeof(import_anim_sampler)),
        };
        dynarray_resize((void**)&payload->animations[animation_start + i].channels, data->animations[i].channels_count);
        dynarray_resize((void**)&payload->animations[animation_start + i].samplers, data->animations[i].samplers_count);

        for (u32 j = 0; j < data->animations[i].channels_count; ++j) {
            payload->animations[animation_start + i].channels[j] = (import_anim_channel) {
                .sampler_index = cgltf_animation_sampler_index(&data->animations[i], data->animations[i].channels[j].sampler),
                .target_index = node_start + cgltf_node_index(data, data->animations[i].channels[j].target_node),
                .trans_type = (transform_type)data->animations[i].channels[j].target_path,
            };
        }

        f32 duration = 0.f;
        for (u32 j = 0; j < data->animations[i].samplers_count; ++j) {
            payload->animations[animation_start + i].samplers[j] = (import_anim_sampler) {
                .timestamps = dynarray_create(data->animations[i].samplers[j].input->count, sizeof(f32)),
                .data = dynarray_create(data->animations[i].samplers[j].output->count, sizeof(v4s)),
                .interp_type = data->animations[i].samplers[j].interpolation,
            };
            u32 times_count = data->animations[i].samplers[j].input->count;
            u32 data_count = data->animations[i].samplers[j].output->count;
            dynarray_resize((void**)&payload->animations[animation_start + i].samplers[j].timestamps, times_count);
            dynarray_resize((void**)&payload->animations[animation_start + i].samplers[j].data, data_count);

            cgltf_accessor_unpack_floats(
                data->animations[i].samplers[j].input,
                payload->animations[animation_start + i].samplers[j].timestamps,
                times_count);

            switch (data->animations[i].samplers[j].output->type) {
                case cgltf_type_vec3:
                    for (u32 k = 0; k < data_count; ++k) {
                        cgltf_accessor_read_float(
                            data->animations[i].samplers[j].output,
                            k,
                            (f32*)&payload->animations[animation_start + i].samplers[j].data[k],
                            cgltf_num_components(cgltf_type_vec3));
                        payload->animations[animation_start + i].samplers[j].data[k].w = 1.f;
                    }
                    break;
                case cgltf_type_vec4:
                    cgltf_accessor_unpack_floats(
                        data->animations[i].samplers[j].output,
                        (f32*)payload->animations[animation_start + i].samplers[j].data,
                        data_count * 4);
                    break;
                case cgltf_type_scalar:
                    // TODO: Implement morph weight animation
                    break;
                default: {
                    ETERROR("Unknown Animation Sampler Component Type.");
                    break;
                }
            }

            duration = glm_max(duration, data->animations[i].samplers[j].input->max[0]);
        }
        payload->animations[animation_start + i].duration = duration;
    }

    return true;
}

m4s mat4s_add4(m4s m0, m4s m1, m4s m2, m4s m3) {
    m4s return_mat;
    return_mat.m00 = m0.m00 + m1.m00 + m2.m00 + m3.m00;
    return_mat.m01 = m0.m01 + m1.m01 + m2.m01 + m3.m01;
    return_mat.m02 = m0.m02 + m1.m02 + m2.m02 + m3.m02;
    return_mat.m03 = m0.m03 + m1.m03 + m2.m03 + m3.m03;
    return_mat.m10 = m0.m10 + m1.m10 + m2.m10 + m3.m10;
    return_mat.m11 = m0.m11 + m1.m11 + m2.m11 + m3.m11;
    return_mat.m12 = m0.m12 + m1.m12 + m2.m12 + m3.m12;
    return_mat.m13 = m0.m13 + m1.m13 + m2.m13 + m3.m13;
    return_mat.m20 = m0.m20 + m1.m20 + m2.m20 + m3.m20;
    return_mat.m21 = m0.m21 + m1.m21 + m2.m21 + m3.m21;
    return_mat.m22 = m0.m22 + m1.m22 + m2.m22 + m3.m22;
    return_mat.m23 = m0.m23 + m1.m23 + m2.m23 + m3.m23;
    return_mat.m30 = m0.m30 + m1.m30 + m2.m30 + m3.m30;
    return_mat.m31 = m0.m31 + m1.m31 + m2.m31 + m3.m31;
    return_mat.m32 = m0.m32 + m1.m32 + m2.m32 + m3.m32;
    return_mat.m33 = m0.m33 + m1.m33 + m2.m33 + m3.m33;
    return return_mat;
}

b8 gltf_dump_json(const char* gltf_path) {
    cgltf_options options = {0};
    cgltf_data* data = 0;
    cgltf_result result = cgltf_parse_file(&options, gltf_path, &data);
    if (result != cgltf_result_success) {
        ETERROR("Failed to load gltf file %s.", gltf_path);
        return false;
    }

    u32 len = str_char_offset(gltf_path, rev_str_char_search(gltf_path, '.'));    
    char* dump_path = sub_strs_concat_alloc(gltf_path, len, ".json", 5);

    etfile* json_file = NULL;
    b8 file_result = file_open(dump_path, FILE_WRITE_FLAG, &json_file);
    if (!file_result) {
        ETERROR("%s failed to open.", dump_path);
        return false;
    }

    u64 bytes_written = 0;
    file_result = file_write(json_file, data->json_size, (void*)data->json, &bytes_written);
    if (!file_result) {
        ETERROR("Failed to write %s's json data to %s.", gltf_path, dump_path);
        return false;
    }

    file_close(json_file);
    ETINFO("Json from %s successfully dumped to %s", gltf_path, dump_path);
    str_free(dump_path);
    return true;
}

void* load_image_data(
    cgltf_image* in_image,
    const char* gltf_path,
    int* width,
    int* height,
    int* channels
) {
    if (in_image->uri == NULL && in_image->buffer_view == NULL) {
        return NULL;
    }

    // Load the image from a buffer view
    if (in_image->buffer_view) {
        void* buffer_data = in_image->buffer_view->buffer->data;
        u64 byte_offset = in_image->buffer_view->offset;
        u64 byte_length = in_image->buffer_view->size;

        return stbi_load_from_memory(
            (u8*)buffer_data + byte_offset,
            byte_length,
            width,
            height,
            channels,
            /* requested channels: */ 4
        );
    }

    const char* uri = in_image->uri;

    // https://en.wikipedia.org/wiki/Data_URI_scheme
    // base64 is 6 bits stored in a string character. As a byte is 8 bits, a min of 
    // string length * (3/4) bytes in memory to hold the resulting data
    if (strsn_equal(uri, "data:", 5)) {
        const char* comma = str_char_search(uri, ',');
        if (comma && comma - uri >= 7 && strsn_equal(comma - 7, ";base64", 7)) {
            cgltf_options options = {0};
            cgltf_size size = (str_length(comma - 7) * 3) / 4;

            void* decoded_data;
            cgltf_result result = cgltf_load_buffer_base64(&options, size, comma, &decoded_data);
            if (result != cgltf_result_success) {
                return NULL;
            }

            void* data = stbi_load_from_memory(
                decoded_data, (int)size, width, height, channels, 4);
            CGLTF_FREE(decoded_data);
            return data;
        }
    }

    // Image is located in an external file
    if (str_str_search(uri, "://") == NULL && gltf_path) {
        u64 full_path_bytes = str_length(gltf_path) + str_length(uri) + 1;
        char* full_path = etallocate(
            full_path_bytes,
            MEMORY_TAG_STRING);
        combine_paths(full_path, gltf_path, uri);
        cgltf_decode_uri(full_path + str_length(full_path) - str_length(uri));
        void* data = stbi_load(full_path, width, height, channels, 4);
        etfree(full_path, full_path_bytes, MEMORY_TAG_STRING);
        return data;
    }

    return NULL;
}

// From cgltf_implementation
static void combine_paths(char* path, const char* base, const char* uri) {
    const char* b0 = base;
    const char* s0 = NULL;
    const char* s1 = NULL;
    while(*b0 != '\0') {
        if (*b0 == '/') s0 = b0;
        if (*b0 == '\\') s1 = b0;
        ++b0;
    }
    const char* slash = s0 ? (s1 && s1 > s0 ? s1 : s0) : s1;

    if (slash) {
        u64 prefix = slash - base + 1;

        strn_copy(path, base, prefix);
        str_copy(path + prefix, uri);
    } else {
        str_copy(path, uri);
    }
}

static void print_nodes(cgltf_node* head) {
    u64 node_count = 0;
    recurse_print_nodes(head, 0, &node_count);
    ETINFO("Node count: %llu.", node_count);
}

static void recurse_print_nodes(cgltf_node* node, u32 depth, u64* node_count) {
    if (!node) return;
    (*node_count)++;

    for (u32 i = 0; i < node->children_count; ++i) {
        recurse_print_nodes(node->children[i], depth + 1, node_count);
    }
    ETINFO("Node %s: depth: %lu", node->name, depth);
}

// Values from https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#reference-sampler
#define GLTF_FILTER_NEAREST 9728
#define GLTF_FILTER_LINEAR 9729
#define GLTF_FILTER_NEAREST_MIPMAP_NEAREST 9984
#define GLTF_FILTER_LINEAR_MIPMAP_NEAREST 9985
#define GLTF_FILTER_NEAREST_MIPMAP_LINEAR 9986
#define GLTF_FILTER_LINEAR_MIPMAP_LINEAR 9987

static VkFilter gltf_filter_to_vk_filter(cgltf_int filter) {
    switch (filter)
    {
    case GLTF_FILTER_NEAREST:
    case GLTF_FILTER_NEAREST_MIPMAP_NEAREST:
    case GLTF_FILTER_NEAREST_MIPMAP_LINEAR:
        return VK_FILTER_NEAREST;

    case GLTF_FILTER_LINEAR:
    case GLTF_FILTER_LINEAR_MIPMAP_NEAREST:
    case GLTF_FILTER_LINEAR_MIPMAP_LINEAR:
        return VK_FILTER_LINEAR;

    default:
        return VK_FILTER_NEAREST;
    }
}

static VkSamplerMipmapMode gltf_filter_to_vk_mipmap_mode(cgltf_int filter) {
    switch (filter)
    {
    case GLTF_FILTER_NEAREST_MIPMAP_NEAREST:
    case GLTF_FILTER_LINEAR_MIPMAP_NEAREST:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    
    case GLTF_FILTER_NEAREST_MIPMAP_LINEAR:
    case GLTF_FILTER_LINEAR_MIPMAP_LINEAR:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;

    default:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

// NOTE: This is still here as I am unsure how cgltf deals with indexed attributes, TEXCOORD_0, TEXCOORD_1.
static cgltf_accessor* get_accessor_from_attributes(cgltf_attribute* attributes, cgltf_size attributes_count, const char* name) {
    for (cgltf_size i = 0; i < attributes_count; ++i) {
        if (strs_equal(attributes[i].name, name)) {
            return attributes[i].data;
        }
    }
    return (void*)0;
}

    // TEMP:HACK: Temporary cpu skinning of meshes in order to get a handle on things
    // for (u32 i = 0; i < data->skins_count; ++i) {
    //     // Fill joint mats with inverse bind matrices
    //     u32 float_count = cgltf_accessor_unpack_floats(data->skins[i].inverse_bind_matrices, NULL, 0);
    //     m4s* joint_mats = etallocate(sizeof(f32) * float_count, MEMORY_TAG_IMPORTER);
    //     cgltf_accessor_unpack_floats(data->skins[i].inverse_bind_matrices, (f32*)joint_mats, float_count);

    //     for (u32 j = 0; j < data->skins[i].joints_count; ++j) {
    //         u32 joint_node_index = node_start + cgltf_node_index(data, data->skins[i].joints[j]);
    //         import_node* joint_node = &payload->nodes[joint_node_index];
    //         joint_mats[j] = glms_mat4_mul(joint_node->world_transform, joint_mats[j]);
    //     }

    //     u32 mesh_index;
    //     for (u32 j = 0; j < data->nodes_count; ++j) {
    //         if (data->nodes[j].skin) {
    //             u32 skin_index = cgltf_skin_index(data, data->nodes[j].skin);
    //             if (skin_index == i) mesh_index = cgltf_mesh_index(data, data->nodes[j].mesh);
    //         }
    //     }
    //     import_mesh* mesh = &payload->meshes[mesh_start + mesh_index];
    //     cgltf_mesh* gltf_mesh = &data->meshes[mesh_index];
    //     for (u32 j = 0; j < gltf_mesh->primitives_count; j++) {
    //         cgltf_primitive* primitive = &gltf_mesh->primitives[j];
    //         cgltf_accessor* joints = get_accessor_from_attributes(primitive->attributes, primitive->attributes_count, "JOINTS_0");
    //         cgltf_accessor* weights = get_accessor_from_attributes(primitive->attributes, primitive->attributes_count, "WEIGHTS_0");
            
    //         u32 vertex_count = dynarray_length(payload->geometries[mesh->geometry_indices[j]].vertices);

    //         iv4s* v_joints = etallocate(sizeof(iv4s) * vertex_count, MEMORY_TAG_IMPORTER);
    //         for (u32 k = 0; k < vertex_count; ++k) {
    //             cgltf_accessor_read_uint(joints, k, (u32*)&v_joints[k], sizeof(u32));
    //         }

    //         u32 weight_float_count = cgltf_accessor_unpack_floats(weights, NULL, 0);
    //         v4s* v_weights = etallocate(sizeof(f32) * weight_float_count, MEMORY_TAG_IMPORTER);
    //         cgltf_accessor_unpack_floats(weights, (f32*)v_weights, weight_float_count);

    //         for (u32 k = 0; k < vertex_count; ++k) {
    //             m4s m0 = glms_mat4_scale(joint_mats[(u32)v_joints[k].raw[0]], v_weights[k].raw[0]);
    //             m4s m1 = glms_mat4_scale(joint_mats[(u32)v_joints[k].raw[1]], v_weights[k].raw[1]);
    //             m4s m2 = glms_mat4_scale(joint_mats[(u32)v_joints[k].raw[2]], v_weights[k].raw[2]);
    //             m4s m3 = glms_mat4_scale(joint_mats[(u32)v_joints[k].raw[3]], v_weights[k].raw[3]);

    //             m4s m = mat4s_add4(m0, m1, m2, m3);

    //             v3s new_pos = glms_mat4_mulv3(m, payload->geometries[mesh->geometry_indices[j]].vertices[k].position, 1.0f);
    //             payload->geometries[mesh->geometry_indices[j]].vertices[k].position = new_pos;
    //         }

    //         etfree(v_joints, sizeof(iv4s) * vertex_count, MEMORY_TAG_IMPORTER);
    //         etfree(v_weights, sizeof(f32) * weight_float_count, MEMORY_TAG_IMPORTER);
    //     }

    //     etfree(joint_mats, sizeof(f32) * float_count, MEMORY_TAG_IMPORTER);
    // }
    // TEMP:HACK: Temporary cpu skinning of meshes in order to get a handle on things