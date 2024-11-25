#define BUTTON_PIN 2 // 푸시 버튼이 연결된 핀 번호

bool lastButtonState = HIGH;  // 버튼이 눌리지 않았을 때의 상태 (내부 풀업 사용으로 기본 HIGH)
bool currentButtonState = HIGH; // 현재 버튼 상태

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP); // 내부 풀업 저항을 사용하여 버튼 핀 설정
  Serial.begin(9600);               // 시리얼 통신 시작
}

void loop() {
  currentButtonState = digitalRead(BUTTON_PIN); // 버튼 상태 읽기

  if (currentButtonState != lastButtonState) {
    if (currentButtonState == LOW) { 
      Serial.println("E"); 
    } else { 
      Serial.println("S"); 
    }
  }

  lastButtonState = currentButtonState; // 이전 버튼 상태 업데이트
}
