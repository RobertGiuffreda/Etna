#pragma once
#include "defines.h"
#include "math/math_types.h"
#include "resources/resource_types.h"

typedef struct node_vt node_vt;

typedef struct node node;
struct node {
    node_vt* vt;

    const char* name;
    node* parent;
    node** children;
    m4s local_transform;
    m4s world_transform;
};

typedef struct mesh_node {
    node base;
    
    mesh* mesh;
} mesh_node;