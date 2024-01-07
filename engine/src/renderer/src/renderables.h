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

void renderable_destroy(renderable* renderable);
void renderable_draw(renderable* renderable, const m4s top_matrix, draw_context* ctx);

void node_destroy(node* node);
void node_refresh_transform(node* node, const m4s parent_matrix);
void node_draw(node* node, const m4s top_matrix, draw_context* ctx);

mesh_node* mesh_node_create(void);
void mesh_node_destroy(mesh_node* node);
void mesh_node_draw(mesh_node* node, const m4s top_matrix, draw_context* ctx);
node* node_from_mesh_node(mesh_node* node);