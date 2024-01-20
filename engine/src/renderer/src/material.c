#include "material.h"

#include "core/etmemory.h"
#include "core/logger.h"

#include "renderer/src/renderer.h"
#include "renderer/src/descriptor.h"

/** NOTE:
 * 
 */

b8 material_blueprint_create(renderer_state* state, const char* vertex_path, const char* fragment_path, material_blueprint* blueprint) {
    if (!load_shader(state, vertex_path, &blueprint->vertex)) {
        ETERROR("Unable to load vertex shader %s.", vertex_path);
        return false;
    }

    if (!load_shader(state, fragment_path, &blueprint->fragment)) {
        ETERROR("Unable to load fragment shader %s.", fragment_path);
        unload_shader(state, &blueprint->vertex);
        return false;
    }

    // Merge vertex and fragment shader set_layouts
    

    /** TODO:
     * Create VkDescriptorSetLayout for the material set layout
     * 
     * Use scene descriptor set layout and created material set layout to create VkPipelineLayout
     * Build VkPipeline objects for transparent and opaque objects
     */

}