/* Copyright (c) 2023, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 */

#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "shared_structs.h"

layout(location = 4) rayPayloadInEXT RayPayload payload;

hitAttributeEXT vec2 bc;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 3, set = 0) uniform sampler2D image;

struct GeometryNode {
	uint64_t vertexBufferDeviceAddress;
	uint64_t indexBufferDeviceAddress;
	int textureIndexBaseColor;
	int textureIndexOcclusion;
};
layout(binding = 4, set = 0) buffer GeometryNodes { GeometryNode nodes[]; } geometryNodes;

layout(binding = 5, set = 0) uniform sampler2D textures[];

#include "bufferreferences.glsl"
#include "geometrytypes.glsl"

void main()
{
	
    // @@ Raycasting: Set payload.hit = true, and fill in the
    // remaining payload values with information (provided by Vulkan)
    // about the hit point.
    payload.hit = true;

    payload.instanceIndex = gl_InstanceCustomIndexEXT;
    payload.primitiveIndex = gl_PrimitiveID;
    payload.bc = vec3(1.0-bc.x-bc.y, bc.x, bc.y);
    payload.hitPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    payload.hitDist = gl_HitTEXT;

	Triangle tri = UnpackTriangle(gl_PrimitiveID, 48);
    payload.normal = vec3(tri.normal);
}
