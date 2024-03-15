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

// NOTE: Currenly leaks memory on failure to load gltf file
// TODO: Create a function to call when importing a file fails
// that checks for memory allocations and frees them before returning

// TODO: Handle cases where parts of the gltf are missing/not present
// No materials or images, no tex coords, etc... 

#include "renderer/src/utilities/vkutils.h"
#include "renderer/src/renderer.h"

#include "renderer/src/buffer.h"
#include "renderer/src/image.h"

#include "resources/image_manager.h"
#include "scene/scene.h"
#include "scene/scene_private.h"

static void recurse_print_nodes(cgltf_node* node, u32 depth, u64* node_count);
static cgltf_accessor* get_accessor_from_attributes(cgltf_attribute* attributes, cgltf_size attributes_count, const char* name);

// Loads the image data from gltf. *data needs to be freed with stb_image_free(*data);
static void* load_image_data(cgltf_image* in_image, const char* gltf_path, int* width, int* height, int* channels);

static void gltf_combine_paths(char* path, const char* base, const char* uri);

static VkFilter gltf_filter_to_vk_filter(cgltf_int filter);
static VkSamplerMipmapMode gltf_filter_to_vk_mipmap_mode(cgltf_int filter);

// TODO: Handle loading other parts of gltf, like skeletal animation and cameras
b8 import_gltf(scene* scene, const char* path, renderer_state* state) {
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

    // Samplers
    scene->samplers = etallocate(sizeof(VkSampler) * data->samplers_count, MEMORY_TAG_SCENE);
    for (u32 i = 0; i < data->samplers_count; ++i) {
        cgltf_sampler* gltf_sampler = (data->samplers + i);
        VkSamplerCreateInfo sampler_info = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = 0,
            .maxLod = VK_LOD_CLAMP_NONE,
            .minLod = 0,
            .magFilter = gltf_filter_to_vk_filter(gltf_sampler->mag_filter),
            .minFilter = gltf_filter_to_vk_filter(gltf_sampler->min_filter),
            .mipmapMode = gltf_filter_to_vk_mipmap_mode(gltf_sampler->min_filter)};
        VK_CHECK(vkCreateSampler(
            state->device.handle,
            &sampler_info,
            state->allocator,
            &scene->samplers[i]
        ));
    }
    scene->sampler_count = data->samplers_count;

    for (u32 i = 0; i < data->images_count; ++i) {
        // TODO: Create an image loader 
        int width, height, channels;
        void* image_data = load_image_data(
            &data->images[i],
            path,
            &width,
            &height,
            &channels);
        if (image_data) {
            image2D_config config = {
                .name = data->images[i].name,
                .width = width,
                .height = height,
                .data = image_data};
            image2D_submit(scene->image_bank, &config);
            stbi_image_free(image_data);
        } else {
            image_manager_increment(scene->image_bank);
            ETERROR("Error loading image %s from gltf file %s. Using default.", data->images[i].name, path);
        }
    }

    // NOTE: Structure to convert a gltf file's material indices to my engines mat id system
    mat_id* material_index_map = dynarray_create(1, sizeof(mat_id));

    for (u32 i = 0; i < data->materials_count; ++i) {
        cgltf_material* gltf_material = (data->materials + i);

        cgltf_pbr_metallic_roughness gltf_pbr_mr = gltf_material->pbr_metallic_roughness;

        blinn_mr_ubo ubo_entry = {
            .color = {
                .r = gltf_pbr_mr.base_color_factor[0],
                .g = gltf_pbr_mr.base_color_factor[1],
                .b = gltf_pbr_mr.base_color_factor[2],
                .a = gltf_pbr_mr.base_color_factor[3],
            },
            .color_id = 0,  // Default
            .metalness = gltf_pbr_mr.metallic_factor,
            .roughness = gltf_pbr_mr.roughness_factor,
            .mr_id = 0,
        };

        if (gltf_material->has_pbr_metallic_roughness) {
            if (gltf_material->pbr_metallic_roughness.base_color_texture.texture) {
                cgltf_texture* color_tex = gltf_material->pbr_metallic_roughness.base_color_texture.texture;
                u64 tex_index = cgltf_texture_index(data, color_tex);
                u64 img_index = cgltf_image_index(data, color_tex->image);
                u64 smpl_index = cgltf_sampler_index(data, color_tex->sampler);

                scene_texture_set(scene, tex_index, img_index, smpl_index);
                ubo_entry.color_id = tex_index;
            }
            if (gltf_material->pbr_metallic_roughness.metallic_roughness_texture.texture) {
                cgltf_texture* mr_tex = gltf_material->pbr_metallic_roughness.metallic_roughness_texture.texture;
                u64 tex_index = cgltf_texture_index(data, mr_tex);
                u64 img_index = cgltf_image_index(data, mr_tex->image);
                u64 smpl_index = cgltf_sampler_index(data, mr_tex->sampler);
                
                scene_texture_set(scene, tex_index, img_index, smpl_index);
                ubo_entry.mr_id = tex_index;
            }
        }

        mat_pipe_type material_type;
        switch (gltf_material->alpha_mode)
        {
        case cgltf_alpha_mode_blend:
            material_type = MAT_PIPE_METAL_ROUGH_TRANSPARENT;
            break;
        case cgltf_alpha_mode_mask:
        case cgltf_alpha_mode_opaque:
            material_type = MAT_PIPE_METAL_ROUGH;
            break;
        }
        u32 pipe_id = (u32)material_type;
        u32 inst_id = mat_instance_create(&scene->materials[material_type], state, sizeof(blinn_mr_ubo), &ubo_entry);

        mat_id mat_inst = {
            .pipe_id = pipe_id,
            // HACK: If unable to create the material instance just use the first one
            .inst_id = (inst_id == INVALID_ID) ? 0 : inst_id,
        };
        dynarray_push((void**)&material_index_map, &mat_inst);
    }

    vertex* scene_vertices = dynarray_create(1, sizeof(vertex));
    u32* scene_indices = dynarray_create(1, sizeof(u32));

    surface* scene_surfaces = dynarray_create(1, sizeof(surface));
    mesh* scene_meshes = dynarray_create(1, sizeof(mesh));

    geometry* scene_geometries = dynarray_create(1, sizeof(geometry));

    u64 opaque_surface_count;
    u64 transparent_surface_count;
    
    for (u32 i = 0; i < data->meshes_count; ++i) {
        cgltf_mesh* gltf_mesh = &data->meshes[i];

        u32 surface_count = dynarray_length(scene_surfaces);
        dynarray_resize((void**)&scene_surfaces, surface_count + gltf_mesh->primitives_count);
        // TEMP: until multiple psos are working
        dynarray_resize((void**)&scene_geometries, surface_count + gltf_mesh->primitives_count);

        mesh new_mesh = {
            .vertex_offset = 0,
            .instance_count = 0,
            .start_surface = surface_count,
            .surface_count = gltf_mesh->primitives_count,
        };
        dynarray_push((void**)&scene_meshes, &new_mesh);

        for (u32 j = 0; j < gltf_mesh->primitives_count; ++j) {
            cgltf_primitive* gltf_primitive = &gltf_mesh->primitives[j];
            
            surface* new_surface = &scene_surfaces[surface_count + j];
            new_surface->start_index = dynarray_length(scene_indices);
            new_surface->index_count = gltf_primitive->indices->count;

            geometry* new_geometry = &scene_geometries[surface_count + j];
            new_geometry->start_index = dynarray_length(scene_indices);
            new_geometry->index_count = gltf_primitive->indices->count;
            // TODO: Take this into account when creating the new geometry
            new_geometry->vertex_offset = 0;


            u64 initial_vertex = dynarray_length(scene_vertices);
            cgltf_accessor* index_accessor = gltf_primitive->indices;
            dynarray_reserve((void**)&scene_indices, dynarray_length(scene_indices) + index_accessor->count);
            for (u32 k = 0; k < index_accessor->count; ++k) {
                u32 index = initial_vertex + cgltf_accessor_read_index(index_accessor, k);
                dynarray_push((void**)&scene_indices, &index);
            }

            // Load position information
            cgltf_accessor* position = get_accessor_from_attributes(
                gltf_primitive->attributes, gltf_primitive->attributes_count, "POSITION");
            if (!position) {
                ETERROR("Attempted to load mesh without vertex positions.");
                return false;
            }
            dynarray_resize((void**)&scene_vertices, dynarray_length(scene_vertices) + position->count);
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
                    return false;
                }
                scene_vertices[initial_vertex + k] = new_v;
            }

            // Load normal information
            cgltf_accessor* normal = get_accessor_from_attributes(
                gltf_primitive->attributes, gltf_primitive->attributes_count, "NORMAL");
            if (!normal) {
                ETERROR("Attempt to load mesh without vertex normals.");
                return false;
            }
            cgltf_size norm_element_size = cgltf_calc_size(normal->type, normal->component_type);
            for (u32 k = 0; k < normal->count; ++k) {
                vertex* v = &scene_vertices[initial_vertex + k];
                if (!cgltf_accessor_read_float(normal, k, v->normal.raw, norm_element_size)) {
                    ETERROR("Error attempting to read normal value from %s.", path);
                }
            }

            // Load texture coordinates, UVs
            cgltf_accessor* uv = get_accessor_from_attributes(
                gltf_primitive->attributes,
                gltf_primitive->attributes_count,
                "TEXCOORD_0");
            if (uv) {
                cgltf_size uv_element_size = cgltf_calc_size(uv->type, uv->component_type);
                for (u32 k = 0; k < uv->count; ++k) {
                    vertex* v = &scene_vertices[initial_vertex + k];

                    v2s uvs = {.raw = {0.f, 0.f}};
                    if (!cgltf_accessor_read_float(uv, k, uvs.raw, uv_element_size)) {
                        ETERROR("Error attempting to read uvs from %s.", path);
                        return false;
                    }

                    v->uv_x = uvs.x;
                    v->uv_y = uvs.y;
                }
            }
            
            // Load vertex color information
            cgltf_accessor* color = get_accessor_from_attributes(
                gltf_primitive->attributes, gltf_primitive->attributes_count, "COLOR_0");
            if (color) {
                cgltf_size element_size = cgltf_calc_size(color->type, color->component_type);
                for (u32 k = 0; k < color->count; ++k) {
                    vertex* v = &scene_vertices[initial_vertex + k];
                    if (!cgltf_accessor_read_float(color, k, v->color.raw, element_size)) {
                        ETERROR("Error attempting to read color value from %s.", path);
                        return false;
                    }
                }
            }
            u64 mat_index = (gltf_primitive->material) ? cgltf_material_index(data, gltf_primitive->material) : 0;
            new_surface->material = material_index_map[mat_index];
        }
    }

    // Gltf material indices have been converted to mat_id. So clean up
    dynarray_destroy(material_index_map);

    m4s** transforms_from_mesh_index = etallocate(sizeof(m4s*) * dynarray_length(scene_meshes), MEMORY_TAG_SCENE);
    for (u32 i = 0; i < dynarray_length(scene_meshes); ++i) {
        transforms_from_mesh_index[i] = dynarray_create(1, sizeof(m4s));
    }

    for (u32 i = 0; i < data->nodes_count; ++i) {
        if (data->nodes[i].mesh) {
            u32 mesh_index = cgltf_mesh_index(data, data->nodes[i].mesh);

            m4s new_transform = {0};
            cgltf_node_transform_world(&data->nodes[i], (cgltf_float*)new_transform.raw);

            dynarray_push((void**)(transforms_from_mesh_index + mesh_index), &new_transform);
            scene_meshes[mesh_index].instance_count++;
        }
    }

    object* scene_objects = dynarray_create(1, sizeof(object));
    m4s* scene_transforms = dynarray_create(1, sizeof(m4s));
    u32 transform_offset = 0;
    for (u32 i = 0; i < dynarray_length(scene_meshes); ++i) {
        for (u32 j = 0; j < scene_meshes[i].instance_count; ++j) {
            // Object representation for test
            for (u32 k = 0; k < scene_meshes[i].surface_count; ++k) {
                surface surface = scene_surfaces[scene_meshes[i].start_surface + k];
                object new_object = {
                    .geo_id = scene_meshes[i].start_surface + k,
                    .pso_id = surface.material.pipe_id,
                    .mat_id = surface.material.inst_id,
                    .transform_id = transform_offset + j,
                };
                dynarray_push((void**)&scene_objects, &new_object);
            }
            dynarray_push((void**)&scene_transforms, &transforms_from_mesh_index[i][j]);
        }
        dynarray_destroy(transforms_from_mesh_index[i]);
        scene_meshes[i].transform_offset = transform_offset;
        transform_offset += scene_meshes[i].instance_count;
    }

    etfree(transforms_from_mesh_index, sizeof(m4s*) * dynarray_length(scene_meshes), MEMORY_TAG_SCENE);

    scene->vertex_count = dynarray_length(scene_vertices);
    scene->index_count = dynarray_length(scene_indices);
    mesh_buffers scene_shared_buffers = upload_mesh_immediate(
        state,
        scene->index_count,
        scene_indices,
        scene->vertex_count,
        scene_vertices
    );

    // Transform buffer & Geometry buffer
    buffer_create_data(
        state,
        scene_transforms,
        sizeof(m4s) * dynarray_length(scene_transforms),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &scene->transform_buffer);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->transform_buffer.handle, "Transform Buffer");
    buffer_create_data(
        state,
        scene_geometries,
        sizeof(geometry) * dynarray_length(scene_geometries),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &scene->geometry_buffer);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, scene->geometry_buffer.handle, "Geometry Buffer");

    // Move Objects to object buffer
    u64 staging_size = sizeof(object) * dynarray_length(scene_objects);
    buffer staging = {0};
    buffer_create(
        state,
        staging_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        &staging);
    void* mapped_staging;
    VK_CHECK(vkMapMemory(
        state->device.handle,
        staging.memory,
        /* offset: */ 0,
        VK_WHOLE_SIZE,
        /* flags: */ 0,
        &mapped_staging));
    etcopy_memory(mapped_staging, scene_objects, staging_size);
    vkUnmapMemory(state->device.handle, staging.memory);
    VkBufferCopy buff_copy = {
        .dstOffset = 0,
        .srcOffset = 0,
        .size = staging_size};
    IMMEDIATE_SUBMIT(state, cmd, {
        vkCmdCopyBuffer(cmd, staging.handle, scene->object_buffer.handle, 1, &buff_copy);
    });
    buffer_destroy(state, &staging);

    // Move data to scene class:
    scene->vertices = scene_vertices;
    scene->indices = scene_indices;
    
    scene->index_buffer = scene_shared_buffers.index_buffer;
    scene->vertex_buffer = scene_shared_buffers.vertex_buffer;

    // TEMP: Change surface & mesh concept in engine
    scene->surfaces = scene_surfaces;
    scene->meshes = scene_meshes;

    scene->surface_count = dynarray_length(scene_surfaces);
    scene->mesh_count = dynarray_length(scene_meshes);
    // TEMP: END

    scene->transforms = scene_transforms;
    scene->transform_count = dynarray_length(scene_transforms);

    u64 object_count = dynarray_length(scene_objects);
    scene->objects = scene_objects;
    scene->geometries = scene_geometries;

    VkDescriptorBufferInfo geometry_buffer_info = {
        .buffer = scene->geometry_buffer.handle,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    VkWriteDescriptorSet geometry_buffer_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .descriptorCount = 1,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .dstSet = scene->scene_set,
        .dstBinding = 4,
        .pBufferInfo = &geometry_buffer_info,
    };
    VkDescriptorBufferInfo vertex_buffer_info = {
        .buffer = scene->vertex_buffer.handle,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    VkWriteDescriptorSet vertex_buffer_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .descriptorCount = 1,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .dstSet = scene->scene_set,
        .dstBinding = 5,
        .pBufferInfo = &vertex_buffer_info,
    };
    VkDescriptorBufferInfo transform_buffer_info = {
        .buffer = scene->transform_buffer.handle,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    VkWriteDescriptorSet transform_buffer_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = 0,
        .descriptorCount = 1,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .dstSet = scene->scene_set,
        .dstBinding = 6,
        .pBufferInfo = &transform_buffer_info,
    };
    VkWriteDescriptorSet writes[] = {
        geometry_buffer_write,
        vertex_buffer_write,
        transform_buffer_write,
    };
    vkUpdateDescriptorSets(
        state->device.handle,
        /* writeCount */ 3,
        writes,
        /* copyCount */ 0,
        /* copies */ NULL 
    );
    // TEMP: END
    return true;
}

