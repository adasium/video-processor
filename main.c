#include "./thirdparty/raylib/src/raylib.h"
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

#define DEBUG 1

#define ARRAY_LEN(array) (sizeof(array)/sizeof(array[0]))

float clampf(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

int clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

int min(int a, int b) {
    return a < b ? a : b;
}

int max(int a, int b) {
    return a > b ? a : b;
}

const char *EXTENSIONS[] = {
    ".mp4",
    ".avi",
};


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

typedef enum {
    NO_MODIFICATION = 1,
    CLONE_LEFT,
    CLONE_RIGHT,
    AUDIO_CHANNELS_LAST,
} AudioChannels;

typedef struct {
    const char **items;
    size_t count;
    size_t capacity;
} Labels;

#define da_append(da, item)                    \
    do {                                       \
        nob_da_reserve((da), (da)->count + 1); \
        (da)->items[(da)->count++] = (item);   \
    } while (0)

Labels audio_channel_labels = {0};

typedef struct {
    char* input_path;
    char* output_path;
    int crf;
    int crop_top;
    int crop_bottom;
    int crop_left;
    int crop_right;
    int volume;
    AudioChannels audio_channels;
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
        params.crf / 10 + '0',
        (params.crf) % 10 + '0',
    };

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "ffmpeg", "-y");
    nob_cmd_append(&cmd, "-i", params.input_path);
    nob_cmd_append(&cmd, "-crf", crf_str);

    char crop[255] = {0};
    if (params.crop_top | params.crop_bottom | params.crop_left | params.crop_right) {
        sprintf(
                crop,
                "crop=in_w-%d:in_h-%d:%d:%d",
                params.crop_left + params.crop_right,
                params.crop_top + params.crop_bottom,
                params.crop_left,
                params.crop_top
                );
        nob_cmd_append(&cmd, "-vf", crop);
    }

    char audio[255] = {0};
    if (params.audio_channels == CLONE_LEFT) {
        char* option = "pan=stereo|FL=FL|FR=FL";
        snprintf(audio, strlen(option) + 1, "%s", option);
    }
    if (params.audio_channels == CLONE_RIGHT) {
        char* option ="pan=stereo|FL=FR|FR=FR";
        snprintf(audio, strlen(option) + 1, "%s", option);
    }

    if (strlen(audio) > 0) snprintf(audio + strlen(audio), 2, ",");
    snprintf(audio + strlen(audio), strlen("volume=")+5, "volume=%.2f", (float)params.volume/100);

    if (strlen(audio) > 0) nob_cmd_append(&cmd, "-af", audio);


    nob_cmd_append(&cmd, params.output_path);
    nob_cmd_append(&cmd, "-progress pipe:1");

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

    fflush(stdout);
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

typedef enum {
    NOTHING,
    BACKGROUND,
    SLIDER,
    BUTTON,
    RADIO_GROUP,
} UIElement;

typedef enum {
    AUDIO_CHANNELS,
} EnumKind;


typedef struct {
    Rectangle bounds;
    char *label;
    Labels labels;
    int font_size;
    int selected_value;
    int first_value;
    int last_value;
    EnumKind enum_kind;
    int button_radius;
    int spacing;
    int padding;
} RadioGroup;


int radio_group_check_collision_point(RadioGroup *radio_group, Vector2 mouse) {
    int spacing = (*radio_group).font_size/DEFAULT_FONT_SIZE;
    float cum_text_size_y = 0;
    for (int option = (*radio_group).first_value, i = 0; option < (*radio_group).last_value; ++option, ++i) {
        Vector2 text_size = MeasureTextEx(GetFontDefault(), (*radio_group).labels.items[i], (*radio_group).font_size, (float)spacing);
        if(CheckCollisionPointRec(
                                  mouse,
                                  (Rectangle){
                                      (*radio_group).bounds.x + (*radio_group).padding,
                                      (*radio_group).bounds.y + (*radio_group).padding + cum_text_size_y + (*radio_group).spacing * i,
                                      text_size.x + (*radio_group).button_radius * 2 + (*radio_group).spacing,
                                      text_size.y,
                                  }
                                  )) {
               return option;
        }
        cum_text_size_y += text_size.y;
    }
    return false;
}

void radio_group_set_value(RadioGroup *radio_group, Vector2 mouse) {
    int option = radio_group_check_collision_point(radio_group, mouse);
    if (option) {
        (*radio_group).selected_value = option;
    }
}


