#pragma once

namespace MilkShake
{
	struct Material
	{
		int BaseColorTextureID = -1;
		int NormalTextureID = -1;
		int OcclusionTextureID = -1;

		glm::vec3 diffuse = glm::vec3(1.0f);
		glm::vec3 specular = glm::vec3(1.0f);
		glm::vec3 emission = glm::vec3(0.0f);
		float shininess = 20.0f;
	};
}