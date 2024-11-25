#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiringPi.h>
#include <wiringSerial.h>
#include <errno.h>
#include <time.h>

#define SERIAL_PORT "/dev/ttyACM0"
#define BAUD_RATE 9600

#define THRESHOLD_PROBABILITY 20.0
#define THRESHOLD_HEIGHT 240.0

//폴더 darknet 내부에 있다고 가정

const char *CAM_COMMAND = "sudo v4l2-ctl --device=/dev/video0 --set-fmt-video=width=1920,height=1080,pixelformat=MJPG --stream-mmap --stream-count=1 --stream-to=/home/mkkan/darknet/target.jpg";
const char *YOLO_COMMAND = "./darknet detector test cfg/coco.data cfg/yolov3-tiny.cfg yolov3-tiny.weights target.jpg -dont_show -ext_output > result.txt";

int fd;

// 시리얼 포트 초기화 함수
int initialize_serial_connection(const char *port_name, int baud_rate) {
    int fd = serialOpen(port_name, baud_rate);
    if (fd < 0) {
        fprintf(stderr, "Unable to open serial device: %s\n", strerror(errno));
        return -1;
    }
    return fd;
}

// 메시지 송신 함수
void send_message(int fd, const char *message) {
    serialPuts(fd, message);
    serialPuts(fd, "\n");
}

// 조건에 맞는 Label인지 확인
int is_target_label(const char *label) {
    return (strcmp(label, "person") == 0 ||
            strcmp(label, "bicycle") == 0 ||
            strcmp(label, "car") == 0 ||
            strcmp(label, "motorcycle") == 0);
}

int parse_and_check_line(const char *line) {
    char label[50];
    int probability = 0, left_x = 0, top_y = 0, width = 0, height = 0;

    // 라인 출력 (디버깅용)
    printf("%s", line);

    // 6개의 값 추출: label, probability, left_x, top_y, width, height
    if (sscanf(line, "%49[^:]: %d%% (left_x: %d top_y: %d width: %d height: %d)", label, &probability, &left_x, &top_y, &width, &height) == 6) {
        printf("추출 결과: %s (확률: %d%%, 위치: (%d, %d), 넓이: %d, 높이: %d)\n", label, probability, left_x, top_y, width, height);



        // 조건 확인
        if (is_target_label(label) &&
            probability >= THRESHOLD_PROBABILITY &&
            height >= THRESHOLD_HEIGHT) {
            // 중심 좌표 계산
            int center_x = left_x + width / 2;
            int center_y = top_y + height / 2;
            printf("중심 좌표: (%d, %d)\n", center_x, center_y);
            return 1;
        }
    }

    return 0;
}

int main() {
    // 시리얼 포트 초기화
    int fd = initialize_serial_connection(SERIAL_PORT, BAUD_RATE);
    if (fd < 0) {
        return 1; // 초기화 실패 시 프로그램 종료
    }

    printf("시리얼 연결 성공. 명령을 기다립니다...\n");

    // 무한 대기 상태
    while (1) {
        // 시리얼 입력 대기
        if (serialDataAvail(fd)) {
            char received = serialGetchar(fd); // 수신 데이터 읽기

            if (received == 'S') {  // 'S' 명령 수신 시 실행
                printf("Received 'S'. 명령 실행 시작...\n");
                
                // 카메라 명령 실행
                int cam_status = system(CAM_COMMAND);
                if (cam_status != 0) {
                    fprintf(stderr, "카메라 명령 실행 실패\n");
                    continue;
                }

                // YOLO 명령 실행
                int yolo_status = system(YOLO_COMMAND);
                if (yolo_status != 0) {
                    fprintf(stderr, "YOLO 명령 실행 실패\n");
                    continue;
                }

                FILE *file = fopen("result.txt", "r");
                if (!file) {
                    perror("파일 열기 실패");
                    return EXIT_FAILURE;
                }

                char line[1024];
                int found = 0;

                // 파일에서 한 줄씩 읽기
                while (fgets(line, sizeof(line), file)) {
                    // 조건을 만족하는 줄이 있는지 확인
                    if (parse_and_check_line(line)) {
                        found = 1; // 조건 만족 표시
                    }
                }

                fclose(file);

                if (found) {
                    printf("조건을 만족하는 물체가 파일에 존재합니다.\n");
                    send_message(fd, "조건을 만족하는 물체가 감지되었습니다.");
                } else {
                    printf("조건을 만족하는 물체가 파일에 없습니다.\n");
                    send_message(fd, "조건을 만족하는 물체가 없습니다.");
                }                
            }
        }

        delay(100); // 짧은 지연으로 CPU 사용률 절약
    }

    // 시리얼 포트 닫기
    serialClose(fd);
    return 0;
}