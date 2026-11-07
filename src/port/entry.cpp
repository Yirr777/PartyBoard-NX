#include <aurora/main.h>
#ifdef __SWITCH__
// libnx's crt0 calls main directly; aurora::main's SDL_main shim is not linked on Switch.
#undef main
#endif
extern "C" int port_main(int argc, char* argv[]);
int main(int argc, char *argv[])
{
    return port_main(argc, argv);
}
