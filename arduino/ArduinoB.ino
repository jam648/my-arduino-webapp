// ===================================================================
// ===== Arduino B (PC接続・コマンド送信ハブ) プログラム =====
// ===================================================================

// ----- ライブラリの読み込み -----
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// ----- 無線(nRF24L01)のセットアップ -----
RF24 radio(7, 8); // CE, CSNピンを7番、8番に設定

// アドレスを2つ設定。Arduino Aとは送受信のアドレスが逆になるように設定します。
// pipe 0でAから受信、pipe 1でAへ送信します。
const byte addresses[][6] = {"00001", "00002"};

// ----- 初期設定 -----
void setup() {
  // PCとのシリアル通信を開始 (ボーレート: 9600)
  Serial.begin(9600);
  Serial.println("Arduino B (送受信ハブ) 起動中...");

  // 無線モジュールの初期化
  radio.begin();
  // 【変更】pipe 1 を使ってArduino Aに送信するように設定 (アドレス: "00002")
  radio.openWritingPipe(addresses[1]);
  // 【変更】pipe 0 を使ってArduino Aから受信するように設定 (アドレス: "00001")
  radio.openReadingPipe(1, addresses[0]);
  // 無線モジュールの送信出力を最小に設定
  radio.setPALevel(RF24_PA_MIN);
  
  // まずは受信モードで待機を開始
  radio.startListening();
  Serial.println("初期化完了。待機中。");
}

// ----- メインループ -----
// この関数は電源が入っている間、繰り返し実行されます。
void loop() {
  // ===== PCからのコマンドをArduino Aへ無線送信する処理 =====
  // PCからシリアル通信でデータが送られてきたかを確認
  if (Serial.available() > 0) {
    // 送信されてきた文字列を改行コードまで読み込む
    String commandFromPC = Serial.readStringUntil('\n');
    // 文字列の前後の空白を削除
    commandFromPC.trim();

    // PCから受信したコマンドが "TRIGGER_REV_SEQ" という文字列だったら
    if (commandFromPC == "TRIGGER_REV_SEQ") {
      // 無線送信のために、一時的に受信モードを停止
      radio.stopListening();
      
      // Arduino Aへ送るためのコマンド文字列 ("EXEC_REV_SEQ") を準備
      const char* text = "EXEC_REV_SEQ";
      // デバッグ用にPCのシリアルモニタへメッセージを出力
      Serial.print("コマンド '");
      Serial.print(text);
      Serial.println("' をArduino Aへ送信します。");

      // 無線でコマンド文字列を送信
      bool ok = radio.write(text, strlen(text));
      // 送信が成功したか失敗したかをPCへ報告
      if (ok) {
        Serial.println("-> 送信成功");
      } else {
        Serial.println("-> 送信失敗");
      }

      // 送信が終わったら、すぐに受信モードに戻り、Arduino Aからの通知を待つ
      radio.startListening();
    }
  }

  // ===== Arduino Aからの状態通知をPCへ転送する処理 =====
  // Arduino Aから無線でデータが届いているかを確認
  if (radio.available()) {
    // 受信データを格納するための文字配列を準備
    char receivedMessage[32] = "";
    // 無線データを読み込む
    radio.read(&receivedMessage, sizeof(receivedMessage));
    // 受信したデータを、そのままPC(local_bridge.py)へシリアル通信で出力（転送）する
    Serial.println(receivedMessage);
  }
}