// TODO: Import animations and joints and skins and stuff
static b8 import_gltf_payload(import_payload* payload, const char* path) {
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

    u32 img_start = dynarray_length(payload->images);
    dynarray_resize(&payload->images, img_start + data->images_count);
    for (u32 i = 0; i < data->images_count; ++i) {
        imported_image* image = &payload->images[img_start + i];
        image->data = load_image_data(&data->images[i], path, &image->width, &image->height, &image->channels);
    }

    u32 tex_start = dynarray_length(payload->textures);
    dynarray_resize(&payload->textures, tex_start + data->textures_count);
    for (u32 i = 0; i < data->textures_count; ++i) {
        payload->textures[tex_start + i].image_id = img_start + cgltf_image_index(data, data->textures[i].image);
    }

    u32 mat_start = dynarray_length(payload->materials);
    dynarray_resize(&payload->materials, mat_start + data->materials_count);
    for (u32 i = 0; i < data->materials_count; ++i) {
        cgltf_material mat = data->materials[i];
        cgltf_pbr_metallic_roughness mr = mat.pbr_metallic_roughness;
        imported_material material = {
            .color_factors = {
                .r = mr.base_color_factor[0],
                .g = mr.base_color_factor[1],
                .b = mr.base_color_factor[2],
                .a = mr.base_color_factor[3],
            },
            .color_tex_id = -1,
            .metalness = mr.metallic_factor,
            .roughness = mr.roughness_factor,
            .mr_tex_id = -1,

            // TODO: Get this from the information once shading models have been figured out
            .shading_model = SHADER_MODEL_PBR,
            .flags.pbr = PBR_PROPERTY_METAL_ROUGH,
            // NOTE: This is evil code I will change when I hammer down transparency and pipelines w/alpha masking
            .alpha = (mat.alpha_mode == cgltf_alpha_mode_blend ) ? ALPHA_MODE_BLEND  :  // if blend then blend or
                     (mat.alpha_mode == cgltf_alpha_mode_opaque) ? ALPHA_MODE_OPAQUE :  // if opaque then opaque or
                     (mat.alpha_mode == cgltf_alpha_mode_mask  ) ? ALPHA_MODE_MASK   :  // if mask then mask or 
                                                                   ALPHA_MODE_UNKNOWN,  // Unknown
            // NOTE: END
            // TODO: END
        };

        if (mr.base_color_texture.texture) {
            material.color_tex_id = tex_start + cgltf_texture_index(data, mr.base_color_texture.texture);
        }
        if (mr.metallic_roughness_texture.texture) {
            material.mr_tex_id = tex_start + cgltf_texture_index(data, mr.metallic_roughness_texture.texture);
        }

        payload->materials[mat_start + i] = material;
    }

    u32 mesh_start = dynarray_length(payload->meshes);    
    dynarray_resize(&payload->meshes, mesh_start + data->meshes_count);
    for (u32 i = 0; i < data->meshes_count; ++i) {
        imported_mesh* mesh = &payload->meshes[mesh_start + i];
        mesh->count = data->meshes[i].primitives_count;
        mesh->geometry_ids = dynarray_create(mesh->count, sizeof(u32));
        mesh->material_ids = dynarray_create(mesh->count, sizeof(u32));
        dynarray_resize((void**)&mesh->geometry_ids, mesh->count);
        dynarray_resize((void**)&mesh->material_ids, mesh->count);
        
        u32 geo_start = dynarray_length(payload->geometries);
        dynarray_resize((void**)&payload->geometries, geo_start + mesh->count);

        for (u32 j = 0; j < data->meshes[i].primitives_count; ++j) {
            cgltf_primitive prim = data->meshes[i].primitives[j];
            imported_geometry* geo = &payload->geometries[geo_start + j];

            // NOTE: Converts u16 indices to u32 indices
            geo->indices = dynarray_create(prim.indices->count, sizeof(u32));
            dynarray_resize((void**)&geo->indices, prim.indices->count);
            cgltf_accessor_unpack_indices(prim.indices, geo->indices, sizeof(u32), prim.indices->count);
            
            u32 vertex_count = prim.attributes[0].data->count;
            geo->vertices = dynarray_create(vertex_count, sizeof(vertex));
            dynarray_resize((void**)&geo->vertices, vertex_count);
            etzero_memory(geo->vertices, sizeof(vertex) * vertex_count);
            
            f32* accessor_data = dynarray_create(1, sizeof(f32));
            for (u32 k = 0; k < prim.attributes_count; ++k) {
                u64 float_count = cgltf_accessor_unpack_floats(prim.attributes[k].data, NULL, 0);
                dynarray_resize((void**)&accessor_data, float_count);
                cgltf_accessor_unpack_floats(prim.attributes[k].data, accessor_data, float_count);
                switch (prim.attributes[k].type) {
                    case cgltf_attribute_type_position:
                        v3s* positions = (v3s*)accessor_data;
                        for (u32 l = 0; l < vertex_count; l++) {
                            geo->vertices[l].position = positions[l];
                        }
                        break;
                    case cgltf_attribute_type_normal:
                        v3s* normals = (v3s*)accessor_data;
                        for (u32 l = 0; l < vertex_count; l++) {
                            geo->vertices[l].normal = normals[l];
                        }
                        break;
                    case cgltf_attribute_type_texcoord:
                        v2s* uvs = (v2s*)accessor_data;
                        for (u32 l = 0; l < vertex_count; l++) {
                            geo->vertices[l].uv_x = uvs[l].x;
                            geo->vertices[l].uv_y = uvs[l].y;
                        }
                        break;
                    case cgltf_attribute_type_color:
                        v4s* colors = (v4s*)accessor_data;
                        for (u32 l = 0; l < vertex_count; l++) {
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

            payload->meshes[i].geometry_ids[j] = geo_start + j;
            // TODO: Add default material index into import payload, or use -1 for not present for now
            payload->meshes[i].material_ids[j] = mat_start + (prim.material) ? cgltf_material_index(data, prim.material) : 0;
        }
    }

    u32 node_start = dynarray_length(payload->nodes);
    dynarray_resize(&payload->nodes, node_start + data->nodes_count);
    for (u32 i = 0; i < data->nodes_count; ++i) {
        imported_node node = {0};
        if (data->nodes[i].mesh != NULL) {
            node.has_mesh = true;
            node.mesh_id = mesh_start + cgltf_mesh_index(data, data->nodes[i].mesh);
        }
        if (data->nodes[i].parent != NULL) {
            node.has_parent = true;
            node.parent_id = node_start + cgltf_node_index(data, data->nodes[i].parent);
        }

        node.children_ids = dynarray_create(data->nodes[i].children_count, sizeof(u32));
        dynarray_resize((void**)&node.children_ids, data->nodes[i].children_count);
        for (u32 j = 0; j < data->nodes[i].children_count; ++j) {
            node.children_ids[j] = node_start + cgltf_node_index(data, data->nodes[i].children[j]);
        }

        cgltf_node_transform_local(&data->nodes[i], (cgltf_float*)node.local_transform.raw);
        cgltf_node_transform_world(&data->nodes[i], (cgltf_float*)node.world_transform.raw);
    }

    // TODO: Animation data

    return true;
}

void import_payload_destroy(import_payload* payload) {
    dynarray_destroy(payload->geometries);
    for (u32 i = 0; i < dynarray_length(payload->images); ++i) {
        stbi_image_free(payload->images[i].data);
    }
    dynarray_destroy(payload->textures);
    dynarray_destroy(payload->materials);
    u32 mesh_count = dynarray_length(payload->meshes);
    for (u32 i = 0; i < mesh_count; ++i) {
        dynarray_destroy(payload->meshes[i].geometry_ids);
        dynarray_destroy(payload->meshes[i].material_ids);
    }
    dynarray_destroy(payload->meshes);
    u32 node_count = dynarray_length(payload->nodes);
    for (u32 i = 0; i < node_count; ++i) {
        dynarray_destroy(payload->nodes[i].children_ids);
    }
    dynarray_destroy(payload->nodes);
}

import_payload import_gltfs_payload(u32 file_count, const char* const* paths) {
    ETASSERT(paths);
    import_payload payload = {
        .geometries = dynarray_create(1, sizeof(imported_geometry)),
        .images = dynarray_create(1, sizeof(imported_image)),
        .textures = dynarray_create(1, sizeof(imported_texture)),
        .materials = dynarray_create(1, sizeof(imported_material)),
        .meshes = dynarray_create(1, sizeof(imported_mesh)),
        .nodes = dynarray_create(1, sizeof(imported_node)),
    };

    u32 failure_count = 0;
    for (u32 i = 0; i < file_count; ++i) {
        ETASSERT(paths[i]);
        if (!import_gltf_payload(&payload, paths[i])) {
            ETWARN("Unable to import gltf file %s.", paths[i]);
            failure_count++;
        }
    }
    return payload;
}

b8 dump_gltf_json(const char* gltf_path, const char* dump_file_path) {
    cgltf_options options = {0};
    cgltf_data* data = 0;
    cgltf_result result = cgltf_parse_file(&options, gltf_path, &data);
    if (result != cgltf_result_success) {
        ETERROR("Failed to load gltf file %s.", gltf_path);
        return false;
    }

    etfile* json_dump_file = NULL;
    b8 file_result = true;
    
    file_result = file_open(dump_file_path, FILE_WRITE_FLAG, &json_dump_file);
    if (!file_result) {
        ETERROR("%s failed to open.", dump_file_path);
        return false;
    }

    u64 bytes_written = 0;
    file_result = file_write(json_dump_file, data->json_size, (void*)data->json, &bytes_written);
    if (!file_result) {
        ETERROR("Failed to write %s's json data to %s.", gltf_path, dump_file_path);
        return false;
    }
    file_close(json_dump_file);
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
        gltf_combine_paths(full_path, gltf_path, uri);
        cgltf_decode_uri(full_path + str_length(full_path) - str_length(uri));
        void* data = stbi_load(full_path, width, height, channels, 4);
        etfree(full_path, full_path_bytes, MEMORY_TAG_STRING);
        return data;
    }

    return NULL;
}

// From cgltf_implementation
static void gltf_combine_paths(char* path, const char* base, const char* uri) {
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

// TODO: Use cgltf_attribute_type instead to find the accessor instead
static cgltf_accessor* get_accessor_from_attributes(cgltf_attribute* attributes, cgltf_size attributes_count, const char* name) {
    for (cgltf_size i = 0; i < attributes_count; ++i) {
        if (strs_equal(attributes[i].name, name)) {
            return attributes[i].data;
        }
    }
    return (void*)0;
}