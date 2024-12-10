/* Copyright (c) 2023, Sascha Willems
 *
 * SPDX-License-Identifier: MIT
 *
 */

#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable

#include "shared_structs.h"

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main()
{
    payload.hit = false;
}