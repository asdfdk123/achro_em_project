
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <mariadb/mysql.h>

#define BUTTON_DEVICE "/dev/fpga_push_switch"
#define DOT_DEVICE "/dev/fpga_dot"
#define TEXTLCD_DEVICE "/dev/fpga_text_lcd"
#define BUZZER_DEVICE "/dev/fpga_buzzer"
#define FND_DEVICE "/dev/fpga_fnd"
#define LED_DEVICE "/dev/fpga_led"

#define MAX_NOTES 10
#define DOT_ROWS 10
#define DOT_COLS 7

typedef struct {
    int row;
    int col;
    int active;
    int missed;
} Note;

pthread_mutex_t lock;
Note notes[MAX_NOTES];
int score = 0;
int life = 8;
int game_running = 1;
time_t start_time;

void update_led(int life) {
    int led_fd = open(LED_DEVICE, O_WRONLY);
    if (led_fd < 0) { perror("open led"); return; }
    unsigned char value = 0xFF >> (8 - life);
    write(led_fd, &value, 1);
    close(led_fd);
}

void render_dot() {
    int dot_fd = open(DOT_DEVICE, O_WRONLY);
    if (dot_fd < 0) { perror("open dot"); return; }
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
    if (buzzer_fd < 0) { perror("open buzzer"); return; }
    unsigned char on = 1;
    write(buzzer_fd, &on, 1);
    usleep(200000);
    on = 0;
    write(buzzer_fd, &on, 1);
    close(buzzer_fd);
}

void update_textlcd_score(int score) {
    int lcd_fd = open(TEXTLCD_DEVICE, O_WRONLY);
    if (lcd_fd < 0) { perror("open textlcd"); return; }
    char buf[32];
    sprintf(buf, "Score: %d", score);
    write(lcd_fd, buf, strlen(buf));
    close(lcd_fd);
}

void display_gameover() {
    int lcd_fd = open(TEXTLCD_DEVICE, O_WRONLY);
    if (lcd_fd < 0) { perror("open textlcd (game over)"); return; }
    char buf[32] = "   Game Over    ";
    write(lcd_fd, buf, strlen(buf));
    close(lcd_fd);
}

void display_rank_on_lcd(int my_score) {
    MYSQL *conn;
    MYSQL_RES *res;
    MYSQL_ROW row;

    conn = mysql_init(NULL);
    if (conn == NULL) { fprintf(stderr, "mysql_init() failed\n"); return; }

    if (mysql_real_connect(conn, "localhost", "game_user", "1234", "rhythm_game", 0, NULL, 0) == NULL) {
        fprintf(stderr, "mysql_real_connect() failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return;
    }

    if (mysql_query(conn, "SELECT score FROM game ORDER BY score DESC")) {
        fprintf(stderr, "SELECT failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return;
    }

    res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "mysql_store_result() failed\n");
        mysql_close(conn);
        return;
    }

    int rank = 1;
    while ((row = mysql_fetch_row(res))) {
        int s = atoi(row[0]);
        if (s == my_score) break;
        rank++;
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "My Rank: %d", rank);
    int lcd_fd = open(TEXTLCD_DEVICE, O_WRONLY);
    if (lcd_fd >= 0) {
        char clear[32] = "                    ";
        write(lcd_fd, clear, strlen(clear));
        usleep(100000);
        write(lcd_fd, buf, strlen(buf));
        close(lcd_fd);
    }

    mysql_free_result(res);
    mysql_close(conn);
}

void update_fnd(int remaining_sec, int score) {
    int fnd_fd = open(FND_DEVICE, O_WRONLY);
    if (fnd_fd < 0) { perror("open fnd"); return; }
    unsigned char fnd_data[4];
    fnd_data[0] = (remaining_sec / 10) % 10;
    fnd_data[1] = remaining_sec % 10;
    fnd_data[2] = (score / 10) % 10;
    fnd_data[3] = score % 10;
    write(fnd_fd, fnd_data, 4);
    close(fnd_fd);
}

