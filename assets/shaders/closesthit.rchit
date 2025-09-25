#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "shared_structs.h"

layout(location = 0) rayPayloadInEXT RayPayload payload;
layout(location = 1) rayPayloadEXT bool isShadowed;

hitAttributeEXT vec2 bc;

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;

// TODO: Merge GeometryNode into single file instead of multiple declaration
struct GeometryNode {
	uint64_t vertexBufferDeviceAddress;
	uint64_t indexBufferDeviceAddress;
	int textureIndexBaseColor;
	int textureIndexOcclusion;
    
    vec3 diffuse;
    vec3 specular;
    vec3 emission;
    float shininess;
};
layout(set = 1, binding = 2) buffer GeometryNodes { GeometryNode nodes[]; } geometryNodes;
layout(set = 1, binding = 3) uniform sampler2D textures[];

#include "bufferreferences.glsl"
#include "geometrytypes.glsl"
#include "material_utility.glsl"

layout(push_constant) uniform _PushConstantRay { PushConstantRay pcRay; };

void main()
{
	
    // @@ Raycasting: Set payload.hit = true, and fill in the
    // remaining payload values with information (provided by Vulkan)
    // about the hit point.
    payload.hit = true;

    // gl_InstanceCustomIndexEXT will represent GeometryNodes Offset from SSBO
    payload.instanceIndex = gl_InstanceCustomIndexEXT;
    payload.primitiveIndex = gl_PrimitiveID;
    payload.geometryIndex = gl_GeometryIndexEXT;
    payload.bc = vec3(1.0 - bc.x - bc.y, bc.x, bc.y);
    payload.hitPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    payload.hitDist = gl_HitTEXT;
}
