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

void render_dot() {
    int dot_fd = open(DOT_DEVICE, O_WRONLY);
    if (dot_fd < 0) {
        perror("open dot");
        return;
    }

    unsigned char dot[DOT_ROWS] = {0};
    for (int i = 0; i < MAX_NOTES; i++) {
        if (notes[i].active && notes[i].row >= 0 && notes[i].row < DOT_ROWS && notes[i].col >= 0 && notes[i].col < DOT_COLS) {
            dot[notes[i].row] |= (1 << (6 - notes[i].col));
        }
    }

    write(dot_fd, dot, sizeof(dot));
    close(dot_fd);
}
void play_buzzer() {
    int buzzer_fd = open(BUZZER_DEVICE, O_WRONLY);
    if (buzzer_fd < 0) {
        perror("open buzzer");
        return;
    }

    unsigned char on = 1;
    write(buzzer_fd, &on, 1);
    usleep(200000);
    on = 0;
    write(buzzer_fd, &on, 1);
    close(buzzer_fd);
}

void update_textlcd_score(int score) {
    int lcd_fd = open(TEXTLCD_DEVICE, O_WRONLY);
    if (lcd_fd < 0) {
        perror("open textlcd");
        return;
    }

    char buf[32];
    sprintf(buf, "Score: %d", score);
    write(lcd_fd, buf, strlen(buf));
    close(lcd_fd);
}

void display_gameover() {
    int lcd_fd = open(TEXTLCD_DEVICE, O_WRONLY);
    if (lcd_fd < 0) {
        perror("open textlcd (game over)");
        return;
    }

    char buf[32] = "   Game Over    ";
    write(lcd_fd, buf, strlen(buf));
    close(lcd_fd);
}

void update_fnd(int remaining_sec, int score) {
    int fnd_fd = open(FND_DEVICE, O_WRONLY);
    if (fnd_fd < 0) {
        perror("open fnd");
        return;
    }

    unsigned char fnd_data[4];
    fnd_data[0] = (remaining_sec / 10) % 10;
    fnd_data[1] = remaining_sec % 10;
    fnd_data[2] = (score / 10) % 10;
    fnd_data[3] = score % 10;

    write(fnd_fd, fnd_data, 4);
    close(fnd_fd);
}
