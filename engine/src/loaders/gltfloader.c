#include "gltfloader.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include "core/logger.h"
#include "core/etstring.h"

// TODO: Store and use the names provided by the gltf file

// NOTE: Currenly leaks memory on failure to load gltf file
// TODO: Create a function to call when loading a file fails
// that checks for memory allocations and frees them before returning

// TODO:TEMP: Loader should not need to know the renderer implementation details
#include "renderer/src/utilities/vkinit.h"
#include "renderer/src/renderer.h"

#include "renderer/src/descriptor.h"
#include "renderer/src/buffer.h"

#include "renderer/src/GLTFMetallic_Roughness.h"

#include "renderer/src/renderables.h"
// TODO:TEMP: END

// TODO: Replace use of this macro with cgltf functions for getting index
/** HELPER FOR CGLTF
 * Macro to calculate the index into the backing array of a given pointer
 * @param type the type the pointer is referencing
 */
#define CGLTF_ARRAY_INDEX(type, array, pointer) (((u64)(pointer) - (u64)(array))/sizeof(type))

static void recurse_print_nodes(cgltf_node* node, u32 depth, u64* node_count);
static cgltf_accessor* get_accessor_from_attributes(cgltf_attribute* attributes, cgltf_size attributes_count, const char* name);

static VkFilter gltf_filter_to_vk_filter(cgltf_int filter);
static VkSamplerMipmapMode gltf_filter_to_vk_mipmap_mode(cgltf_int filter);

