// ===================================================================
// ===== Arduino A (センサー/指令ハブ) プログラム【双方向対応版】 =====
// ===================================================================

// ----- ライブラリの読み込み -----
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <SoftwareSerial.h>

// ----- 無線(nRF24L01)のセットアップ -----
RF24 radio(7, 8);
// アドレスを2つ設定。pipe 0でBに送信、pipe 1でBから受信
const byte addresses[][6] = {"00001", "00002"};

// ----- SoftwareSerialのピン設定 -----
SoftwareSerial C_Serial(A2, A3);

// ----- 定数の設定 -----
const int SENSOR_PIN = A0;
const int NEAR_THRESHOLD = 500;
const unsigned long COOLDOWN_PERIOD = 5000;
const int COMMAND_DELAY = 200;
const int WAIT_DELAY = 3000;
// ----- グローバル変数の設定 -----
int sensorValue;
int triggerCount = 0;
bool wasNear = false;
unsigned long lastTriggerTime = 0;

// ----- 初期設定 -----
void setup() {
  Serial.begin(9600); // デバッグ用
  C_Serial.begin(9600);

  Serial.println("Arduino A (指令ハブ) 起動中...");

  radio.begin();
  // pipe 0 を使ってArduino Bに送信するように設定
  radio.openWritingPipe(addresses[0]);
  // pipe 1 を使ってArduino Bから受信するように設定
  radio.openReadingPipe(1, addresses[1]);
  radio.setPALevel(RF24_PA_MIN);
  
  // 受信待機モードで起動
  radio.startListening();
  Serial.println("初期化完了。監視中。");
}

// ----- コマンド送信用の関数 -----
void sendCommand(const char* command) {
  // 送信前に一時的に送信モードへ
  radio.stopListening();

  Serial.print("生成コマンド: ");
  Serial.println(command);

  char cmd_buffer[32];
  strcpy(cmd_buffer, command);

  // Arduino Cへ有線で送信
  C_Serial.println(cmd_buffer);
  
  // Arduino Bへ無線で送信
  radio.write(&cmd_buffer, sizeof(cmd_buffer));

  // 送信後にすぐに受信モードへ戻る
  radio.startListening();
}

// ----- メインループ -----
// この関数はArduinoの電源が入っている間、繰り返し実行され続けます。
void loop() {
  // ===== Arduino Bからの無線コマンド受信処理 =====
  // 無線モジュールで受信データがあるか確認します。
  if (radio.available()) {
    // 受信データを格納するための文字配列を用意します。
    char cmd[32] = "";
    // 無線データを読み込み、cmd配列に格納します。
    radio.read(&cmd, sizeof(cmd));
    // デバッグのため、受信したコマンドをシリアルモニタに出力します。
    Serial.print("無線コマンド受信: ");
    Serial.println(cmd);

    // 受信したコマンドが "EXEC_REV_SEQ" という文字列と一致するか比較します。
    if (strcmp(cmd, "EXEC_REV_SEQ") == 0) {
      // 一致した場合、回収シーケンスを開始するためのコマンドを送信します。
      sendCommand("REV_SEQ_START");
      // 荷物の検知回数をリセットします。
      triggerCount = 0;
    }
  }

  // ===== 既存の距離センサー処理 =====
  // 距離センサーからアナログ値を読み取ります。
  sensorValue = analogRead(SENSOR_PIN);
  // センサーの値がしきい値より大きいか判断し、物体が近くにあるかどうかをtrue/falseで記録します。
  bool isNear = (sensorValue > NEAR_THRESHOLD);

  // 物体が「遠い」から「近い」に変わった瞬間で、かつ前回の検知から一定時間以上が経過している場合のみ、中の処理を実行します。
  // これにより、物が置かれた瞬間を一度だけ検知し、連続的な誤作動を防ぎます。
  if ((isNear && !wasNear) && (millis() - lastTriggerTime > COOLDOWN_PERIOD)) {
    // 今回の検知時刻を記録し、クールダウンタイマーをリセットします。
    lastTriggerTime = millis();
    // 検知回数を1増やします。
    triggerCount++;

    // 送信するコマンド文字列を格納するための配列を用意します。
    char message[32] = "";

    // 検知回数に応じて、実行する処理を分岐します。
    switch (triggerCount) {
      case 1:
        // 1回目の検知の場合、モーター1と2を順番に作動させるコマンドを送信します。
        delay(WAIT_DELAY);
        strcpy(message, "M1_ON");
        sendCommand(message);
        delay(COMMAND_DELAY); // 次のコマンド送信までに少し待ちます。
        strcpy(message, "M2_ON");
        sendCommand(message);
        break;
      case 2:
        // 2回目の検知の場合、モーター3と4を順番に作動させるコマンドを送信します。
        delay(WAIT_DELAY);
        strcpy(message, "M3_ON");
        sendCommand(message);
        delay(COMMAND_DELAY);
        strcpy(message, "M4_ON");
        sendCommand(message);
        break;
      default:
        // 3回目以降の検知の場合（満杯とみなす）、警告コマンドを送信します。
        strcpy(message, "SENSOR_DETECTED_ALERT");
        sendCommand(message);
        break;
    }
  }

  // 現在のセンサーの状態を「前回の状態」として保存し、次のループでの変化を検知できるようにします。
  wasNear = isNear;
  // ループが速すぎないように、また処理を安定させるために100ミリ秒待機します。
  delay(100);
}
