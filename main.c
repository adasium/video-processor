#include "raylib.h"
#include <stdio.h>
#include <string.h>

#define MAX_FILEPATH_RECORDED   4096
#define MAX_FILEPATH_SIZE       2048

#define MAX_CRF 64

void draw_slider(int x, int y, int width, float percentage) {
    const int button_width = 10;
    const int button_height = 10;
    const int line_thickness = 2;
    DrawRectangle(x, y, width, line_thickness, LIGHTGRAY);
    DrawRectangle(x+width*percentage/100.0f-button_width/2, y-button_height/2, button_width, button_height, GRAY);
}

typedef enum {
    NOTHING,
    CRF,
} InteractingWith;


float clampf(float value, float min, float max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}


int main(void)
{
    InitWindow(800, 450, "video-processor");
    SetWindowMonitor(0);
    SetTargetFPS(60);

    bool exit_window = false;
    char file_path[MAX_FILEPATH_SIZE] = "";
    InteractingWith interacting_with = NOTHING;

    int crf = 28;
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

        Rectangle slider_bounds = {
            100,
            200,
            100,
            20
        };
        Rectangle slider_btn = {
            slider_bounds.x-5 + slider_bounds.width*crf/MAX_CRF,
            slider_bounds.y-5,
            10,
            10
        };
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();
            printf("%f %f %f %f | %f %f\n", slider_btn.x, slider_btn.y, slider_btn.width, slider_btn.height, mouse.x, mouse.y);

            if (interacting_with == NOTHING) {
                if(CheckCollisionPointRec(mouse, slider_btn)) {
                    interacting_with = CRF;
                }
            }

            if (interacting_with == CRF) {
                float value = clampf(mouse.x, slider_bounds.x, slider_bounds.x+slider_bounds.width) - slider_bounds.x;
                value /= slider_bounds.width;
                value *= MAX_CRF;
                crf = (int)value;
            }
        }

        if (IsMouseButtonUp(MOUSE_BUTTON_LEFT)) {
            interacting_with = NOTHING;
        }



        BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText(TextFormat("path: %s", file_path), 100, 100, 20, BLACK);
            DrawText(TextFormat("CRF: %d", crf), 100, 170, 20, BLACK);
            draw_slider(100, 200, 100, 100.f*crf/MAX_CRF);
            DrawRectangleLinesEx(submit_btn, 2, BLACK);

            int size = MeasureText("run", 10);
            DrawText(
                     "run",
                     submit_btn.x + submit_btn.width/3 - size/2,
                     submit_btn.y + submit_btn.height/4,
                     crf/MAX_CRF,
                     BLACK);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