mesh_asset* load_gltf_meshes(const char* path, struct renderer_state* state) {
    cgltf_options options = {0};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, path, &data);
    if (result != cgltf_result_success) {
        ETERROR("Error loading gltf file %s.", path);
        return (void*)0;
    }

    result = cgltf_load_buffers(&options, data, 0);
    if (result != cgltf_result_success) {
        ETERROR("Error loading gltf file %s's buffers.", path);
        cgltf_free(data);
        return (void*)0;
    }

    mesh_asset* meshes = dynarray_create(data->meshes_count, sizeof(mesh_asset));

    // Use the same vectors for all meshes so that the memory doesnt 
    // reallocate as often
    vertex* vertices = dynarray_create(1, sizeof(vertex));
    u32* indices = dynarray_create(1, sizeof(u32));
    for (u32 i = 0; i < data->meshes_count; i++) {
        // Examine each mesh
        cgltf_mesh* mesh = &data->meshes[i];
        
        mesh_asset new_mesh;

        new_mesh.name = string_duplicate_allocate(mesh->name);
        new_mesh.surfaces = dynarray_create(mesh->primitives_count, sizeof(geo_surface));

        dynarray_clear(indices);
        dynarray_clear(vertices);

        // Examine each primitive in each mesh
        for (u32 j = 0; j < mesh->primitives_count; ++j) {
            cgltf_primitive* primitive = &mesh->primitives[j];

            geo_surface new_surface;
            new_surface.start_index = (u32)dynarray_length(indices);
            new_surface.count = (u32)primitive->indices->count;

            u32 initial_vertex = dynarray_length(vertices);

            // NOTE: elements are pushed to indices and vertices after a resize
            // TODO: Use dynarray reserve or don't use dynarray push

            // load indices
            cgltf_accessor* index_accessor = primitive->indices;
            dynarray_reserve((void**)&indices, dynarray_length(indices) + index_accessor->count);

            for (u32 k = 0; k < index_accessor->count; ++k) {
                u32 index = initial_vertex + cgltf_accessor_read_index(index_accessor, k);
                dynarray_push((void**)&indices, &index);
            }

            // load vertex positions
            cgltf_accessor* position = get_accessor_from_attributes(
                primitive->attributes, primitive->attributes_count, "POSITION");
            if (position) {
                dynarray_resize((void**)&vertices, dynarray_length(vertices) + position->count);
                cgltf_size element_size = cgltf_calc_size(position->type, position->component_type);
                for (u32 k = 0; k < position->count; k++) {
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
                    vertices[initial_vertex + k] = new_v;
                }
            } else {
                ETERROR("Attempted to load mesh without vertex positions");
                dynarray_destroy(meshes);
                return 0;
            }

            // load vertex normals
            cgltf_accessor* normal = get_accessor_from_attributes(
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
            cgltf_accessor* uv = get_accessor_from_attributes(
                primitive->attributes, primitive->attributes_count, "TEXCOORD_0");
            if (uv) {
                cgltf_size element_size = cgltf_calc_size(uv->type, uv->component_type);
                for (u32 k = 0; k < uv->count; ++k) {
                    vertex* v = &vertices[initial_vertex + k];
                    v2s uvs = {.raw = {0, 0}};
                    if (!cgltf_accessor_read_float(uv, k, uvs.raw, element_size)) {
                        ETERROR("Error attempting to read uv value from %s.", path);
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
            cgltf_accessor* color = get_accessor_from_attributes(
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
        bool override_colors = false;
        if (override_colors) {
            for (u32 i = 0; i < dynarray_length(vertices); ++i) {
                vertices[i].color.r = vertices[i].normal.r;
                vertices[i].color.g = vertices[i].normal.g;
                vertices[i].color.b = vertices[i].normal.b;
            }
        }
        // Upload mesh data to the GPU
        new_mesh.mesh_buffers = upload_mesh(state,
            dynarray_length(indices), indices,
            dynarray_length(vertices), vertices);

        // Place the new mesh into the meshes dynamic array
        dynarray_push((void**)&meshes, &new_mesh);

        u32 max_index_value = 0;
        for (u32 i = 0; i < dynarray_length(indices); ++i) {
            max_index_value = (max_index_value < indices[i]) ? indices[i] : max_index_value;
        }
        ETINFO("Vertex count: %lu", dynarray_length(vertices));
        ETINFO("Max index value: %lu", max_index_value);
    }

    dynarray_destroy(vertices);
    dynarray_destroy(indices);

    cgltf_free(data);
    return meshes;
}

// TODO: Use cgltf_attribute_type instead to find the accessor instead
static cgltf_accessor* get_accessor_from_attributes(cgltf_attribute* attributes, cgltf_size attributes_count, const char* name) {
    for (cgltf_size i = 0; i < attributes_count; ++i) {
        if (strings_equal(attributes[i].name, name)) {
            return attributes[i].data;
        }
    }
    return (void*)0;
}

// TODO: Handle texture_view implementation here (cgltf_texture_view)
void load_gltf(struct loaded_gltf* gltf, const char* path, struct renderer_state* state) {
    gltf->render_state = state;
    
    // Attempt to load gltf file data with cgltf library
    cgltf_options options = {0};
    cgltf_data* data = 0;
    cgltf_result result = cgltf_parse_file(&options, path, &data);
    if (result != cgltf_result_success) {
        ETERROR("Failed to load gltf file %s.", path);
        return;
    }

    // Attempt to load the data from buffers in
    result = cgltf_load_buffers(&options, data, 0);
    if (result != cgltf_result_success) {
        ETERROR("Failed to load gltf file %s.", path);
        cgltf_free(data);
        return;
    }

    // Descriptor Allocator for gltf
    pool_size_ratio ratios[] = {
        {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .ratio = 3},
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .ratio = 3},
        {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .ratio = 1}
    };
    pool_size_ratio* dynarray_ratios = 
        dynarray_create_data(3, sizeof(pool_size_ratio), 3, ratios);
    descriptor_set_allocator_growable_initialize(
        &gltf->descriptor_allocator,
        data->materials_count,
        dynarray_ratios,
        state);
    dynarray_destroy(dynarray_ratios);

    // Create samplers
    gltf->samplers = etallocate(sizeof(VkSampler) * data->samplers_count, MEMORY_TAG_RENDERABLE);
    for (u32 i = 0; i < data->samplers_count; ++i) {
        cgltf_sampler* i_sampler = (data->samplers + i);
        VkSamplerCreateInfo sampler_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = 0,
            .maxLod = VK_LOD_CLAMP_NONE,
            .minLod = 0,
            .magFilter = gltf_filter_to_vk_filter(i_sampler->mag_filter),
            .minFilter = gltf_filter_to_vk_filter(i_sampler->min_filter),
            .mipmapMode = gltf_filter_to_vk_mipmap_mode(i_sampler->min_filter)};
        
        vkCreateSampler(
            state->device.handle,
            &sampler_info,
            state->allocator,
            &gltf->samplers[i]);
    }
    gltf->sampler_count = data->samplers_count;

    // TODO: Load actual images. For now: defaults from renderer
    gltf->images = etallocate(sizeof(image) * data->images_count, MEMORY_TAG_RENDERABLE);
    for (u32 i = 0; i < data->images_count; ++i) {
        gltf->images[i] = state->error_checkerboard_image;
    }
    gltf->image_count = data->images_count;

    // Load materials and material data:
    // Create buffer for holding material data. Constants and such
    buffer_create(state,
        sizeof(struct material_constants) * data->materials_count,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &gltf->material_data_buffer
    );
    void* mapped_memory;
    vkMapMemory(
        state->device.handle,
        gltf->material_data_buffer.memory,
        /* offset: */ 0,
        sizeof(struct material_constants) * data->materials_count,
        /* flags: */ 0,
        &mapped_memory);
    struct material_constants* material_constants = (struct material_constants*)mapped_memory;
    
    // Allocate memory for storing materials
    gltf->materials = etallocate(
        sizeof(GLTF_material) * data->materials_count,
        MEMORY_TAG_RENDERABLE);

    // Create Materials
    for (u32 i = 0; i < data->materials_count; ++i) {
        cgltf_material* i_material = (data->materials + i);

        // TODO: Check for cgltf_pbr_metallic_roughness beforehand
        // if not present use some default values
        cgltf_pbr_metallic_roughness gltf_pbr_mr = i_material->pbr_metallic_roughness;
        struct material_constants constants = {
            .color_factors = {
                .r = gltf_pbr_mr.base_color_factor[0],
                .g = gltf_pbr_mr.base_color_factor[1],
                .b = gltf_pbr_mr.base_color_factor[2],
                .a = gltf_pbr_mr.base_color_factor[3],
            },
            .metal_rough_factors = {
                .x = gltf_pbr_mr.metallic_factor,
                .y = gltf_pbr_mr.roughness_factor
            }
        };
        material_constants[i] = constants;

        material_pass pass_type = MATERIAL_PASS_MAIN_COLOR;
        if (i_material->alpha_mode == cgltf_alpha_mode_blend) {
            pass_type = MATERIAL_PASS_TRANSPARENT;
        }

        // Create material resources with defaults
        struct material_resources material_resources = {
            .color_image = state->white_image,
            .color_sampler = state->default_sampler_linear,
            .metal_rough_image = state->white_image,
            .metal_rough_sampler = state->default_sampler_linear,

            .data_buffer = gltf->material_data_buffer.handle,
            .data_buffer_offset = sizeof(struct material_constants) * i
        };

        if (i_material->has_pbr_metallic_roughness &&
            i_material->pbr_metallic_roughness.base_color_texture.texture)
        {
            cgltf_texture* tex = i_material->pbr_metallic_roughness.base_color_texture.texture;
            u64 img_index = CGLTF_ARRAY_INDEX(cgltf_image, data->images, tex->image);
            u64 smpl_index = CGLTF_ARRAY_INDEX(cgltf_sampler, data->samplers, tex->sampler);

            material_resources.color_image = gltf->images[img_index];
            material_resources.color_sampler = gltf->samplers[smpl_index];
        }

        // Write the material instance
        gltf->materials[i].data = GLTF_MR_write_material(
            &state->metal_rough_material,
            state,
            pass_type,
            &material_resources,
            &gltf->descriptor_allocator);
    }
    vkUnmapMemory(state->device.handle, gltf->material_data_buffer.memory);
    gltf->material_count = data->materials_count;

    // Allocate memory for meshes in loaded_gltf
    gltf->meshes = etallocate(sizeof(mesh_asset) * data->meshes_count, MEMORY_TAG_RENDERABLE);
    
    vertex* vertices = dynarray_create(1, sizeof(vertex));
    u32* indices = dynarray_create(1, sizeof(u32));
    for (u32 i = 0; i < data->meshes_count; ++i) {
        cgltf_mesh* mesh = &data->meshes[i];

        mesh_asset* new_mesh = &gltf->meshes[i];

        new_mesh->name = string_duplicate_allocate(mesh->name);

        // Allocate memory for mesh surfaces:
        // TODO: Store the amount of surfaces and use that instead of dynarray
        // I want to use dynarrays when they are needed. Not just convinient
        // new_mesh->surfaces = etallocate(
        //     mesh->primitives_count * sizeof(geo_surface),
        //     MEMORY_TAG_RENDERABLE);
        new_mesh->surfaces = dynarray_create(
            mesh->primitives_count,
            sizeof(geo_surface));
        dynarray_length_set(new_mesh->surfaces, mesh->primitives_count);

        dynarray_clear(indices);
        dynarray_clear(vertices);

        for (u32 j = 0; j < mesh->primitives_count; ++j) {
            cgltf_primitive* primitive = &mesh->primitives[j];

            geo_surface* new_surface = &new_mesh->surfaces[j];
            new_surface->start_index = dynarray_length(indices);
            new_surface->count = primitive->indices->count;

            u64 initial_vertex = dynarray_length(vertices);

            // load indices
            cgltf_accessor* index_accessor = primitive->indices;
            dynarray_reserve((void**)&indices, dynarray_length(indices) + index_accessor->count);

            for (u32 k = 0; k < index_accessor->count; ++k) {
                u32 index = initial_vertex + cgltf_accessor_read_index(index_accessor, k);
                dynarray_push((void**)&indices, &index);
            }

            // Load position information
            cgltf_accessor* position = get_accessor_from_attributes(
                primitive->attributes, primitive->attributes_count, "POSITION");
            if (!position) {
                ETERROR("Attempted to load mesh without vertex positions.");
                return;
            }

            dynarray_resize((void**)&vertices, dynarray_length(vertices) + position->count);
            cgltf_size pos_element_size = cgltf_calc_size(position->type, position->component_type);
            for (u32 k = 0; k < position->count; k++) {
                vertex new_v = {
                    .position = (v3s){.raw = {0.f, 0.f, 0.f}},
                    .normal = (v3s){.raw = {1.f, 0.f, 0.f}},
                    .color = (v4s){.raw = {1.f, 1.f, 1.f, 1.f}},
                    .uv_x = 0,
                    .uv_y = 0,
                };
                
                if (!cgltf_accessor_read_float(position, k, new_v.position.raw, pos_element_size)) {
                    ETERROR("Error attempting to read position value from %s.", path);
                    return;
                }
                vertices[initial_vertex + k] = new_v;
            }

            // Load normal information
            cgltf_accessor* normal = get_accessor_from_attributes(
                primitive->attributes, primitive->attributes_count, "NORMAL");
            if (!normal) {
                ETERROR("Attempted to load mesh without vertex normals.");
                return;
            }

            cgltf_size norm_element_size = cgltf_calc_size(normal->type, normal->component_type);
            for (u32 k = 0; k < normal->count; ++k) {
                vertex* v = &vertices[initial_vertex + k];
                if (!cgltf_accessor_read_float(normal, k, v->normal.raw, norm_element_size)) {
                    ETERROR("Error attempting to read normal value from %s.", path);
                    return;
                }
            }

            // Load texture coordinates, UVs
            cgltf_accessor* uv = get_accessor_from_attributes(
                primitive->attributes, primitive->attributes_count, "TEXCOORD_0");
            if (!uv) {
                ETERROR("Attempted to load mesh without vertex texture coords.");
                return;
            }

            cgltf_size uv_element_size = cgltf_calc_size(uv->type, uv->component_type);
            for (u32 k = 0; k < uv->count; ++k) {
                vertex* v = &vertices[initial_vertex + k];

                v2s uvs = {.raw = {0.f, 0.f}};
                if (!cgltf_accessor_read_float(uv, k, uvs.raw, uv_element_size)) {
                    ETERROR("Error attempting to read uvs from %s.", path);
                    return;
                }

                v->uv_x = uvs.x;
                v->uv_y = uvs.y;
            }

            // Load vertex color information
            cgltf_accessor* color = get_accessor_from_attributes(
                primitive->attributes, primitive->attributes_count, "COLOR_0");
            if (color) {
                cgltf_size element_size = cgltf_calc_size(color->type, color->component_type);
                for (u32 k = 0; k < color->count; ++k) {
                    vertex* v = &vertices[initial_vertex + k];
                    if (!cgltf_accessor_read_float(color, k, v->color.raw, element_size)) {
                        ETERROR("Error attempting to read color value from %s.", path);
                        return;
                    }
                }
            }

            // Set material
            if (primitive->material) {
                u64 mat_index = CGLTF_ARRAY_INDEX(cgltf_material, data->materials, primitive->material);
                new_surface->material = &gltf->materials[mat_index];
            } else {
                new_surface->material = &gltf->materials[0];
            }
        }
        new_mesh->mesh_buffers = upload_mesh(state,
            dynarray_length(indices), indices,
            dynarray_length(vertices), vertices
        );
    }
    dynarray_destroy(vertices);
    dynarray_destroy(indices);
    gltf->mesh_count = data->meshes_count;

    // Count up needed mesh_nodes & needed nodes. Then allocate backing memory for them
    // TODO: Find a gltf with a non mesh node and see if it still works
    u32 node_count = 0;
    u32 mesh_node_count = 0;
    for (u32 i = 0; i < data->nodes_count; ++i) {
        if (data->nodes[i].mesh) {
            mesh_node_count++;
        } else {
            node_count++;
        }
    }

    // Calling malloc, which etallocate currently does, can have undefined behavoir
    if (mesh_node_count) {
        gltf->mesh_nodes = etallocate(sizeof(mesh_node) * mesh_node_count, MEMORY_TAG_RENDERABLE);
        for (u32 i = 0; i < mesh_node_count; ++i) {
            mesh_node_create(&gltf->mesh_nodes[i]);
        }
    } else gltf->mesh_nodes = 0;
    if (node_count) {
        gltf->nodes = etallocate(sizeof(node) * node_count, MEMORY_TAG_RENDERABLE);
        for (u32 i = 0; i < node_count; ++i) {
            node_create(&gltf->nodes[i]);
        }
    } else gltf->nodes = 0;
    
    node** nodes_by_index = etallocate(sizeof(node*) * data->nodes_count, MEMORY_TAG_RENDERABLE);
    for (u32 m_node_i = 0, node_i = 0; (m_node_i + node_i) < data->nodes_count;) {
        u32 i = m_node_i + node_i;

        node* node;
        if (data->nodes[i].mesh) {
            mesh_node* m_node = &gltf->mesh_nodes[m_node_i];
            u32 mesh_index = CGLTF_ARRAY_INDEX(cgltf_mesh, data->meshes, data->nodes[i].mesh);
            m_node->mesh = &gltf->meshes[mesh_index];
            node = node_from_mesh_node(m_node);
            m_node_i++;
        } else {
            node = &gltf->nodes[node_i];
            node_i++;
        }

        // TODO: Test if working with a gltf file that has transforms
        cgltf_node_transform_local(&data->nodes[i], (cgltf_float*)node->local_transform.raw);
        cgltf_node_transform_world(&data->nodes[i], (cgltf_float*)node->world_transform.raw);

        // Store the node address in the nodes_by_index to grab a reference by index when
        // setting up the node hierarchy
        nodes_by_index[i] = node;
    }

    // Connect nodes with children and parents
    gltf->top_nodes = dynarray_create(1, sizeof(node*));
    for (u32 i = 0; i < data->nodes_count; ++i) {
        node* node = nodes_by_index[i];

        dynarray_resize((void**)&node->children, data->nodes[i].children_count);
        for (u32 j = 0; j < data->nodes[i].children_count; ++j) {
            u32 child_node_index = CGLTF_ARRAY_INDEX(cgltf_node, data->nodes, data->nodes[i].children[j]);
            dynarray_push((void**)&node->children, &nodes_by_index[child_node_index]);
        }
        if (data->nodes[i].parent) {
            node->parent = nodes_by_index[CGLTF_ARRAY_INDEX(cgltf_node, data->nodes, data->nodes[i].parent)];
        }
        // No parent means top level node
        else {
            dynarray_push((void**)&gltf->top_nodes, &nodes_by_index[i]);
        }
    }
    etfree(nodes_by_index, sizeof(node*) * data->nodes_count, MEMORY_TAG_RENDERABLE);
    gltf->mesh_node_count = mesh_node_count;
    gltf->node_count = node_count;
    gltf->top_node_count = dynarray_length(gltf->top_nodes);
    for (u32 i = 0; i < gltf->top_node_count; ++i) {
        node_refresh_transform(gltf->top_nodes[i], glms_mat4_identity());
    }

    // Clean up memory
    cgltf_free(data);
}

void unload_gltf(struct loaded_gltf* gltf) {
    dynarray_destroy(gltf->top_nodes);

    /** TODO: Destroy and free memory for:
     * images
     * samplers
     * materials
     * material_data_buffer
     * meshes
     * 
     * mesh_nodes
     * nodes
     * 
     * descriptor_allocator
     */
    
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