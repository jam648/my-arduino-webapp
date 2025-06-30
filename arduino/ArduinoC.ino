// ===================================================================
// ===== Arduino C (モーター制御側) プログラム【STBY制御・コメント追加版】 =====
// ===================================================================

// ----- ライブラリの読み込み -----
#include <SoftwareSerial.h> // Arduino Aとの有線シリアル通信に必要

// ----- モータードライバーのピン設定 -----
const int MOTOR_SPEED = 50; // モーターの回転速度(PWMのデューティ比, 0-255)
const int ROTATION_DURATION = 2000; // 一度のコマンドでモーターが回転し続ける時間(ミリ秒)

// モータードライバ1 (M1, M2用) のピン割り当て
#define M1_PWM 3  // モーター1 速度制御(PWM)
#define M1_IN1 2  // モーター1 回転方向制御1
#define M1_IN2 4  // モーター1 回転方向制御2
#define M2_PWM 5  // モーター2 速度制御(PWM)
#define M2_IN1 7  // モーター2 回転方向制御1
#define M2_IN2 8  // モーター2 回転方向制御2
#define STBY_PIN1 A4 // STBYピン

// モータードライバ2 (M3, M4用) のピン割り当て
#define M3_PWM 6  // モーター3 速度制御(PWM)
#define M3_IN1 12 // モーター3 回転方向制御1
#define M3_IN2 13 // モーター3 回転方向制御2
#define M4_PWM 9  // モーター4 速度制御(PWM)
#define M4_IN1 A0 // モーター4 回転方向制御1
#define M4_IN2 A1 // モーター4 回転方向制御2
#define STBY_PIN2 A5 // STBYピン

// ----- SoftwareSerialのピン設定（ピン競合回避のため変更） -----
// Arduino Cの A2(RX) <--> Arduino Aの A3(TX)
// Arduino Cの A3(TX) <--> Arduino Aの A2(RX)
SoftwareSerial A_Serial(A2, A3);

// ----- 動作制御用の変数 -----
unsigned long motorStopTimes[4] = {0, 0, 0, 0}; // 各モーターの自動停止時刻を格納する配列。0の場合は停止中。

// 逆回転動作の状態を管理するための「状態(State)」を列挙型(enum)で定義
enum SeqState { IDLE, STEP_M3_REV, WAIT1, STEP_M4_REV, WAIT2, STEP_M1_REV, WAIT3, STEP_M2_REV };
SeqState revSequenceState = IDLE; // 現在のシーケンス状態を保持する変数。初期状態はIDLE(待機)
unsigned long seqLastStepTime = 0; // シーケンスの各ステップが開始した時刻を記録する変数

// 動作内の待機時間（ミリ秒）
const int SEQ_SHORT_DELAY = 200;
const int SEQ_LONG_DELAY = 500;

// ----- 初期設定 -----
void setup() {
  // シリアル通信を9600bpsで開始
  Serial.begin(9600);   // PCとのデバッグ用
  A_Serial.begin(9600); // Arduino Aとの通信用

  int pins[] = {M1_PWM, M1_IN1, M1_IN2, M2_PWM, M2_IN1, M2_IN2, M3_PWM, M3_IN1, M3_IN2, M4_PWM, M4_IN1, M4_IN2, STBY_PIN1,STBY_PIN2};
  // forループを使って、リスト内の全ピンをまとめて出力モードに設定
  for (int pin : pins) {
    pinMode(pin, OUTPUT);
  }
  
  // 起動時にモータードライバを有効(アクティブ)にする
  digitalWrite(STBY_PIN1, HIGH);
  digitalWrite(STBY_PIN2, HIGH);

  // 全てのモーターを停止した状態で起動する
  stopAllMotors();
  Serial.println("モーター制御C、起動完了。コマンド待機中...");
}

// ----- モーター制御の基本関数 -----
// 指定された番号のモーターを停止する
void stopMotor(int motorNumber) {
  switch (motorNumber) {
    case 1: analogWrite(M1_PWM, 0); break; // PWM出力を0にしてモーターを停止
    case 2: analogWrite(M2_PWM, 0); break;
    case 3: analogWrite(M3_PWM, 0); break;
    case 4: analogWrite(M4_PWM, 0); break;
  }
}

// 全てのモーターを停止し、関連するタイマーや状態もリセットする
void stopAllMotors() {
  for (int i = 1; i <= 4; i++) {
    stopMotor(i); // 各モーターを停止
    motorStopTimes[i-1] = 0; // 自動停止タイマーをリセット
  }
  revSequenceState = IDLE; // 実行中の動作があれば中断し、待機状態に戻す
  
  // 全停止時にはモータードライバを待機モード(低消費電力)にする
  digitalWrite(STBY_PIN1, LOW);
  //digitalWrite(STBY_PIN2, LOW);
  Serial.println("全モーター停止。ドライバ待機モード。");
}

