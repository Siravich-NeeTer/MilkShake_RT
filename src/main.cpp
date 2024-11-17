#include "Renderer/VKRenderer.h"

using namespace MilkShake;

int main()
{
    Graphics::VKRenderer renderer;

    try 
    {
        renderer.Run();
    }
    catch (const std::exception& e) 
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}