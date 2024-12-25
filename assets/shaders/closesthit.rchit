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

layout(location = 0) rayPayloadInEXT RayPayload payload;
layout(location = 1) rayPayloadEXT bool isShadowed;

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
    payload.bc = vec3(1.0-bc.x-bc.y, bc.x, bc.y);
    payload.hitPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    payload.hitDist = gl_HitTEXT;

	Triangle tri = UnpackTriangle(gl_PrimitiveID);
	GeometryNode geometryNode = geometryNodes.nodes[gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT];

    
    // Computing the coordinates of the hit position
    const vec3 pos      = tri.vertices[0].pos * payload.bc.x + tri.vertices[1].pos * payload.bc.y + tri.vertices[2].pos * payload.bc.z;
    const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));  // Transforming the position to world space

    vec3 normal = tri.normal;
    vec3 worldNormal = normalize(vec3(normal * gl_WorldToObjectEXT));  // Transforming the normal to world space

    payload.normal = normal;
    payload.color = texture(textures[nonuniformEXT(geometryNode.textureIndexBaseColor)], tri.uv).rgb;
    
	if (geometryNode.textureIndexOcclusion > -1) 
    {
		float occlusion = texture(textures[nonuniformEXT(geometryNode.textureIndexOcclusion)], tri.uv).r;
		payload.color *= occlusion;
	}

    // -------------------------------------------------------------------------------------------------------------- //
    // Lighting
    vec3 L = normalize(pcRay.lightPosition);
    float lightIntensity = pcRay.lightIntensity;
    float lightDistance  = 100000.0f;

    // PointLight
    vec3 lDir      = pcRay.lightPosition - worldPos;
    lightDistance  = length(lDir);
    lightIntensity = pcRay.lightIntensity / (lightDistance * lightDistance);
    L              = normalize(lDir);

    // Diffuse
    vec3 diffuse = ComputeDiffuse(L, worldNormal);
    diffuse *= texture(textures[nonuniformEXT(geometryNode.textureIndexBaseColor)], tri.uv).rgb;

    vec3  specular    = vec3(0);
    float attenuation = 1.0f;

    // Tracing shadow ray only if the light is visible from the surface
    if(dot(worldNormal, L) > 0)
    {
        float tMin   = 0.001;
        float tMax   = lightDistance;
        vec3  origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
        vec3  rayDir = L;
        uint  flags  = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
        isShadowed   = true;
        traceRayEXT(topLevelAS,  // acceleration structure
                    flags,       // rayFlags
                    0xFF,        // cullMask
                    0,           // sbtRecordOffset
                    0,           // sbtRecordStride
                    1,           // missIndex
                    origin,      // ray origin
                    tMin,        // ray min range
                    rayDir,      // ray direction
                    tMax,        // ray max range
                    1            // payload (location = 1)
        );

        if(isShadowed)
        {
            attenuation = 0.3;
        }
        else
        {
            // Specular
            specular = ComputeSpecular(gl_WorldRayDirectionEXT, L, worldNormal);
        }
    }

    payload.color = vec3(lightIntensity * attenuation * (diffuse + specular));
    // -------------------------------------------------------------------------------------------------------------- //
}
