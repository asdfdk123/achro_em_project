#include <stdio.h> // 표준 입출력 사용
#include <stdlib.h> // 동적 메모리, 난수 생성 등
#include <pthread.h> // POSIX 쓰레드 사용
#include <fcntl.h> // open() 함수의 옵션 설정
#include <unistd.h> // close(), usleep() 등 시스템 호출
#include <string.h> // 문자열 처리 함수
#include <time.h> // 시간 관련 함수 (게임 시간 계산)
#include <mariadb/mysql.h> // MariaDB 연동

// 각 FPGA 디바이스 파일 경로 정의
#define BUTTON_DEVICE "/dev/fpga_push_switch"
#define DOT_DEVICE "/dev/fpga_dot"
#define TEXTLCD_DEVICE "/dev/fpga_text_lcd"
#define BUZZER_DEVICE "/dev/fpga_buzzer"
#define FND_DEVICE "/dev/fpga_fnd"
#define LED_DEVICE "/dev/fpga_led"

#define MAX_NOTES 10 // 동시에 존재할 수 있는 최대 노트 수
#define DOT_ROWS 10 // 도트 매트릭스의 행 수
#define DOT_COLS 7  // 도트 매트릭스의 열 수

// 노트 구조체: 떨어지는 노트의 상태 정보
typedef struct {
    int row;    // 현재 행 위치
    int col;    // 현재 열 위치
    int active; // 노트가 활성 상태인지 여부
    int missed; // 사용자가 놓쳤는지 여부
} Note;

// 전역 변수들
pthread_mutex_t lock; // 쓰레드 동기화용 뮤텍스
Note notes[MAX_NOTES]; // 노트 배열
int score = 0;         // 현재 점수
int life = 8;          // 남은 생명 (LED로 표현)
int game_running = 1;  // 게임 실행 여부 플래그
time_t start_time;     // 게임 시작 시간

// 생명 수에 따라 LED 점등
void update_led(int life) {
    int led_fd = open(LED_DEVICE, O_WRONLY);
    if (led_fd < 0) { perror("open led"); return; }
    unsigned char value = 0xFF >> (8 - life); // 오른쪽부터 켜짐
    write(led_fd, &value, 1);
    close(led_fd);
}

// notes 배열을 기반으로 도트 매트릭스 렌더링
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

// 부저 소리 출력 (0.2초)
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
