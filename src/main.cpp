#include "Core/MilkShakeEngine.h"

using namespace MilkShake;

int main()
{
    MilkShake::Core::MilkShakeEngine engine;

    try 
    {
        engine.Init();
        engine.Update();
        engine.Destroy();
    }
    catch (const std::exception& e) 
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}