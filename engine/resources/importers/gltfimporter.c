#include "gltfimporter.h"
#include "importer_types.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "data_structures/dynarray.h"

#include "core/logger.h"
#include "core/asserts.h"
#include "core/etstring.h"
#include "core/etfile.h"

#include "memory/etmemory.h"
#include "renderer/src/vk_types.h"

// NOTE: Currenly leaks memory on failure to load gltf file
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

/** TODO: Next:
 * Put vertex data into single vertex buffer on import
 * Put index data into single index buffer on import
 * Assign material data pipeline ids and instance ids on import
 * Create transform buffer from nodes on import
 * 
 * After creating scene from payload
 * Import animations and joints and skins and stuff
 * 
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
        image->name = (data->images[i].name) ? str_duplicate_allocate(data->images[i].name) : str_duplicate_allocate("COCK");
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

    // TODO: Deserialize all files set to be imported, check the material 
    // specs for each one and place all the deafult pipelines needed into 
    // the payload before calling this function.  
    i32 opaque_index = -1;
    i32 transparent_index = -1;
    u32 pipeline_count = dynarray_length(payload->pipelines);
    for (u32 i = 0; i < pipeline_count; ++i) {
        switch (payload->pipelines[i].type) {
            case IMPORT_PIPELINE_TYPE_GLTF_TRANSPARENT:
                transparent_index = (i32)i;
                break;
            case IMPORT_PIPELINE_TYPE_GLTF_DEFAULT: {
                opaque_index = (i32)i;
                break;
            }
        }
    }
    if (opaque_index == -1) {
        import_pipeline gltf_default = default_import_pipelines[IMPORT_PIPELINE_TYPE_GLTF_DEFAULT];
        gltf_default.instances = dynarray_create(0, gltf_default.inst_size);
        dynarray_push((void**)&payload->pipelines, &gltf_default);
        opaque_index = (i32)pipeline_count++;
    }
    if (transparent_index == -1) {
        import_pipeline gltf_transparent = default_import_pipelines[IMPORT_PIPELINE_TYPE_GLTF_TRANSPARENT];
        gltf_transparent.instances = dynarray_create(0, gltf_transparent.inst_size);
        dynarray_push((void**)&payload->pipelines, &gltf_transparent);
        transparent_index = (i32)pipeline_count++;
    }
    // TODO: END

    u32 mat_index_id_offset = dynarray_grow((void**)&payload->mat_index_to_id, data->materials_count);
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
            .color_tex_index = DEFAULT_TEXTURE_WHITE,
            .metalness = mr.metallic_factor,
            .roughness = mr.roughness_factor,
            .mr_tex_index = DEFAULT_TEXTURE_WHITE,
            .normal_index = DEFAULT_TEXTURE_NORMAL,
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
                id.pipe_id = transparent_index;
                id.inst_id = dynarray_length(payload->pipelines[transparent_index].instances);
                dynarray_push((void**)&payload->pipelines[transparent_index].instances, &instance);
                break;
            default:
                ETWARN("Unknown alpha mode for material %lu in gltf file %s. Setting opaque.", i, path);
            case cgltf_alpha_mode_mask:
            case cgltf_alpha_mode_opaque: {
                id.pipe_id = opaque_index;
                id.inst_id = dynarray_length(payload->pipelines[opaque_index].instances);
                dynarray_push((void**)&payload->pipelines[opaque_index].instances, &instance);
                break;
            }
        }
        
        payload->mat_index_to_id[mat_index_id_offset + i] = id;
    }

    // TODO: Put vertex data into single vertex buffer & index data 
    // into single index buffer in this function.
    u32 mesh_start = dynarray_grow((void**)&payload->meshes, data->meshes_count);    
    for (u32 i = 0; i < data->meshes_count; ++i) {
        import_mesh* mesh = &payload->meshes[mesh_start + i];
        mesh->count = data->meshes[i].primitives_count;
        
        mesh->geometry_indices = dynarray_create(mesh->count, sizeof(u32));
        dynarray_resize((void**)&mesh->geometry_indices, mesh->count);

        mesh->material_indices = dynarray_create(mesh->count, sizeof(u32));
        dynarray_resize((void**)&mesh->material_indices, mesh->count);
        
        u32 geo_start = dynarray_grow((void**)&payload->geometries, mesh->count);

        for (u32 j = 0; j < data->meshes[i].primitives_count; ++j) {
            cgltf_primitive prim = data->meshes[i].primitives[j];
            import_geometry* geo = &payload->geometries[geo_start + j];

            // NOTE: Converts u16 indices to u32 indices
            geo->indices = dynarray_create(prim.indices->count, sizeof(u32));
            dynarray_resize((void**)&geo->indices, prim.indices->count);
            cgltf_accessor_unpack_indices(prim.indices, geo->indices, sizeof(u32), prim.indices->count);
            
            u32 vertex_count = prim.attributes[0].data->count;
            geo->vertices = dynarray_create(vertex_count, sizeof(vertex));
            dynarray_resize((void**)&geo->vertices, vertex_count);
            for (u32 i = 0; i < vertex_count; ++i) {
                geo->vertices[i] = (vertex) {
                    .position = (v3s) {0, 0, 0},
                    .normal = (v3s) {1.0f, 0, 0},
                    .uv_x = 0.f,
                    .uv_y = 0.f,
                    .color = (v4s) { 1.f, 1.f, 1.f, 1.f},
                };
            }
            
            f32* accessor_data = dynarray_create(1, sizeof(f32));
            for (u32 k = 0; k < prim.attributes_count; ++k) {
                u64 float_count = cgltf_accessor_unpack_floats(prim.attributes[k].data, NULL, 0);
                dynarray_resize((void**)&accessor_data, float_count);
                cgltf_accessor_unpack_floats(prim.attributes[k].data, accessor_data, float_count);
                switch (prim.attributes[k].type) {
                    case cgltf_attribute_type_position:
                        v3s* positions = (v3s*)accessor_data;
                        for (u32 l = 0; l < prim.attributes[k].data->count; l++) {
                            geo->vertices[l].position = positions[l];
                        }
                        break;
                    case cgltf_attribute_type_normal:
                        v3s* normals = (v3s*)accessor_data;
                        for (u32 l = 0; l < prim.attributes[k].data->count; l++) {
                            geo->vertices[l].normal = normals[l];
                        }
                        break;
                    case cgltf_attribute_type_texcoord:
                        v2s* uvs = (v2s*)accessor_data;
                        for (u32 l = 0; l < prim.attributes[k].data->count; l++) {
                            geo->vertices[l].uv_x = uvs[l].x;
                            geo->vertices[l].uv_y = uvs[l].y;
                        }
                        break;
                    case cgltf_attribute_type_color:
                        v4s* colors = (v4s*)accessor_data;
                        for (u32 l = 0; l < prim.attributes[k].data->count; l++) {
                            geo->vertices[l].color = colors[l];
                        }
                        break;
                    // TODO: Implement
                    case cgltf_attribute_type_tangent: break;
                    case cgltf_attribute_type_joints: break;
                    case cgltf_attribute_type_weights: break;
                    case cgltf_attribute_type_custom: break;
                    // TODO: END
                    default: {
                        ETWARN("Unknown gltf vertex attribute type.");
                        break;
                    }
                }
            }
            dynarray_destroy(accessor_data);

            // TODO: Frustum culling is causing pop-in
            v4s min_pos = glms_vec4(geo->vertices[0].position, 0.0f);
            v4s max_pos = glms_vec4(geo->vertices[0].position, 0.0f);
            for (u32 k = 0; k < vertex_count; ++k) {
                min_pos = glms_vec4_minv(min_pos, glms_vec4(geo->vertices[k].position, 0.0f));
                max_pos = glms_vec4_maxv(max_pos, glms_vec4(geo->vertices[k].position, 0.0f));
            }

            geo->origin = glms_vec4_scale(glms_vec4_add(max_pos, min_pos), 0.5f);
            geo->extent = glms_vec4_scale(glms_vec4_sub(max_pos, min_pos), 0.5f);
            geo->radius = glms_vec3_norm(glms_vec3(geo->extent));
            // TODO: END

            ETDEBUG("max_pos: x, y, z | %lf %lf %lf", max_pos.x, max_pos.y, max_pos.z);
            ETDEBUG("min_pos: x, y, z | %lf %lf %lf", min_pos.x, min_pos.y, min_pos.z);

            mesh->geometry_indices[j] = geo_start + j;
            // TODO: When pipelines are placed before this function, we can store the pipeline_id, instance_id combo here
            mesh->material_indices[j] = (prim.material) ? mat_index_id_offset + cgltf_material_index(data, prim.material) : mat_index_id_offset;
            // TODO: END
        }
    }
    // TODO: END

    // TODO: Create transform buffer from node transforms, just mesh nodes for now.
    u32 node_start = dynarray_grow((void**)&payload->nodes, data->nodes_count);
    for (u32 i = 0; i < data->nodes_count; ++i) {
        import_node node = {0};
        if (data->nodes[i].mesh != NULL) {
            node.has_mesh = true;
            node.mesh_index = mesh_start + cgltf_mesh_index(data, data->nodes[i].mesh);
        }
        if (data->nodes[i].parent != NULL) {
            node.has_parent = true;
            node.parent_index = node_start + cgltf_node_index(data, data->nodes[i].parent);
        }

        node.children_indices = dynarray_create(data->nodes[i].children_count, sizeof(u32));
        dynarray_resize((void**)&node.children_indices, data->nodes[i].children_count);
        for (u32 j = 0; j < data->nodes[i].children_count; ++j) {
            node.children_indices[j] = node_start + cgltf_node_index(data, data->nodes[i].children[j]);
        }

        cgltf_node_transform_local(&data->nodes[i], (cgltf_float*)node.local_transform.raw);
        cgltf_node_transform_world(&data->nodes[i], (cgltf_float*)node.world_transform.raw);

        payload->nodes[node_start + i] = node;
    }
    // TODO: END

    // TODO: Import Animation data & such

    return true;
}

b8 dump_gltf_json(const char* gltf_path, const char* dump_file_path) {
    cgltf_options options = {0};
    cgltf_data* data = 0;
    cgltf_result result = cgltf_parse_file(&options, gltf_path, &data);
    if (result != cgltf_result_success) {
        ETERROR("Failed to load gltf file %s.", gltf_path);
        return false;
    }

    etfile* json_file = NULL;
    b8 file_result = file_open(dump_file_path, FILE_WRITE_FLAG, &json_file);
    if (!file_result) {
        ETERROR("%s failed to open.", dump_file_path);
        return false;
    }

    u64 bytes_written = 0;
    file_result = file_write(json_file, data->json_size, (void*)data->json, &bytes_written);
    if (!file_result) {
        ETERROR("Failed to write %s's json data to %s.", gltf_path, dump_file_path);
        return false;
    }

    file_close(json_file);
    ETINFO("Json from %s successfully dumped to %s", gltf_path, dump_file_path);
    return true;
}

// TODO: Have this return data to be processed by stbi_load_from_memory
// Image manager should handle loading in image memory
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