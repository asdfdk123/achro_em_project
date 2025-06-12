#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define BUTTON_DEVICE "/dev/fpga_push_switch"
#define DOT_DEVICE "/dev/fpga_dot"
#define TEXTLCD_DEVICE "/dev/fpga_text_lcd"
#define BUZZER_DEVICE "/dev/fpga_buzzer"
#define FND_DEVICE "/dev/fpga_fnd"

#define MAX_NOTES 10
#define DOT_ROWS 10
#define DOT_COLS 7

typedef struct {
    int row;
    int col;
    int active;
} Note;

pthread_mutex_t lock;

Note notes[MAX_NOTES];
int score = 0;
int game_running = 1;
time_t start_time;