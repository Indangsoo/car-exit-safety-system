#define BUTTON_PIN 2   // 푸시 버튼이 연결된 핀 번호
#define BUTTON_3_PIN 3 // 새로운 푸시 버튼이 연결된 핀 번호
#define BUZZER_PIN 4   // 부저가 연결된 핀 번호
#define LED_PIN 12     // LED가 연결된 핀 번호
#define LED_D_PIN 13   // "D" 명령어로 제어할 LED 핀 번호
#define HIGH_FREQUENCY 1000  // 높은 주파수 (단위: Hz)
#define LOW_FREQUENCY 500    // 낮은 주파수 (단위: Hz)
#define DELAY_TIME 300       // 각 주파수 사이의 딜레이 시간 (단위: 밀리초)

bool lastButtonState = LOW;  // 버튼이 눌리지 않았을 때의 상태 (내부 풀업 사용으로 기본 HIGH)
bool currentButtonState = LOW; // 현재 버튼 상태
bool lastButton3State = HIGH; // 새로운 버튼 상태 (항상 눌려 있는 상태)
bool currentButton3State = HIGH; // 새로운 버튼 상태
bool toggleState = true;    // "E"와 "S"를 번갈아 출력하기 위한 상태
bool scanState = false;

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP); // 내부 풀업 저항을 사용하여 버튼 핀 설정
  pinMode(BUTTON_3_PIN, INPUT_PULLUP); // 새로운 버튼 핀 설정 (항상 눌려 있는 상태)
  pinMode(BUZZER_PIN, OUTPUT);       // 부저 핀 설정
  pinMode(LED_PIN, OUTPUT);          // LED 핀 설정
  pinMode(LED_D_PIN, OUTPUT);        // "D" LED 핀 설정
  Serial.begin(9600);                // 시리얼 통신 시작
}

void loop() {
  currentButtonState = digitalRead(BUTTON_PIN); // 버튼 상태 읽기
  currentButton3State = digitalRead(BUTTON_3_PIN); // 새로운 버튼 상태 읽기

  // 기존 버튼 눌림 처리
  if (currentButtonState == LOW && lastButtonState == HIGH) { // 버튼이 눌릴 때
    toggleState = !toggleState; // 상태를 반전
    if (toggleState) {
      Serial.println("E");  // "E" 출력
      scanState = false;
      digitalWrite(LED_PIN, LOW); 
      noTone(BUZZER_PIN);
      digitalWrite(LED_D_PIN, LOW);
    } else {
      Serial.println("S");  // "S" 출력
      scanState = true;
      digitalWrite(LED_PIN, HIGH); 
    }
  }

  // 새로운 버튼이 눌렸을 때 "O"를 시리얼로 출력
  if (currentButton3State == LOW && lastButton3State == HIGH) { // 새로운 버튼이 눌릴 때
    Serial.println("O"); // "O" 출력
  }

  // 시리얼로 받은 명령에 따른 부저 및 LED_D 동작
  if (Serial.available() > 0 ) {  // 시리얼 데이터가 들어온 경우
    char received = Serial.read(); // 시리얼로 들어온 데이터 읽기

    Serial.print("받은 데이터: ");
    Serial.println(received); // 받은 데이터를 그대로 출력

    if (received == 'S') {   // "S"가 들어오면 부저 끄기
      noTone(BUZZER_PIN);   // 부저 끄기
      digitalWrite(LED_D_PIN, LOW); // LED_D 끄기
    }

    if (received == 'D' && scanState) {         // "D"가 들어오면 부저 울리기 시작
      digitalWrite(LED_D_PIN, HIGH); // LED_D 켜기
      tone(BUZZER_PIN, HIGH_FREQUENCY);
    }
  }

  lastButtonState = currentButtonState; // 이전 버튼 상태 업데이트
  lastButton3State = currentButton3State; // 새로운 버튼 상태 업데이트
}

void toggleLED() {
  static bool ledState = false; // LED 상태를 저장하는 변수
  ledState = !ledState; // 상태 반전
  digitalWrite(LED_PIN, ledState ? HIGH : LOW); // LED 상태 변경
  Serial.print("LED 상태: ");
  Serial.println(ledState ? "ON" : "OFF");
}
