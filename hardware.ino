#define BUTTON_PIN 2   // 푸시 버튼이 연결된 핀 번호
#define BUTTON_3_PIN 3 // 새로운 푸시 버튼이 연결된 핀 번호
#define BUZZER_PIN 4   // 부저가 연결된 핀 번호
#define LED_PIN 12     // LED가 연결된 핀 번호
#define LED_D_PIN 13   // "D" 명령어로 제어할 LED 핀 번호
#define T_1_PIN 9
#define T_2_PIN 6
#define E_1_PIN 8
#define E_2_PIN 7


#define HIGH_FREQUENCY 1000  // 높은 주파수 (단위: Hz)
#define LOW_FREQUENCY 500    // 낮은 주파수 (단위: Hz)
#define DELAY_TIME 300       // 각 주파수 사이의 딜레이 시간 (단위: 밀리초)

bool lastButtonState = LOW;  // 버튼이 눌리지 않았을 때의 상태 (내부 풀업 사용으로 기본 HIGH)
bool currentButtonState = LOW; // 현재 버튼 상태
bool lastButton3State = HIGH; // 새로운 버튼 상태 (항상 눌려 있는 상태)
bool currentButton3State = HIGH; // 새로운 버튼 상태
bool toggleState = true;    // "E"와 "S"를 번갈아 출력하기 위한 상태
bool scanState = false;
bool dangerState = false;

float measureDistance(int trigPin, int echoPin) {
  // 트리거 핀에 펄스 신호 전송
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // 에코 핀에서 신호 수신
  long duration = pulseIn(echoPin, HIGH);

  // 거리 계산 (소리 속도: 343m/s)
  float distance = (duration * 0.0343) / 2.0;

  return distance;
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP); // 내부 풀업 저항을 사용하여 버튼 핀 설정
  pinMode(BUTTON_3_PIN, INPUT_PULLUP); // 새로운 버튼 핀 설정 (항상 눌려 있는 상태)
  pinMode(BUZZER_PIN, OUTPUT);       // 부저 핀 설정
  pinMode(LED_PIN, OUTPUT);          // LED 핀 설정
  pinMode(LED_D_PIN, OUTPUT);        // "D" LED 핀 설정

  pinMode(T_1_PIN, OUTPUT);
  pinMode(T_2_PIN, OUTPUT);
  pinMode(E_1_PIN, INPUT);
  pinMode(E_2_PIN, INPUT);

  Serial.begin(9600);                // 시리얼 통신 시작
}

void highToLowTone(int pin) {
  for (int freq = 2000; freq >= 200; freq -= 100) { // 고정된 범위에서 100Hz 단위로 감소
    tone(pin, freq); // 부저에 주파수 전달
    delay(100);      // 각 음을 100ms 동안 재생
  }
  noTone(pin); // 마지막에 부저 정지
}

void lowToHighTone(int pin) {
  for (int freq = 200; freq <= 2000; freq += 100) { // 고정된 범위에서 100Hz 단위로 증가
    tone(pin, freq); // 부저에 주파수 전달
    delay(100);      // 각 음을 100ms 동안 재생
  }
  noTone(pin); // 마지막에 부저 정지
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
      while (measureDistance(T_1_PIN, E_1_PIN) < 20){
        tone(BUZZER_PIN, HIGH_FREQUENCY);
      }
      noTone(BUZZER_PIN);
      float distance2 = measureDistance(T_2_PIN, E_2_PIN);

      Serial.print(distance2); 
      if (distance2 < 30){
        lowToHighTone(BUZZER_PIN);
      }else if(distance2 > 200){
        highToLowTone(BUZZER_PIN);
      }

      Serial.println("S");  // "S" 출력
      scanState = true;
      digitalWrite(LED_PIN, HIGH); 
      distance2 = 0;
    }
  }

  if (currentButton3State == LOW && lastButton3State == HIGH) { 
    if(!dangerState) {
      Serial.println("O");
    } else {
      Serial.println("D");
    }
  }

  // 시리얼로 받은 명령에 따른 부저 및 LED_D 동작
  if (Serial.available() > 0 ) {  // 시리얼 데이터가 들어온 경우
    char received = Serial.read(); // 시리얼로 들어온 데이터 읽기

    Serial.print("받은 데이터: ");
    Serial.println(received); // 받은 데이터를 그대로 출력

    if (received == 'S') {   // "S"가 들어오면 부저 끄기
      noTone(BUZZER_PIN);   // 부저 끄기
      digitalWrite(LED_D_PIN, LOW); // LED_D 끄기
      dangerState = false;
    }

    if (received == 'D' && scanState) {         // "D"가 들어오면 부저 울리기 시작
      digitalWrite(LED_D_PIN, HIGH); // LED_D 켜기
      tone(BUZZER_PIN, HIGH_FREQUENCY);
      dangerState = true;
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
