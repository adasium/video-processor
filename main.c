#include "raylib.h"
#include <stdio.h>
#include <string.h>

#define NOB_IMPLEMENTATION
#include "./thirdparty/nob.h"

#define MAX_FILEPATH_RECORDED   4096
#define MAX_FILEPATH_SIZE       2048

#define MAX_CRF 64
#define SLIDER_BUTTON_SIZE 10

#define LABEL_Y_OFFSET 20
#define DEFAULT_FONT_SIZE 10

#define DEBUG 0

float clampf(float value, float min, float max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

const char *EXTENSIONS[] = {
    ".mp4",
    ".avi",
};


typedef enum {
    NOTHING,
    BACKGROUND,
    CRF,
    CROP_TOP,
    CROP_BOTTOM,
    CROP_LEFT,
    CROP_RIGHT,
    SUBMIT_BTN,
} InteractingWith;


typedef struct {
    Rectangle bounds;
    int min;
    int max;
    int value;
    int step;
} Slider;


void slider_debug(Slider *slider) {
    printf("[%f %f %f %f] %d (%d %d, %d)\n",
            (*slider).bounds.x,
            (*slider).bounds.y,
            (*slider).bounds.width,
            (*slider).bounds.height,
            (*slider).value,
            (*slider).min,
            (*slider).max,
            (*slider).step
            );
}


Rectangle slider_get_button_bounds(Slider *slider) {
    Rectangle rect = {
        (*slider).bounds.x - SLIDER_BUTTON_SIZE/2 + (*slider).bounds.width * (*slider).value / (*slider).max,
        (*slider).bounds.y - SLIDER_BUTTON_SIZE/2,
        SLIDER_BUTTON_SIZE,
        SLIDER_BUTTON_SIZE,
    };
    return rect;
}


int slider_check_collision_point(Slider *slider, Vector2 mouse) {

    if (CheckCollisionPointRec(mouse, (*slider).bounds)) {
        return true;
    }
    return false;
}


void slider_set_value(Slider *slider, Vector2 mouse) {
    float value = clampf(mouse.x, (*slider).bounds.x, (*slider).bounds.x + (*slider).bounds.width) - (*slider).bounds.x;
    value /= (*slider).bounds.width;
    value *= (*slider).max;
    (*slider).value = (int)value - (int)value % (*slider).step;
}


void slider_draw(Slider *slider, char* label) {
    const int line_thickness = 2;
    const int label_font_size = 18;
    DrawText(TextFormat(label), (*slider).bounds.x, (*slider).bounds.y - LABEL_Y_OFFSET, label_font_size, BLACK);
    DrawText(TextFormat("%d", (*slider).value), (*slider).bounds.x + (*slider).bounds.width + 10, (*slider).bounds.y, label_font_size, BLACK);
    DrawRectangle(
                  (*slider).bounds.x,
                  (*slider).bounds.y + SLIDER_BUTTON_SIZE/2,
                  (*slider).bounds.width,
                  line_thickness,
                  LIGHTGRAY);
    DrawRectangle(
                  (*slider).bounds.x + (*slider).bounds.width*(*slider).value/(*slider).max - SLIDER_BUTTON_SIZE/2,
                  (*slider).bounds.y,
                  SLIDER_BUTTON_SIZE,
                  SLIDER_BUTTON_SIZE,
                  GRAY);
}

typedef struct {
    Rectangle bounds;
    char *label;
    int font_size;
} Button;


void button_draw(Button *btn, int active) {
    int spacing = (*btn).font_size/DEFAULT_FONT_SIZE;

    if (active) {
        DrawRectangleRec((*btn).bounds, GRAY);
    }
    DrawRectangleLinesEx((*btn).bounds, 2, BLACK);


    Vector2 text_size = MeasureTextEx(GetFontDefault(), (*btn).label, (*btn).font_size, (float)spacing);

    DrawText(
             (*btn).label,
             (*btn).bounds.x + (*btn).bounds.width/2 - text_size.x/2,
             (*btn).bounds.y + (*btn).bounds.height/2 - text_size.y/2,
             (*btn).font_size,
             BLACK);

}


typedef struct {
    char* input_path;
    char* output_path;
    int crf;
    int crop_top;
    int crop_bottom;
    int crop_left;
    int crop_right;
} FfmpegParams;


void ffmpeg_params_print(FfmpegParams *params) {
    printf("[DEBUG] input_path: \"%s\"\n", (*params).input_path);
    printf("[DEBUG] output_path: \"%s\"\n", (*params).output_path);
    printf("[DEBUG] crf: %d\n", (*params).crf);
    printf("[DEBUG] crop: [%d, %d, %d, %d]",
           (*params).crop_top,
           (*params).crop_bottom,
           (*params).crop_left,
           (*params).crop_right);
    printf("\n");
}


int run_ffmpeg(FfmpegParams params) {
    if (DEBUG) ffmpeg_params_print(&params);

    char crf_str[3] = {
        (params.crf) % 10 + '0',
        params.crf / 10 + '0',
    };

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "ffmpeg");
    nob_cmd_append(&cmd, "-i", params.input_path);
    nob_cmd_append(&cmd, "-crf", crf_str);
    if (params.crop_left | params.crop_right | params.crop_left | params.crop_right) {
        char crop[255];
        sprintf(
                crop,
                "crop=in_w-%d:in_h-%d:%d:%d",
                params.crop_left + params.crop_right,
                params.crop_top + params.crop_bottom,
                params.crop_left,
                params.crop_right
                );
        nob_cmd_append(&cmd, crop);
    }

    nob_cmd_append(&cmd, params.output_path);

    Nob_String_Builder sb = {0};
    nob_cmd_render(cmd, &sb);
    nob_sb_append_null(&sb);
    nob_log(NOB_INFO, "CMD: %s", sb.items);
    nob_sb_free(sb);
    memset(&sb, 0, sizeof(sb));

    if(!strlen(params.input_path)) {
        printf("[INFO] no file is selected.\n");
        return 1;
    } else {
        printf("[INFO] selected file: %s\n", params.input_path);
    }


    if (!nob_cmd_run(&cmd)) return 1;
    return 0;
}


