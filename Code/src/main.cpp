#include "../header/vulkan/Application.hpp"

#include <iostream>
#include <cstdlib>
#include <stdexcept>

int main()
{
    try
    {
        vkapp::Application app;
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