void* note_thread(void* arg) {
    srand(time(NULL));
    int tick = 0;
    while (game_running) {
        pthread_mutex_lock(&lock);
        for (int i = 0; i < MAX_NOTES; i++) {
            if (notes[i].active) {
                notes[i].row++;
                if (notes[i].row >= DOT_ROWS) {
                    if (!notes[i].missed) {
                        life--;
                        update_led(life);
                        if (life <= 0) {
                            game_running = 0;
                            display_gameover();
                            pthread_mutex_unlock(&lock);
                            return NULL;
                        }
                        notes[i].missed = 1;
                    }
                    notes[i].active = 0;
                }
            }
        }
        if (tick % 5 == 0) {
            for (int i = 0; i < MAX_NOTES; i++) {
                if (!notes[i].active) {
                    notes[i].row = 0;
                    notes[i].col = rand() % DOT_COLS;
                    notes[i].active = 1;
                    notes[i].missed = 0;
                    break;
                }
            }
        }
        render_dot();
        pthread_mutex_unlock(&lock);
        usleep(200000);
        tick++;
    }
    return NULL;
}

void* input_thread(void* arg) {
    unsigned char btn[9];
    while (game_running) {
        int btn_fd = open(BUTTON_DEVICE, O_RDONLY);
        if (btn_fd < 0) { perror("open button"); usleep(100000); continue; }
        read(btn_fd, &btn, sizeof(btn));
        close(btn_fd);
        pthread_mutex_lock(&lock);
        for (int i = 0; i < MAX_NOTES; i++) {
            if (notes[i].active && notes[i].row == DOT_ROWS - 1 && btn[notes[i].col] == 1) {
                score++;
                play_buzzer();
                update_textlcd_score(score);
                notes[i].active = 0;
                notes[i].missed = 1;
            }
        }
        pthread_mutex_unlock(&lock);
        usleep(100000);
    }
    return NULL;
}

void* timer_thread(void* arg) {
    while (game_running) {
        int elapsed = time(NULL) - start_time;
        int remaining = 30 - elapsed;
        pthread_mutex_lock(&lock);
        update_fnd(remaining, score);
        pthread_mutex_unlock(&lock);
        if (remaining <= 0) {
            game_running = 0;
            display_gameover();
            break;
        }
        sleep(1);
    }
    return NULL;
}

void save_score_to_db(int score) {
    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL) { fprintf(stderr, "mysql_init() failed\n"); return; }

    if (mysql_real_connect(conn, "localhost", "game_user", "1234", "rhythm_game", 0, NULL, 0) == NULL) {
        fprintf(stderr, "mysql_real_connect() failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return;
    }

    char query[128];
    snprintf(query, sizeof(query), "INSERT INTO game (score) VALUES (%d)", score);
    if (mysql_query(conn, query)) {
        fprintf(stderr, "INSERT failed: %s\n", mysql_error(conn));
    }

    mysql_close(conn);
}

int main() {
    setenv("DISPLAY", ":0", 1);
    pthread_t tid_note, tid_input, tid_timer;
    pthread_mutex_init(&lock, NULL);
    memset(notes, 0, sizeof(notes));
    start_time = time(NULL);
    update_textlcd_score(score);
    update_led(life);
    pthread_create(&tid_note, NULL, note_thread, NULL);
    pthread_create(&tid_input, NULL, input_thread, NULL);
    pthread_create(&tid_timer, NULL, timer_thread, NULL);
    pthread_join(tid_note, NULL);
    pthread_join(tid_input, NULL);
    pthread_join(tid_timer, NULL);
    pthread_mutex_destroy(&lock);
    printf(">> Game Over! Final Score: %d\n", score);
    printf(">> Saving to DB...\n");
    save_score_to_db(score);
    sleep(1);
    display_rank_on_lcd(score);
    printf(">> Saved to DB.\n");
    printf(">> Opening ranking page...\n");
    sleep(1);
    system("chromium-browser --no-sandbox --disable-gpu --disable-software-rasterizer --kiosk http://localhost:5000");
    return 0;
}