void set_input_path(char* input_path) {
    const char* nemo_paths = getenv("NEMO_SCRIPT_SELECTED_FILE_PATHS");
    if (nemo_paths == NULL) return;

    if (DEBUG) printf("[DEBUG] %s", nemo_paths);
    int length = strchr(nemo_paths, '\n') - nemo_paths;
    strncpy(input_path, nemo_paths, length);
    return;
}


int str_endswith(const char* string, const char* ending) {
    char* pos = strrchr(string, '.');
    if (pos != NULL)
        return strcmp(pos, ending);
    return -1;
}


void set_output_path(char* output_path, char* input_path) {
    if (strlen(input_path) < 1) return;
    if (strlen(output_path) > 0) return;
    char name_suffix[] = "_v2";
    for (size_t i = 0; i < sizeof(EXTENSIONS)/sizeof(EXTENSIONS[0]); ++i) {
        if(str_endswith(input_path, EXTENSIONS[i]) != 0) continue;

        char* pos = strrchr(input_path, '.');
        strcpy(output_path, input_path);
        strcpy(output_path + (pos - input_path), name_suffix);
        strcpy(output_path + (pos - input_path) + strlen(name_suffix), EXTENSIONS[i]);
        return;
    }
}


int main(void)
{
    InitWindow(800, 450, "video-processor");
    SetWindowMonitor(0);
    SetTargetFPS(60);

    bool exit_window = false;
    char input_path[MAX_FILEPATH_SIZE] = "";
    char output_path[MAX_FILEPATH_SIZE] = "";
    set_input_path(input_path);
    set_output_path(output_path, input_path);
    InteractingWith interacting_with = NOTHING;
    Vector2 center = {
        GetScreenWidth() / 2,
        GetScreenHeight() / 2,
    };
    int slider_width = 100;
    int slider_height = 20;
    int slider_x_offset = 150;
    int slider_y_offset = 50;
    Slider crf = {
        .bounds = {
            center.x,
            center.y,
            slider_width,
            slider_height,
        },
        .min = 1,
        .max = 64,
        .value = 28,
        .step = 1,
    };
    Slider crop_top = {
        .bounds = {
            center.x,
            center.y - slider_y_offset,
            slider_width,
            slider_height,
        },
        .min = 0,
        .max = 1000,
        .value = 0,
        .step = 50,
    };
    Slider crop_bottom = {
        .bounds = {
            center.x,
            center.y + slider_y_offset,
            slider_width,
            slider_height,
        },
        .min = 0,
        .max = 1000,
        .value = 0,
        .step = 50,
    };
    Slider crop_left = {
        .bounds = {
            center.x - slider_x_offset,
            center.y,
            slider_width,
            slider_height,
        },
        .min = 0,
        .max = 1000,
        .value = 0,
        .step = 50,
    };
    Slider crop_right = {
        .bounds = {
            center.x + slider_x_offset,
            center.y,
            slider_width,
            slider_height,
        },
        .min = 0,
        .max = 1000,
        .value = 0,
        .step = 50,
    };
    Button submit_btn = {
        .bounds = {
            .x = center.x + slider_x_offset,
            .y = center.y + slider_y_offset,
            .width = 100,
            .height = 50,
        },
        .label = "run",
        .font_size = 28,
    };
    while (!exit_window)
    {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q) || WindowShouldClose()) exit_window = true;

        if (IsFileDropped()) {
            FilePathList dropped_files = LoadDroppedFiles();
            if (dropped_files.count > 0) {
                strcpy(input_path, dropped_files.paths[0]);
                set_output_path(output_path, input_path);
            }
            UnloadDroppedFiles(dropped_files);
        }

        Vector2 mouse = GetMousePosition();
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            if (DEBUG) slider_debug(&crf);
            while (1) {
                if (interacting_with == NOTHING) {
                    if(slider_check_collision_point(&crf, mouse)) {
                        interacting_with = CRF;
                        break;
                    }

                    if(slider_check_collision_point(&crop_top, mouse)) {
                        interacting_with = CROP_TOP;
                        break;
                    }
                    if(slider_check_collision_point(&crop_bottom, mouse)) {
                        interacting_with = CROP_BOTTOM;
                        break;
                    }
                    if(slider_check_collision_point(&crop_left, mouse)) {
                        interacting_with = CROP_LEFT;
                        break;
                    }
                    if(slider_check_collision_point(&crop_right, mouse)) {
                        interacting_with = CROP_RIGHT;
                        break;
                    }

                    if(CheckCollisionPointRec(mouse, submit_btn.bounds)) {
                        interacting_with = SUBMIT_BTN;
                        break;
                    }
                    interacting_with = BACKGROUND;
                    break;
                }
                break;
            }

            if (interacting_with == CRF) {
                slider_set_value(&crf, mouse);
            }
            if (interacting_with == CROP_TOP) {
                slider_set_value(&crop_top, mouse);
            }
            if (interacting_with == CROP_BOTTOM) {
                slider_set_value(&crop_bottom, mouse);
            }
            if (interacting_with == CROP_LEFT) {
                slider_set_value(&crop_left, mouse);
            }
            if (interacting_with == CROP_RIGHT) {
                slider_set_value(&crop_right, mouse);
            }
        }

        if (IsMouseButtonUp(MOUSE_BUTTON_LEFT)) {
            if(interacting_with == SUBMIT_BTN && CheckCollisionPointRec(mouse, submit_btn.bounds)) {
                FfmpegParams params = {
                    .input_path = input_path,
                    .output_path = output_path,
                    .crf = crf.value,
                    .crop_top = crop_top.value,
                    .crop_bottom = crop_bottom.value,
                    .crop_left = crop_left.value,
                    .crop_right = crop_right.value,
                };
                run_ffmpeg(params);
            }
            interacting_with = NOTHING;
        }

        BeginDrawing();
            ClearBackground(GetColor(0xffffffff));
            DrawText(TextFormat("input path: %s", input_path),
                     center.x - slider_x_offset * 2,
                     center.y - slider_y_offset * 4,
                     20,
                     BLACK);
            DrawText(TextFormat("output path: %s", output_path),
                     center.x - slider_x_offset * 2,
                     center.y - slider_y_offset * 3,
                     20,
                     BLACK);

            slider_draw(&crf, "crf");
            slider_draw(&crop_top, "crop top");
            slider_draw(&crop_bottom, "crop bottom");
            slider_draw(&crop_left, "crop left");
            slider_draw(&crop_right, "crop right");
            button_draw(&submit_btn, interacting_with == SUBMIT_BTN);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