// 指定されたモーターを制御する（回転方向と自動停止タイマーの設定）
void controlMotor(int motorNumber, bool forward) {
  // モーターを動かす前に、必ずドライバを有効(アクティブ)にする
  digitalWrite(STBY_PIN1, HIGH);
  digitalWrite(STBY_PIN2, HIGH);

  if (forward) { // 正回転の場合
    switch (motorNumber) {
      // IN1をHIGH, IN2をLOWに設定して正回転させ、PWMピンに速度を出力
      case 1: digitalWrite(M1_IN1, HIGH); digitalWrite(M1_IN2, LOW); analogWrite(M1_PWM, MOTOR_SPEED); break;
      case 2: digitalWrite(M2_IN1, HIGH); digitalWrite(M2_IN2, LOW); analogWrite(M2_PWM, MOTOR_SPEED); break;
      case 3: digitalWrite(M3_IN1, HIGH); digitalWrite(M3_IN2, LOW); analogWrite(M3_PWM, MOTOR_SPEED); break;
      case 4: digitalWrite(M4_IN1, HIGH); digitalWrite(M4_IN2, LOW); analogWrite(M4_PWM, MOTOR_SPEED); break;
    }
  } else { // 逆回転の場合
    switch (motorNumber) {
      // IN1をLOW, IN2をHIGHに設定して逆回転させる
      case 1: digitalWrite(M1_IN1, LOW); digitalWrite(M1_IN2, HIGH); analogWrite(M1_PWM, MOTOR_SPEED); break;
      case 2: digitalWrite(M2_IN1, LOW); digitalWrite(M2_IN2, HIGH); analogWrite(M2_PWM, MOTOR_SPEED); break;
      case 3: digitalWrite(M3_IN1, LOW); digitalWrite(M3_IN2, HIGH); analogWrite(M3_PWM, MOTOR_SPEED); break;
      case 4: digitalWrite(M4_IN1, LOW); digitalWrite(M4_IN2, HIGH); analogWrite(M4_PWM, MOTOR_SPEED); break;
    }
  }
  // モーターの自動停止時刻を設定（現在の時刻 + 回転継続時間）
  motorStopTimes[motorNumber - 1] = millis() + ROTATION_DURATION;
}

// 逆回転動作を進行させる関数
void updateReverseSequence() {
  // 動作実行中でなければ(IDLE状態なら)何もせずに関数を抜ける
  if (revSequenceState == IDLE) return; 

  unsigned long currentTime = millis(); // 現在時刻を取得

  // 現在の動作の状態(revSequenceState)に応じて処理を分岐
  switch (revSequenceState) {
    case STEP_M3_REV: // ステップ：M3を逆回転
      controlMotor(3, false);    // M3を逆回転させる
      revSequenceState = WAIT1;  // 次の状態(WAIT1)に遷移
      seqLastStepTime = currentTime; // ステップ完了時刻を記録
      break;
    case WAIT1: // ステップ：短い待機
      if (currentTime - seqLastStepTime >= SEQ_SHORT_DELAY) { // 短い待機時間が経過したら
        revSequenceState = STEP_M4_REV; // 次の状態(STEP_M4_REV)に遷移
      }
      break;
    case STEP_M4_REV: // ステップ：M4を逆回転
      controlMotor(4, false);
      revSequenceState = WAIT2;
      seqLastStepTime = currentTime;
      break;
    case WAIT2: // ステップ：長い待機
      if (currentTime - seqLastStepTime >= SEQ_LONG_DELAY) { // 長い待機時間が経過したら
        revSequenceState = STEP_M1_REV; // 次の状態に遷移
      }
      break;
    case STEP_M1_REV: // ステップ：M1を逆回転
      controlMotor(1, false);
      revSequenceState = WAIT3;
      seqLastStepTime = currentTime;
      break;
    case WAIT3: // ステップ：短い待機
      if (currentTime - seqLastStepTime >= SEQ_SHORT_DELAY) {
        revSequenceState = STEP_M2_REV;
      }
      break;
    case STEP_M2_REV: // ステップ：M2を逆回転
      controlMotor(2, false);
      revSequenceState = IDLE; // 全てのステップが完了したので、待機状態に戻る
      break;
  }
}

// ----- メインループ -----
void loop() {
  // 1. コマンド受信処理
  // Arduino Aからシリアル通信でデータが送られてきていれば
  if (A_Serial.available() > 0) {
    String command = A_Serial.readStringUntil('\n'); // 改行文字('\n')までを一つのコマンドとして読み込む
    command.trim(); // コマンドの前後の空白や改行コードを削除
    Serial.print("受信コマンド: ");
    Serial.println(command); // 受信したコマンドをPCのシリアルモニタに表示

    // 受信したコマンド文字列に応じて、対応する関数を呼び出す
    if (command == "M1_ON") controlMotor(1, true);
    else if (command == "M1_REV") controlMotor(1, false);
    else if (command == "M2_ON") controlMotor(2, true);
    else if (command == "M2_REV") controlMotor(2, false);
    else if (command == "M3_ON") controlMotor(3, true);
    else if (command == "M3_REV") controlMotor(3, false);
    else if (command == "M4_ON") controlMotor(4, true);
    else if (command == "M4_REV") controlMotor(4, false);
    else if (command == "ALL_OFF") stopAllMotors();
    else if (command == "REV_SEQ_START") { // 逆回転動作の開始コマンドを受け取ったら
      revSequenceState = STEP_M3_REV;      // 最初の状態を設定
    }
  }

  // 2. モーター自動停止処理
  unsigned long currentTime = millis(); // 現在時刻を取得
  for (int i = 0; i < 4; i++) {
    // motorStopTimes[i]が0より大きい(タイマー作動中) かつ 現在時刻が停止予定時刻を過ぎていたら
    if (motorStopTimes[i] > 0 && currentTime >= motorStopTimes[i]) {
      stopMotor(i + 1);    // 対応するモーターを停止
      motorStopTimes[i] = 0; // タイマーをリセット
    }
  }
  
  // 3. 逆回転動作の更新処理
  // この関数を毎ループ呼び出すことで、動作が自動的に進行する
  updateReverseSequence();
}