void radio_group_draw(RadioGroup *radio_group) {
    int line_thickness = 1;
    int padding = 10;


    int spacing = (*radio_group).font_size/DEFAULT_FONT_SIZE;
    Vector2 max_size = {0, 0};
    for (size_t i = 0; i < (*radio_group).labels.count; ++i) {
        Vector2 text_size = MeasureTextEx(GetFontDefault(), (*radio_group).labels.items[i], (*radio_group).font_size, (float)spacing);
        if (max_size.x <= text_size.x) max_size.x = text_size.x;
        if (max_size.y <= text_size.y) max_size.y = text_size.y;
    }

    float cum_text_size_y = 0;
    for (int option = (*radio_group).first_value, i = 0; option < (*radio_group).last_value; ++option, ++i) {
        (option == (*radio_group).selected_value ? DrawCircle : DrawCircleLines)
            (
             (*radio_group).bounds.x + (*radio_group).button_radius + padding,
             (*radio_group).bounds.y + (*radio_group).button_radius + padding + cum_text_size_y + i * (*radio_group).spacing,
             (*radio_group).button_radius,
             BLACK);

        Vector2 text_size = MeasureTextEx(GetFontDefault(), (*radio_group).labels.items[i], (*radio_group).font_size, (float)spacing);
        DrawText(
                 (*radio_group).labels.items[i],
                 (*radio_group).bounds.x + padding + (*radio_group).button_radius*2 + (*radio_group).spacing,
                 (*radio_group).bounds.y + padding + cum_text_size_y + (*radio_group).spacing * i,
                 (*radio_group).font_size,
                 BLACK);
        cum_text_size_y += text_size.y;
    }

    DrawRectangleLinesEx(
                         (Rectangle){
                             (*radio_group).bounds.x,
                             (*radio_group).bounds.y,
                             max_size.x + (*radio_group).button_radius * 2 + padding * 2 + padding,
                             cum_text_size_y + (*radio_group).spacing * ((*radio_group).labels.count - 1) + padding * 2,
                         },
                         line_thickness,
                         BLACK);
}




typedef struct {
    UIElement type;
    union {
        Button *button;
        Slider *slider;
        RadioGroup *radio_group;
    };
} InteractingWith;


