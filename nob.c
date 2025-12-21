#define NOB_IMPLEMENTATION
#include "./thirdparty/nob.h"
#define RAYLIB_SRC_FOLDER "./thirdparty/raylib/src/"

#ifdef _WIN32
#define MAYBE_PREFIXED(x) x
#else
#define MAYBE_PREFIXED(x) "x86_64-w64-mingw32-"x
#endif // _WIN32

static const char *raylib_modules[] = {
    "rcore",
    "raudio",
    "rglfw",
    "rmodels",
    "rshapes",
    "rtext",
    "rtextures",
    "utils",
};

bool build_raylib()
{
    bool result = true;
    Nob_Cmd cmd = {0};
    Nob_File_Paths object_files = {0};

    if (!nob_mkdir_if_not_exists("./build/raylib")) {
        nob_return_defer(false);
    }

    Nob_Procs procs = {0};

    const char *build_path = nob_temp_sprintf("./build/raylib");

    if (!nob_mkdir_if_not_exists(build_path)) {
        nob_return_defer(false);
    }

    for (size_t i = 0; i < NOB_ARRAY_LEN(raylib_modules); ++i) {
        const char *input_path = nob_temp_sprintf(RAYLIB_SRC_FOLDER"%s.c", raylib_modules[i]);
        const char *output_path = nob_temp_sprintf("%s/%s.o", build_path, raylib_modules[i]);
        output_path = nob_temp_sprintf("%s/%s.o", build_path, raylib_modules[i]);

        nob_da_append(&object_files, output_path);

        if (nob_needs_rebuild(output_path, &input_path, 1)) {
            nob_cc(&cmd);
            nob_cmd_append(&cmd, "-c", input_path);
            nob_cmd_append(&cmd, "-o", output_path);
            nob_cmd_append(&cmd, "-D_GLFW_X11");
            nob_cmd_append(&cmd, "-ggdb", "-DPLATFORM_DESKTOP");
            nob_cmd_append(&cmd, "-fPIC", "-shared");

            if (!nob_cmd_run(&cmd, .async = &procs)) nob_return_defer(false);
        }
    }

    if (!nob_procs_flush(&procs)) nob_return_defer(false);

    const char *libraylib_path = nob_temp_sprintf("%s/libraylib.a", build_path);

    if (nob_needs_rebuild(libraylib_path, object_files.items, object_files.count)) {
        nob_cmd_append(&cmd, "ar");
        nob_cmd_append(&cmd, "-crs", libraylib_path);
        for (size_t i = 0; i < NOB_ARRAY_LEN(raylib_modules); ++i) {
            const char *input_path = nob_temp_sprintf("%s/%s.o", build_path, raylib_modules[i]);
            nob_cmd_append(&cmd, input_path);
        }
        if (!nob_cmd_run(&cmd)) nob_return_defer(false);
    }

defer:
    nob_cmd_free(cmd);
    nob_da_free(object_files);
    return result;
}



int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "./thirdparty/nob.h");
    if (!nob_mkdir_if_not_exists("build")) return 1;

    build_raylib();

    Nob_Cmd cmd = {0};

    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    nob_cc_output(&cmd, "vp");
    nob_cc_inputs(&cmd, "main.c");
    nob_cmd_append(&cmd, "-I.");
    nob_cmd_append(&cmd, "-I"RAYLIB_SRC_FOLDER);
    nob_cmd_append(&cmd,
               "-L./build/raylib/",
               "-lraylib");
    nob_cmd_append(&cmd);
    nob_cmd_append(
                   &cmd,
                  "-lm", "-ldl", "-flto=auto", "-lpthread");
    if (!nob_cmd_run(&cmd)) return 1;


    return 0;
}
