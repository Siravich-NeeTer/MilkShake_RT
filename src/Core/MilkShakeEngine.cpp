#include "MilkShakeEngine.h"

namespace MilkShake
{
	namespace Core
	{
		MilkShakeEngine::MilkShakeEngine()
		{
			m_Renderer = std::make_shared<Graphics::VulkanRenderer>();
		}

		void MilkShakeEngine::Init()
		{
			m_Renderer->Init();
		}
		void MilkShakeEngine::Update()
		{
			m_Renderer->Run();
		}
		void MilkShakeEngine::Destroy()
		{
			m_Renderer->Destroy();
		}
	}
}