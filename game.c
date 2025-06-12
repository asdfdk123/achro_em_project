#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#define BUTTON_DEVICE "/dev/fpga_push_switch"
#define BUTTON_NUM 9
#define NOTE_NUM 10
#define NOTE_DELAY 1000000  // 1초

int read_button() {
    unsigned char buttons[BUTTON_NUM] = {0};
    int fd = open(BUTTON_DEVICE, O_RDONLY);
    if (fd < 0) {
        perror("버튼 디바이스 열기 실패");
        return -1;
    }

    int read_bytes = read(fd, buttons, sizeof(buttons));
    if (read_bytes != BUTTON_NUM) {
        perror("버튼 입력 읽기 실패");
        close(fd);
        return -1;
    }

    close(fd);

    for (int i = 0; i < BUTTON_NUM; i++) {
        if (buttons[i]) {
            return i;
        }
    }

    return -1;  // 아무 버튼도 안 눌림
}

int main() {
    srand(time(NULL));
    printf("리듬 게임 시작!\n");

    for (int i = 0; i < NOTE_NUM; i++) {
        int target = rand() % BUTTON_NUM;
        printf("노트 %d 떨어짐! (타겟: %d번 버튼)\n", i + 1, target);

        int elapsed = 0;
        int hit = 0;
        while (elapsed < 30) {  // 약 3초 대기 (100ms * 30)
            int btn = read_button();
            if (btn == target) {
                printf("Perfect!\n");
                hit = 1;
                break;
            } else if (btn != -1) {
                printf("Miss! (입력: %d)\n", btn);
                hit = 1;
                break;
            }
            usleep(100000);  // 100ms
            elapsed++;
        }

        if (!hit) {
            printf("Time Over!\n");
        }

        usleep(NOTE_DELAY);
    }

    printf("게임 종료!\n");
    return 0;
}
