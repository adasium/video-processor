#define NOB_IMPLEMENTATION
#include "./thirdparty/nob.h"


int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "./thirdparty/nob.h");
    if (!nob_mkdir_if_not_exists("build")) return 1;
    Nob_Cmd cmd = {0};

    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    nob_cmd_append(&cmd, "-lraylib");
    nob_cc_output(&cmd, "vp");
    nob_cc_inputs(&cmd, "main.c");
    if (!nob_cmd_run(&cmd)) return 1;

    return 0;
}
