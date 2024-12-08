#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiringPi.h>
#include <wiringSerial.h>
#include <errno.h>
#include <time.h>
#include <math.h>

#define SERIAL_PORT "/dev/ttyACM0"
#define BAUD_RATE 9600

#define THRESHOLD_PROBABILITY 50.0
#define THRESHOLD_HEIGHT 240.0

#define AREA_THRESHOLD 1000
#define DISTANCE_THRESHOLD 500

int fd;

// 메시지 송신 함수
void send_message(int fd, const char *message) {
    serialPuts(fd, message);
    serialPuts(fd, "\n");
}

//폴더 darknet 내부에 있다고 가정
const char *CAM_COMMAND = "sudo v4l2-ctl --device=/dev/video0 --set-fmt-video=width=1920,height=1080,pixelformat=MJPG --stream-mmap --stream-count=1 --stream-to=/home/mkkan/darknet/target.jpg";
const char *YOLO_COMMAND = "./darknet detector test cfg/coco.data cfg/yolov4-tiny.cfg yolov4-tiny.weights target.jpg -dont_show -ext_output > result.txt";

char before_label[50] = "";
int before_center_x = -1;
int before_center_y = -1;
int before_area = -1;

int capture_count = 0;
int catch_target = 0;

// 두 점 간의 거리 계산 함수
double calculateDistance(int x1, int y1, int x2, int y2) {
    return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
}

// 라벨과 중심 좌표, 넓이 비교 및 업데이트 함수
int isSimilarAndUpdate(const char* new_label, int new_center_x, int new_center_y, int new_area) {
    // 라벨이 같은지 확인
    if (strcmp(before_label, new_label) != 0) {
        return 0;
    }

    printf("Before: (%d, %d), New: (%d, %d)\n", before_center_x, before_center_y, new_center_x, new_center_y);

    // 중심 좌표 간의 거리 계산
    double distance = calculateDistance(before_center_x, before_center_y, new_center_x, new_center_y);

    printf("유사 물체 식별 진행: %f\n", distance);

    // 거리 비교 (100 이하인지 확인)
    if (distance <= DISTANCE_THRESHOLD) {
        printf("유사 물체 식별 완료. 비교 진행\n");

        catch_target = 1;

        // 넓이 비교
        if (new_area - before_area > AREA_THRESHOLD) {
            printf("========================접근 중==========================\n");  // 넓이가 일정 값 이상일 경우 출력
            send_message(fd, "D");
        }

        // 조건에 맞으면 기존 변수들을 새로운 값으로 업데이트
        strcpy(before_label, new_label);
        before_center_x = new_center_x;
        before_center_y = new_center_y;
        before_area = new_area;

        return 1; // 조건에 맞아서 업데이트 했음을 반환
    }

    return 0; // 조건에 맞지 않으면 false 반환
}


// 시리얼 포트 초기화 함수
int initialize_serial_connection(const char *port_name, int baud_rate) {
    fd = serialOpen(port_name, baud_rate);
    if (fd < 0) {
        fprintf(stderr, "Unable to open serial device: %s\n", strerror(errno));
        return -1;
    }
    return fd;
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
    //printf("%s", line);

    // 6개의 값 추출: label, probability, left_x, top_y, width, height
    if (sscanf(line, "%49[^:]: %d%% (left_x: %d top_y: %d width: %d height: %d)", label, &probability, &left_x, &top_y, &width, &height) == 6) {
        printf("추출 결과: %s (확률: %d%%, 위치: (%d, %d), 넓이: %d, 높이: %d)\n", label, probability, left_x, top_y, width, height);

        // 조건 확인
        if (is_target_label(label) &&
            probability >= THRESHOLD_PROBABILITY &&
            height >= THRESHOLD_HEIGHT) {

            printf("%s", line);

            // 중심 좌표 계산
            int center_x = left_x + width / 2;
            int center_y = top_y + height / 2;
            int area = width * height;

            printf("중심 좌표: (%d, %d)\n", center_x, center_y);

            if (capture_count < 1){
                if(before_area < area){
                     printf("========================새로운 물체 식별==========================\n");
                    send_message(fd, "D");
                    strcpy(before_label, label);
                    before_center_x = center_x;
                    before_center_y = center_y;
                    before_area = area;
                }
            } else {
                printf("저장 값 존재. 비교 실행\n");
                isSimilarAndUpdate(label, center_x, center_y, area);
            }

            return 1;
        }
    }

    return 0;
}

void process_detection() {
    printf("Before Label: %s\n", before_label);
    printf("Before Center X: %d\n", before_center_x);
    printf("Before Center Y: %d\n", before_center_y);
    printf("Before Area: %d\n", before_area);

    catch_target = 0;

    // 카메라 명령 실행
    if (system(CAM_COMMAND) != 0) {
        fprintf(stderr, "카메라 명령 실행 실패\n");
        return;
    }

    // YOLO 명령 실행
    if (system(YOLO_COMMAND) != 0) {
        fprintf(stderr, "YOLO 명령 실행 실패\n");
        return;
    }

    FILE *file = fopen("result.txt", "r");
    if (!file) {
        perror("파일 열기 실패");
        return;
    }

    
    send_message(fd, "S");


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

    if(catch_target) {
        capture_count = 0;
    }else if(before_area > -1){
        capture_count++;
    }

    if(catch_target < 1 && capture_count > 2){
        strcpy(before_label, "");
        before_center_x = -1;
        before_center_y = -1;
        before_area = -1;

        capture_count = 0;
    }

    printf("Catch_target: %d\n", catch_target);

    if (found) {
        printf("조건을 만족하는 물체가 파일에 존재합니다.\n");
    } else {
        printf("조건을 만족하는 물체가 파일에 없습니다.\n");
    }
}

int main() {
    // 시리얼 포트 초기화
    int fd = initialize_serial_connection(SERIAL_PORT, BAUD_RATE);
    if (fd < 0) {
        return 1; // 초기화 실패 시 프로그램 종료
    }

    printf("시리얼 연결 성공. 명령을 기다립니다...\n");

    // 상태 변수
    int capturing = 0;

    // 메인 루프
    while (1) {
        // 시리얼 입력 대기
        if (serialDataAvail(fd)) {
            char received = serialGetchar(fd); // 수신 데이터 읽기

            if (received == 'S') {  // 'S' 명령 수신 시 시작
                printf("Received 'S'. 촬영 시작...\n");
                capturing = 1;
            } else if (received == 'E') {  // 'E' 명령 수신 시 중지
                printf("Received 'E'. 촬영 중지...\n");
                capturing = 0;
                strcpy(before_label, "");
                before_center_x = -1;
                before_center_y = -1;
                before_area = -1;

                capture_count = 0;
            }
        }

        // 촬영 및 YOLO 실행
        if (capturing) {
            process_detection();
            //delay(5000); // 촬영 간격 5초
        }

        delay(100); // 짧은 지연으로 CPU 사용률 절약
    }

    // 시리얼 포트 닫기
    serialClose(fd);
    return 0;
}
