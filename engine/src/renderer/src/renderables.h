#pragma once

#include "renderer/src/vk_types.h"

/** NOTE: Renderables should be visible outside of the renderer
 * struct scene
 * struct node
 * struct mesh
 * struct mesh_node
 * 
 * 
 * Renderable is an interface/abstract class implementation
 * Node is a parent class of mesh_node
 * Mesh_node derives from node
 */

// Singular inheritance -- Chosen
// Use the fact that the base struct is stored first in the derived struct.
// The address of a derived struct given a base struct address is just the 
// address of the base struct because we have stored the base struct as the
// first member in the derived struct
// ^^ This would not allow an array of abstract classes; Would need an array of pointers
// Which is needed anyway as the self pointer would be stored in the renderable struct

// TODO: Create a container_of macro for multiple inheritance?? (Curiosity not need)
// if I want the derived struct to inherit multiple things.

/** NOTE: Hacky c code mimicking C++ OOP inheritance of node classes from vkguide.dev.
 * TODO: Move this into the renderables section
 */
typedef struct renderable_virtual_table renderable_vt;
typedef struct renderable {
    renderable_vt* vt;
} renderable;

typedef struct node_virtual_table node_vt;
typedef struct node {
    // Extends renderable
    renderable renderable;

    // Polymorphism data
    node_vt* vt;

    // Actual struct node data
    char* name;
    struct node* parent;
    struct node** children;
    m4s local_transform;
    m4s world_transform;
} node;

typedef struct mesh_node {
    // Extends node
    struct node base;

    // Pointer as mesh can be shared between multiple nodes
    mesh* mesh;
} mesh_node;

void renderable_destroy(renderable* renderable);
void renderable_draw(renderable* renderable, const m4s top_matrix, draw_context* ctx);

// Initializes a node where out_node is pointing
void node_create(node* out_node);

// Deinitializes a node where node is pointing
void node_destroy(node* node);

// Calls transform where all the nodes are pointing
void node_refresh_transform(node* node, const m4s parent_matrix);

// Calls the node's draw function
void node_draw(node* node, const m4s top_matrix, draw_context* ctx);

// Initializes a mesh_node where out_node is pointing
void mesh_node_create(mesh_node* out_node);

// Deinitializes a mesh_node where node is pointing
void mesh_node_destroy(mesh_node* node);

// Calls the mesh_node's draw function
void mesh_node_draw(mesh_node* node, const m4s top_matrix, draw_context* ctx);

// Returns a reference to the base node struct in mesh_node
node* node_from_mesh_node(mesh_node* node);

void gltf_draw(struct loaded_gltf* gltf, const m4s top_matrix, struct draw_context* ctx);

void node_print(node* node);

void mesh_node_print(mesh_node* node);

void gltf_print(struct loaded_gltf* gltf, const char* gltf_name);

void draw_context_print(draw_context* ctx);