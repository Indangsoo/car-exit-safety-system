// 핀 설정
const int BUZZER_PIN_FRONT = 4;  // 부저 핀을 4번으로 설정
const int BUZZER_PIN_BACK = 5;  // 부저 핀을 5번으로 설정
const int BUZZER_PIN_OUT = 6;  // 부저 핀을 6번으로 설정

const int BUTTON_PIN_TRY = 2;
const int BUTTON_PIN_OPEN = 3;

const int TPIN_UP = 12;
const int TPIN_DOWN = 13;

const int EPIN_UP = 8;
const int EPIN_DOWN = 9;

// 실행 코드 //
void setup() {
  pinMode(BUZZER_PIN_FRONT, OUTPUT);
  pinMode(BUZZER_PIN_BACK, OUTPUT);
  pinMode(BUZZER_PIN_OUT, OUTPUT);

  pinMode(BUTTON_PIN_TRY, INPUT); 
  pinMode(BUTTON_PIN_OPEN, INPUT); 

  pinMode(TPIN_UP, OUTPUT);
  pinMode(TPIN_DOWN, OUTPUT);

  pinMode(EPIN_UP, INPUT);
  pinMode(EPIN_DOWN, INPUT);

  Serial.begin(9600);

  delay(2000);
}

//D1: 의도되지 않은 문 열림
//D2: 위험이 감지되었지만 문 열림
//S1: 정상적으로 내림

// 손잡이 버튼을 누르면 주변 안전 확인 이후 isSafe 변경
// 손잡이 버튼이 안 눌리면 초기화
int isSafe = 0;

void loop() {
  //안전 확인이 안되었는데 문이 열림
  if (isSafe == 0 && digitalRead(BUTTON_PIN_OPEN) == LOW) {
    Serial.println("D1");
    activateBuzzers();

    while (digitalRead(BUTTON_PIN_OPEN) == LOW) { }

    deactivateBuzzers();
  }

  if (isSafe == 1 && digitalRead(BUTTON_PIN_OPEN) == LOW) {
    Serial.println("S1");
    isSafe = 0;
  }

  if (digitalRead(BUTTON_PIN_TRY) == LOW){
    isSafe = 0;
  }

  // 1. 2번 버튼이 클릭될 때까지 대기
  if (digitalRead(BUTTON_PIN_TRY) == HIGH) {  // 버튼이 눌렸을 때
    // 2. TPIN_UP과 EPIN_UP 초음파 센서로 거리 측정
    long distanceUp = measureDistance(TPIN_UP, EPIN_UP);
    
    if (distanceUp <= 110) {  // 거리가 1.1m 이하 (cm로 계산)
      // 세 개의 부저를 모두 울림
      activateBuzzers();
      
      // 1.1m 이하인 경우 계속 울림
      while (true) {
        distanceUp = measureDistance(TPIN_UP, EPIN_UP);
        if (distanceUp > 110) {  // 1.1m 초과시 탈출
          break;
        }

        if (digitalRead(BUTTON_PIN_TRY) == LOW) {
          Serial.println("D2");
        }
      }
      
      // 부저 정지
      deactivateBuzzers();
    }

    // 3. TPIN_DOWN과 EPIN_DOWN 초음파 센서로 거리 측정
    long distanceDown = measureDistance(TPIN_DOWN, EPIN_DOWN);
    
    // 30cm 기준으로 부저 소리 조절
    if (distanceDown > 30) {
      highToLowTone();  // 멀면 고음에서 저음으로
    } else {
      lowToHighTone();  // 가까우면 저음에서 고음으로
    }

    isSafe = 1;
  }
}

// 거리 측정 함수 (초음파 센서)
long measureDistance(int triggerPin, int echoPin) {
  digitalWrite(triggerPin, LOW);
  delayMicroseconds(2);
  digitalWrite(triggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(triggerPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH);
  long distance = duration * 0.034 / 2;  // cm로 변환
  return distance;
}

// 모든 부저를 울리는 함수
void activateBuzzers() {
  digitalWrite(BUZZER_PIN_FRONT, HIGH);
  digitalWrite(BUZZER_PIN_BACK, HIGH);
  digitalWrite(BUZZER_PIN_OUT, HIGH);
}

// 모든 부저를 정지하는 함수
void deactivateBuzzers() {
  digitalWrite(BUZZER_PIN_FRONT, LOW);
  digitalWrite(BUZZER_PIN_BACK, LOW);
  digitalWrite(BUZZER_PIN_OUT, LOW);
}

// 고음에서 저음으로 소리 내기
void highToLowTone() {
  for (int i = 2000; i > 500; i -= 50) {
    tone(BUZZER_PIN_FRONT, i, 100);
    delay(150);
  }
}

// 저음에서 고음으로 소리 내기
void lowToHighTone() {
  for (int i = 500; i < 2000; i += 50) {
    tone(BUZZER_PIN_FRONT, i, 100);
    delay(150);
  }
}
