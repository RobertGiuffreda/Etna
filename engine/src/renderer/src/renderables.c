#include "renderables.h"

#include "containers/dynarray.h"

#include "math/etmath.h"

#include "core/etmemory.h"
#include "core/logger.h"

// Internal static functions
static inline void _renderable_destroy(renderable* renderable);

// Wrapper function for vtable function pointer type compatibility
static inline void _node_destroy_vt(void* self);
// Actual implementation of node_destroy. node struct version specific
static inline void _node_destroy(node* node);

// Wrapper function for vtable function pointer type compatibility
static inline void _node_draw_vt(void* self, const m4s top_matrix, draw_context* ctx);
// Actual implementation of node_draw. node struct version specific
static inline void _node_draw(node* node, const m4s top_matrix, draw_context* ctx);

static inline void _mesh_node_destroy_vt(void* self);
static inline void _mesh_node_destroy(mesh_node* node);

static inline void _mesh_node_draw_vt(void* self, const m4s top_matrix, draw_context* ctx);
static inline void _mesh_node_draw(mesh_node* node, const m4s top_matrix, draw_context* ctx);

// Renderable functions:
void renderable_destroy(renderable* renderable) {
    renderable->vt->destroy(renderable->self);
}
static inline void _renderable_destroy(renderable* renderable) {
    etfree(renderable->vt, sizeof(renderable_vt), MEMORY_TAG_RENDERABLE);
}

void renderable_draw(renderable* renderable, const m4s top_matrix, draw_context* ctx) {
    renderable->vt->draw(renderable->self, top_matrix, ctx);
}

// Node functions:
void node_create(node* out_node) {
    renderable_vt* r_vt = (renderable_vt*)etallocate(sizeof(renderable_vt), MEMORY_TAG_RENDERABLE);
    r_vt->draw = _node_draw_vt;
    r_vt->destroy = _node_destroy_vt;

    node_vt* n_vt = (node_vt*)etallocate(sizeof(node_vt), MEMORY_TAG_RENDERABLE);
    n_vt->draw = _node_draw_vt;
    n_vt->destroy = _node_destroy_vt;

    node new_node = {
        .renderable = { .vt = r_vt },
        .vt = n_vt,
        .children = dynarray_create(0, sizeof(struct node*))};
    *out_node = new_node;

    out_node->self = out_node;
    out_node->renderable.self = out_node;
}

void node_destroy(node* node) {
    node->vt->destroy(node->self);
}
static inline void _node_destroy_vt(void* self) {
    _node_destroy(self);
}
static inline void _node_destroy(node* node) {
    _renderable_destroy(&node->renderable);
    etfree(node->vt, sizeof(node_vt), MEMORY_TAG_RENDERABLE);
    dynarray_destroy(node->children);
}

void node_refresh_transform(node* node, const m4s parent_matrix) {
    node->world_transform = glms_mat4_mul(parent_matrix, node->local_transform);
    for (u32 i = 0; i < dynarray_length(node->children); ++i) {
        node_refresh_transform(node->children[i], node->world_transform);
    }
}

void node_draw(node* node, const m4s top_matrix, draw_context* ctx) {
    node->vt->draw(node->self, top_matrix, ctx);
}
static inline void _node_draw_vt(void* self, const m4s top_matrix, draw_context* ctx) {
    _node_draw(self, top_matrix, ctx);
}
static inline void _node_draw(node* node, const m4s top_matrix, draw_context* ctx) {
    for (u32 i = 0; i < dynarray_length(node->children); ++i) {
        node->children[i]->vt->draw(node->children[i]->self, top_matrix, ctx);
    }
}

// Mesh node functions:
void mesh_node_create(mesh_node* out_node) {
    // Set up virtual tables
    renderable_vt* r_vt = (renderable_vt*)etallocate(sizeof(renderable_vt), MEMORY_TAG_RENDERABLE);
    r_vt->draw = _mesh_node_draw_vt;
    r_vt->destroy = _mesh_node_destroy_vt;

    node_vt* n_vt = (node_vt*)etallocate(sizeof(node_vt), MEMORY_TAG_RENDERABLE);
    n_vt->draw = _mesh_node_draw_vt;
    n_vt->destroy = _mesh_node_destroy_vt;

    mesh_node new_node = {
        .base = {
            .renderable = {
                .vt = r_vt
            },
            .vt = n_vt,
            .children = dynarray_create(0, sizeof(struct node*)),
        }
    };
    *out_node = new_node;

    out_node->base.renderable.self = out_node;
    out_node->base.self = out_node;
}

