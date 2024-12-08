#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiringPi.h>
#include <wiringSerial.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <curl/curl.h>

#define SERIAL_PORT "/dev/ttyACM0"
#define BAUD_RATE 9600

#define THRESHOLD_PROBABILITY 50.0
#define THRESHOLD_HEIGHT 240.0

#define AREA_THRESHOLD 1000
#define DISTANCE_THRESHOLD 1000

int fd;
pthread_mutex_t serial_mutex = PTHREAD_MUTEX_INITIALIZER;  // 시리얼 포트 잠금용 mutex

int capturing = 0;

// 현재 시간 문자열을 반환하는 함수
char* get_current_time_str() {
    time_t t;
    struct tm* tm_info;
    static char time_str[20];  // "YYYY-MM-DD HH:MM:SS" 형식으로 출력

    time(&t);
    tm_info = localtime(&t);

    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    return time_str;
}

// HTTP 요청을 보내는 함수
void send_http_request(const char *url) {
    CURL *curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);

        // 요청 보내기
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "HTTP 요청 실패: %s\n", curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
}


// 메시지 송신 함수
void send_message(int fd, const char *message) {
    pthread_mutex_lock(&serial_mutex); // 메시지 송신 전에 잠금
    serialPuts(fd, message);
    serialPuts(fd, "\n");
    pthread_mutex_unlock(&serial_mutex); // 송신 후 잠금 해제
}

// 폴더 darknet 내부에 있다고 가정
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
    if (strcmp(before_label, new_label) != 0) {
        return 0;
    }

    printf("Before: (%d, %d), New: (%d, %d)\n", before_center_x, before_center_y, new_center_x, new_center_y);

    double distance = calculateDistance(before_center_x, before_center_y, new_center_x, new_center_y);

    printf("유사 물체 식별 진행: %f\n", distance);

    if (distance <= DISTANCE_THRESHOLD) {
        printf("유사 물체 식별 완료. 비교 진행\n");

        catch_target = 1;

        if (new_area - before_area > AREA_THRESHOLD) {
            printf("========================접근 중==========================\n");
            send_message(fd, "D");
        }

        strcpy(before_label, new_label);
        before_center_x = new_center_x;
        before_center_y = new_center_y;
        before_area = new_area;

        return 1;
    }

    return 0;
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

    if (sscanf(line, "%49[^:]: %d%% (left_x: %d top_y: %d width: %d height: %d)", label, &probability, &left_x, &top_y, &width, &height) == 6) {
        printf("추출 결과: %s (확률: %d%%, 위치: (%d, %d), 넓이: %d, 높이: %d)\n", label, probability, left_x, top_y, width, height);

        if (is_target_label(label) &&
            probability >= THRESHOLD_PROBABILITY &&
            height >= THRESHOLD_HEIGHT) {

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

    if (system(CAM_COMMAND) != 0) {
        fprintf(stderr, "카메라 명령 실행 실패\n");
        return;
    }

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

    while (fgets(line, sizeof(line), file)) {
        if (parse_and_check_line(line)) {
            found = 1;
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

void* serial_thread(void* arg) {
    while (1) {
        if (serialDataAvail(fd)) {
            char received = serialGetchar(fd);
            if (received == 'S') {
                printf("Received 'S'. 촬영 시작...\n");
                capturing = 1;
            } else if (received == 'E') {
                printf("Received 'E'. 촬영 중지...\n");
                capturing = 0;
                strcpy(before_label, "");
                before_center_x = -1;
                before_center_y = -1;
                before_area = -1;

                capture_count = 0;
            } else if (received == 'O') { 

                printf("Received 'E'. 촬영 중지...\n");
                capturing = 0;
                strcpy(before_label, "");
                before_center_x = -1;
                before_center_y = -1;
                before_area = -1;


                capture_count = 0;

                printf("Received 'O'. 현재 시간 전송...\n");
                char url[256];
                sprintf(url, "http://59.187.251.226:54549/car/open?key=%s", get_current_time_str());
                send_http_request(url);  // HTTP 요청 보내기
            } else if (received == 'D') { 
                printf("Received 'E'. 촬영 중지...\n");
                capturing = 0;
                strcpy(before_label, "");
                before_center_x = -1;
                before_center_y = -1;
                before_area = -1;


                capture_count = 0;

                printf("Received 'D'. 현재 시간 전송...\n");
                char url[256];
                sprintf(url, "http://59.187.251.226:54549/car/danger?key=%s", get_current_time_str());
                send_http_request(url);  // HTTP 요청 보내기
            }

            while (serialDataAvail(fd)) {
                serialGetchar(fd);
            }
        }

        delay(100); // 짧은 지연으로 CPU 사용률 절약
    }
    return NULL;
}

int main() {
    int fd = initialize_serial_connection(SERIAL_PORT, BAUD_RATE);
    if (fd < 0) {
        return 1; // 초기화 실패 시 프로그램 종료
    }

    pthread_t serial_tid;
    pthread_create(&serial_tid, NULL, serial_thread, NULL); // 시리얼 통신을 처리하는 스레드 생성

    printf("시리얼 연결 성공. 명령을 기다립니다...\n");

    while (1) {
        if (capturing) {
            process_detection();
        }

        delay(100); // 짧은 지연으로 CPU 사용률 절약
    }

    pthread_join(serial_tid, NULL); // 시리얼 스레드 종료 대기
    serialClose(fd);
    return 0;
}
