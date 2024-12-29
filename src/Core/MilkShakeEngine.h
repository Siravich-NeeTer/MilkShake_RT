#pragma once

#include <memory>

#include "Graphics/VulkanRenderer.h"

namespace MilkShake
{
	namespace Core
	{
		class MilkShakeEngine
		{
			public:
				MilkShakeEngine();

				void Init();
				void Update();
				void Destroy();

			private:
				std::shared_ptr<Graphics::VulkanRenderer> m_Renderer;
		};
	}
}