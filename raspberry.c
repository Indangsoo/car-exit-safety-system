#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiringPi.h>
#include <wiringSerial.h>
#include <errno.h>
#include <time.h>

#define SERIAL_PORT "/dev/ttyACM0"
#define BAUD_RATE 9600

//폴더 darknet 내부에 있다고 가정

const char *CAM_COMMAND = "sudo v4l2-ctl --device=/dev/video0 --set-fmt-video=width=1920,height=1080,pixelformat=MJPG --stream-mmap --stream-count=1 --stream-to=/home/mkkan/darknet/target.jpg";
const char *YOLO_COMMAND = "./darknet detector test cfg/coco.data cfg/yolov3-tiny.cfg yolov3-tiny.weights target.jpg > result.txt";

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
void send(int fd, const char *message) {
    serialPuts(fd, message);
    serialPuts(fd, "\n");
}

int main() {
}