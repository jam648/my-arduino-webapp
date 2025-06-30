# local_bridge.py
# ---
# このスクリプトはローカルPC（Arduinoが接続されているマシン）で実行する。
# 役割:
# 1. 物理的なArduinoとのUSBシリアル通信を行う。
# 2. クラウド上のAzure Web AppとSocket.IOでリアルタイム通信を行う。
# 3. Arduinoから受信したデータ (例: センサー検知) をAzureへ転送する。
# 4. Azureから受信したコマンド (例: 回収指示) をArduinoへ転送する。
# ---

import os
import sys
import threading
import time
import serial
import socketio
from dotenv import load_dotenv

# .envファイルから環境変数を読み込む
load_dotenv()

# --- 環境変数 ---
# 接続先のAzure Web AppのURL
AZURE_APP_URL = os.getenv("AZURE_APP_URL")
# Arduino Bが接続されているシリアルポート名 (環境に合わせて変更)
SERIAL_PORT = os.getenv("SERIAL_PORT", "/dev/cu.usbmodem101")
# シリアル通信のボーレート
BAUDRATE = int(os.getenv("BAUDRATE", "9600"))

# 必須の環境変数が設定されているかチェック
if not AZURE_APP_URL:
    print("[ERROR] 環境変数 `AZURE_APP_URL` を設定してください。")
    sys.exit(1)

# --- Socket.IOクライアントのセットアップ ---
sio = socketio.Client()
serial_conn = None  # シリアル接続オブジェクトをグローバルで保持

# --- Socket.IOイベントハンドラ ---
@sio.event
def connect():
    """サーバーへの接続が成功したときに呼び出される。"""
    print("[OK] Azureサーバーに接続しました。")

@sio.event
def disconnect():
    """サーバーから切断されたときに呼び出される。"""
    print("[WARN] Azureサーバーから切断されました。")

@sio.on("command_to_local")
def on_command_to_local(data):
    """Azureサーバーからコマンドを受信し、Arduinoに書き込む。"""
    command = data.get("command")
    if command and serial_conn and serial_conn.is_open:
        try:
            print(f"[CMD -> Arduino] コマンド '{command}' を送信中...")
            # コマンドの末尾に改行を追加してエンコードし、シリアルポートに書き込む
            serial_conn.write((command + "\n").encode())
            print("[OK] 送信完了")
        except serial.SerialException as e:
            print(f"[ERROR] シリアル書き込みに失敗しました: {e}")

# --- シリアル通信スレッド ---
def listen_serial():
    """
    (バックグラウンドスレッドで実行)
    Arduinoからのシリアルデータを永続的に待ち受け、受信したらAzureサーバーに送信する。
    """
    global serial_conn
    while True:
        try:
            # シリアルポートへの接続を試行
            print(f"[SERIAL] Arduino ({SERIAL_PORT}) に接続試行中...")
            serial_conn = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=1)
            print("[OK] Arduinoに接続しました。メッセージを待機中...")

            # 接続が続く限り、1行ずつデータを読み込む
            while serial_conn.is_open:
                # readline()で改行まで読み込み、デコードして前後の空白を削除
                line = serial_conn.readline().decode().strip()
                if line:
                    print(f"[SERIAL RECV] 受信データ: '{line}'")
                    # 受信したデータを 'serial_data_from_local' イベントとしてAzureサーバーへ送信
                    sio.emit("serial_data_from_local", {"data": line})

        except serial.SerialException:
            # ポートが見つからない場合や切断された場合
            print(f"[WARN] シリアルポート {SERIAL_PORT} が見つかりません。5秒後に再試行します。")
            if serial_conn:
                serial_conn.close()
            serial_conn = None
            time.sleep(5)
        except Exception as e:
            # その他の予期せぬエラー
            print(f"[ERROR] 予期せぬエラーが発生しました: {e}")
            time.sleep(5)

# --- メイン処理 ---
if __name__ == "__main__":
    # シリアル通信の待受を別スレッドで開始（メインのSocket.IO通信をブロックしないため）
    serial_thread = threading.Thread(target=listen_serial, daemon=True)
    serial_thread.start()

    # Socket.IOサーバーへの接続を無限ループで試行
    while True:
        try:
            print(f"[AZURE] Azureサーバー ({AZURE_APP_URL}) に接続試行中...")
            sio.connect(AZURE_APP_URL)
            sio.wait()  # 接続が切れるまで待機
        except socketio.exceptions.ConnectionError as e:
            # 接続に失敗した場合、10秒待ってから再試行
            print(f"[ERROR] 接続エラー: {e}。10秒後に再接続します。")
            time.sleep(10)
        except Exception as e:
            print(f"[ERROR] 予期せぬエラーでSocket.IOが停止しました: {e}。10秒後に再接続します。")
            time.sleep(10)