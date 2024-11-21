#ifndef SHARED_STRUCTS
#define SHARED_STRUCTS

#ifdef __cplusplus
#include <glm/glm.hpp>
// GLSL Type
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;
using uint = unsigned int;
#endif

struct RayPayload
{
    uint seed;		// Used in Path Tracing step as random number seed
    bool hit;           // Does the ray intersect anything or not?
    float hitDist;      // Used in the denoising step
    vec3 hitPos;	// The world coordinates of the hit point.      
    int instanceIndex;  // Index of the object instance hit (we have only one, so =0)
    int primitiveIndex; // Index of the hit triangle primitive within object
    vec3 bc;            // Barycentric coordinates of the hit point within triangle
    vec3 normal;        // TODO: Temp Data
};

#endif // SHARED_STRUCTS
