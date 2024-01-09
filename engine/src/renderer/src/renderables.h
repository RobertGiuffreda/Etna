#pragma once

#include "vk_types.h"

/** NOTE:
 * Renderable is an interface/abstract class implementation
 * Node is a parent class of mesh_node
 * Mesh_node derives from node
 * 
 * Only creating a mesh_node and putting it in a node array is currently supported
 * 
 * Instead of storing self it may be possible to just use the passed in object
 */

// TODO: Current Issue: Copying the struct will create a shallow copy at the moment, 
// which means that the void* self pointers stored to pass to the function will be pointing
// to the wrong memory. 

// Solutions:
// Opt 1: Copy constructor function
// Could have a specific copy function for copying the object that would also set the self 
// pointer to the new correct location. But that would restrict using the dynarray which
// just copies the struct over into the array using void* and a memcpy.
// Have a version of dynarray that takes a copy constructor function argument on creation
// and uses that to move memory over to the dynarray.

// Opt 2: Singular inheritance
// Use the fact that the base struct is stored first in the derived struct.
// The address of a derived struct given a base struct address is just the 
// address of the base struct because we have stored the base struct as the
// first member in the derived struct
// ^^ This would not allow an array of abstract classes; Would need an array of pointers
// Which is needed anyway as the self pointer would be stored in the renderable struct

// TODO: Create a windows equivelent of the container_of macro 
// if I want the derived struct to inherit multiple things.

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