void mesh_node_destroy(mesh_node* node) {
    _mesh_node_destroy(node);
}
static inline void _mesh_node_destroy_vt(void* self) {
    _mesh_node_destroy(self);
}
static inline void _mesh_node_destroy(mesh_node* node) {
    _node_destroy(&node->base);
}

void mesh_node_draw(mesh_node* node, const m4s top_matrix, draw_context* ctx) {
    _mesh_node_draw(node, top_matrix, ctx);
}
static inline void _mesh_node_draw_vt(void* self, const m4s top_matrix, draw_context* ctx) {
    _mesh_node_draw(self, top_matrix, ctx);
}
static inline void _mesh_node_draw(mesh_node* node, const m4s top_matrix, draw_context* ctx) {
    m4s node_matrix = glms_mat4_mul(top_matrix, node->base.world_transform);

    u32 surface_count = dynarray_length(node->mesh->surfaces);
    for (u32 i = 0; i < surface_count; ++i) {
        geo_surface* s = &node->mesh->surfaces[i];
        render_object def = {
            .index_count = s->count,
            .first_index = s->start_index,
            .index_buffer = node->mesh->mesh_buffers.index_buffer.handle,
            .material = &s->material->data,
            .transform = node_matrix,
            .vertex_buffer_address = node->mesh->mesh_buffers.vertex_buffer_address
        };
        if (s->material->data.pass_type == MATERIAL_PASS_TRANSPARENT) {
            dynarray_push((void**)&ctx->transparent_surfaces, &def);
        }
        else {
            dynarray_push((void**)&ctx->opaque_surfaces, &def);
        }
    }

    _node_draw(&node->base, top_matrix, ctx);
}

node* node_from_mesh_node(mesh_node* node) {
    return &node->base;
}

void gltf_draw(struct loaded_gltf* gltf, const m4s top_matrix, struct draw_context* ctx) {
    u32 node_count = dynarray_length(gltf->top_nodes);
    for (u32 i = 0; i < node_count; ++i) {
        node_draw(gltf->top_nodes[i], top_matrix, ctx);
    }
}

void mesh_asset_print(mesh_asset* m_asset) {
    ETINFO("Mesh asset %s data:", m_asset->name);
    for (u32 i = 0; i < dynarray_length(m_asset->surfaces); ++i) {
        geo_surface* surface = &m_asset->surfaces[i];
        ETINFO(
            "Surface %lu: Start index: %lu | index_count: %lu", 
            i, surface->start_index, surface->count
        );
    }
}

void node_print(node* node) {
    ETINFO("Local transform: ");
    glms_mat4_print(node->local_transform, stderr);
    ETINFO("World transform: ");
    glms_mat4_print(node->world_transform, stderr);
}

void mesh_node_print(mesh_node* node) {
    mesh_asset_print(node->mesh);
    node_print(&node->base);
}

void gltf_print(struct loaded_gltf* gltf, const char* gltf_name) {
    ETINFO("GLTF: %s's data:", gltf_name);
    ETINFO(
        "Mesh count: %lu | Image count: %lu | Material count: %lu | Sampler count %lu.",
        gltf->mesh_count, gltf->image_count, gltf->material_count, gltf->sampler_count
    );
    ETINFO(
        "Mesh node count: %lu | node count: %lu | Top node count: %lu",
        gltf->mesh_node_count, gltf->node_count, gltf->top_node_count
    );
    ETINFO("Mesh asset information: ");
    for (u32 i = 0; i < gltf->mesh_node_count; ++i) {
        mesh_node_print(&gltf->mesh_nodes[i]);
    }
}

void draw_context_print(draw_context* ctx) {
    ETINFO("Opaque surfaces: ");
    for (u32 i = 0; i < dynarray_length(ctx->opaque_surfaces); ++i) {
        render_object* rob = &ctx->opaque_surfaces[i];
        ETINFO(
            "Render Object %lu: Start index: %lu | Index count: %lu",
            i, rob->first_index, rob->index_count
        );
    }
    ETINFO("Transparent surfaces: ");
    for (u32 i = 0; i < dynarray_length(ctx->transparent_surfaces); ++i) {
        render_object* rob = &ctx->transparent_surfaces[i];
        ETINFO(
            "Render Object %lu: Start index: %lu | Index count: %lu",
            i, rob->first_index, rob->index_count
        );
    }

}