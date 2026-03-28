#include "vibecraft/app/Application.hpp"

int main()
{
    vibecraft::app::Application application;
    if (!application.initialize())
    {
        return 1;
    }

    return application.run();
}
