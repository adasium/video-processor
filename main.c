#include "raylib.h"
#include <stdio.h>
#include <string.h>

#define NOB_IMPLEMENTATION
#include "./thirdparty/nob.h"

#define MAX_FILEPATH_RECORDED   4096
#define MAX_FILEPATH_SIZE       2048

#define MAX_CRF 64
#define SLIDER_BUTTON_SIZE 10

float clampf(float value, float min, float max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}



typedef enum {
    NOTHING,
    BACKGROUND,
    CRF,
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
    (*slider).value = (int)value;
}


void slider_draw(Slider *slider) {
    const int line_thickness = 2;
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



int run_ffmpeg(char* path, int crf) {

    if(!strlen(path)) {
        printf("[INFO] no file is selected.\n");
        return 1;
    }

    printf("[INFO] selected file: %s\n", path);

    char crf_str[3] = {
        crf % 10 + '0',
        crf / 10 + '0',
    };

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "ffmpeg");
    nob_cmd_append(&cmd, "-i", path);
    nob_cmd_append(&cmd, "-crf", crf_str);
    nob_cmd_append(&cmd, "output.mp4");
    if (!nob_cmd_run(&cmd)) return 1;
    return 0;
}


int main(void)
{
    InitWindow(800, 450, "video-processor");
    SetWindowMonitor(0);
    SetTargetFPS(60);

    bool exit_window = false;
    char file_path[MAX_FILEPATH_SIZE] = "";
    InteractingWith interacting_with = NOTHING;

    Slider crf = {
        .bounds = {
            100,
            200,
            100,
            20
        },
        .min = 1,
        .max = 64,
        .value = 28,
        .step = 1,
    };
    Rectangle submit_btn = {
        100,
        250,
        100,
        50
    };
    while (!exit_window)
    {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q) || WindowShouldClose()) exit_window = true;

        if (IsFileDropped()) {
            FilePathList dropped_files = LoadDroppedFiles();
            if (dropped_files.count > 0) {
                strcpy(file_path, dropped_files.paths[0]);
            }
            UnloadDroppedFiles(dropped_files);
        }

        Vector2 mouse = GetMousePosition();
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            slider_debug(&crf);
            while (1) {
                if (interacting_with == NOTHING) {
                    if(slider_check_collision_point(&crf, mouse)) {
                        interacting_with = CRF;
                        break;
                    }
                    if(CheckCollisionPointRec(mouse, submit_btn)) {
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
        }

        if (IsMouseButtonUp(MOUSE_BUTTON_LEFT)) {
            if(interacting_with == SUBMIT_BTN && CheckCollisionPointRec(mouse, submit_btn)) {
                run_ffmpeg(file_path, crf.value);
            }
            interacting_with = NOTHING;
        }

        BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText(TextFormat("path: %s", file_path), 100, 100, 20, BLACK);
            DrawText(TextFormat("CRF: %d", crf.value), 100, 170, 20, BLACK);
            slider_draw(&crf);
            if (interacting_with == SUBMIT_BTN) {
                DrawRectangleRec(submit_btn, GRAY);
            }
            DrawRectangleLinesEx(submit_btn, 2, BLACK);

            int size = MeasureText("run", 10);
            DrawText(
                     "run",
                     submit_btn.x + submit_btn.width/3 - size/2,
                     submit_btn.y + submit_btn.height/4,
                     28,
                     BLACK);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