int main(void)
{

    da_append(&audio_channel_labels, "NO MODIFICATION");
    da_append(&audio_channel_labels, "CLONE LEFT");
    da_append(&audio_channel_labels, "CLONE RIGHT");

    InitWindow(800, 450, "video-processor");
    Image icon = LoadImage("assets/icons/video-processor.png");
    SetWindowIcon(icon);
    SetWindowMonitor(0);
    SetTargetFPS(60);

    bool exit_window = false;
    char input_path[MAX_FILEPATH_SIZE] = "";
    char output_path[MAX_FILEPATH_SIZE] = "";
    set_input_path(input_path);
    set_output_path(output_path, input_path);
    InteractingWith interacting_with = {NOTHING, {0}};
    InteractingWith last_interacted_with = {NOTHING, {0}};
    Vector2 center = {
        GetScreenWidth() / 2,
        GetScreenHeight() / 2,
    };
    int slider_width = 150;
    int slider_height = 20;
    Vector2 slider_start = {50, 100};
    int slider_x_offset = slider_width + 50;
    int slider_y_offset = 50;
    Slider crf = {
        .bounds = {
            slider_start.x + slider_x_offset,
            slider_start.y + slider_y_offset * 2,
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
            slider_start.x + slider_x_offset,
            slider_start.y + slider_y_offset,
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
            slider_start.x + slider_x_offset,
            slider_start.y + slider_y_offset * 3,
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
            slider_start.x,
            slider_start.y + slider_y_offset * 2,
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
            slider_start.x + slider_x_offset * 2,
            slider_start.y + slider_y_offset * 2,
            slider_width,
            slider_height,
        },
        .min = 0,
        .max = 1000,
        .value = 0,
        .step = 50,
    };
    Slider volume = {
        .bounds = {
            slider_start.x + slider_x_offset,
            slider_start.y + slider_y_offset * 4,
            slider_width,
            slider_height,
        },
        .min = 1,
        .max = 150,
        .value = 100,
        .step = 5,
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

    Slider *sliders[] = {
        &crf,
        &crop_top,
        &crop_bottom,
        &crop_left,
        &crop_right,
        &volume,
    };
    RadioGroup audio_channnels_radio_group = {
        .bounds = {
            .x = 20,
            .y = 300,
            .width = 100,
            .height = 50,
        },
        .label = "audio channels",
        .labels = audio_channel_labels,
        .font_size = 12,
        .selected_value = NO_MODIFICATION,
        .first_value = NO_MODIFICATION,
        .last_value = AUDIO_CHANNELS_LAST,
        .enum_kind = AUDIO_CHANNELS,
        .button_radius = 5,
        .spacing = 5,
        .padding = 10,
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
            /* if (DEBUG) slider_debug(&crf); */
            if (interacting_with.type == NOTHING) {
                for (size_t i = 0; i < ARRAY_LEN(sliders); ++i) {
                    if(slider_check_collision_point(sliders[i], mouse)) {
                        interacting_with.type = SLIDER;
                        interacting_with.slider = sliders[i];
                        goto interacted;
                    }
                }

                if(CheckCollisionPointRec(mouse, submit_btn.bounds)) {
                    interacting_with.type = BUTTON;
                    interacting_with.button = &submit_btn;
                    goto interacted;
                }

                int r_option = radio_group_check_collision_point(&audio_channnels_radio_group, mouse);
                if(r_option) {
                    interacting_with.type = RADIO_GROUP;
                    interacting_with.radio_group = &audio_channnels_radio_group;
                    goto interacted;
                }
                interacting_with.type = BACKGROUND;
                goto interacted;
            }
        interacted:
            last_interacted_with = interacting_with;

            if (interacting_with.type == SLIDER) {
                slider_set_value(interacting_with.slider, mouse);
            }
        }
        if (IsKeyPressed(KEY_LEFT) && last_interacted_with.type == SLIDER) {
            last_interacted_with.slider->value = max(last_interacted_with.slider->value - last_interacted_with.slider->step, last_interacted_with.slider->min);
        }
        if (IsKeyPressed(KEY_RIGHT) && last_interacted_with.type == SLIDER) {
            last_interacted_with.slider->value = min(last_interacted_with.slider->value + last_interacted_with.slider->step, last_interacted_with.slider->max);
        }

        float mwheel_move = GetMouseWheelMove();
        if (mwheel_move != 0) {
            for (size_t i = 0; i < ARRAY_LEN(sliders); ++i) {
                if(slider_check_collision_point(sliders[i], mouse)) {
                    sliders[i]->value = clamp(
                                              sliders[i]->value + sliders[i]->step * (mwheel_move > 0 ? -1 : 1),
                                              sliders[i]->min,
                                              sliders[i]->max);
                }
            }

        }

        if (IsMouseButtonUp(MOUSE_BUTTON_LEFT)) {
            if(interacting_with.type == RADIO_GROUP) {
                radio_group_set_value(interacting_with.radio_group, mouse);
            }
            if(interacting_with.type == BUTTON && interacting_with.button == &submit_btn && CheckCollisionPointRec(mouse, submit_btn.bounds)) {
                FfmpegParams params = {
                    .input_path = input_path,
                    .output_path = output_path,
                    .crf = crf.value,
                    .crop_top = crop_top.value,
                    .crop_bottom = crop_bottom.value,
                    .crop_left = crop_left.value,
                    .crop_right = crop_right.value,
                    .volume = volume.value,
                    .audio_channels = audio_channnels_radio_group.selected_value,
                };
                run_ffmpeg(params);
            }
            interacting_with.type = NOTHING;
        }

        BeginDrawing();
            ClearBackground(GetColor(0xffffffff));
            DrawText(TextFormat("input path: %s", input_path),
                     0,
                     0,
                     18,
                     BLACK);
            DrawText(TextFormat("output path: %s", output_path),
                     0,
                     50,
                     18,
                     BLACK);

            slider_draw(&crf, "crf");
            slider_draw(&crop_top, "crop top");
            slider_draw(&crop_bottom, "crop bottom");
            slider_draw(&crop_left, "crop left");
            slider_draw(&crop_right, "crop right");
            slider_draw(&volume, "volume");
            button_draw(&submit_btn, interacting_with.type == BUTTON && interacting_with.button == &submit_btn);
            radio_group_draw(&audio_channnels_radio_group);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
