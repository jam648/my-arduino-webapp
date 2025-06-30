# app/main.py
# ---
# Flask + Socket.IO を使用したWebサーバーアプリケーションのメインファイル。
# 以下の役割を担う:
# 1. Web UI (index.html) の提供
# 2. 配送履歴APIの提供 (/api/deliveries)
# 3. ブラウザ、ローカルブリッジとのリアルタイム通信 (Socket.IO)
# 4. システム状態の管理と更新
# 5. Discordへのイベント通知
# ---

import os
from datetime import datetime
import requests
from flask import Blueprint, jsonify, render_template
from . import socketio
from .models import record_delivery, latest

# --- 環境変数からの設定読み込み ---
# Discordに通知するためのWebhook URL
WEBHOOK_URL = os.getenv("WEBHOOK_URL")

# FlaskのBlueprintを定義
bp = Blueprint("main", __name__)

# --- アプリケーションのランタイム状態 ---
# サーバーが起動している間、メモリ上でシステムの現在の状態を保持する辞書。
state = {
    "m1": False,
    "m2": False,
    "m3": False,
    "m4": False,
    "last_message": "システム待機中...",
    "last_updated": None,
}

# --------------------------------------------------------------------------
# ヘルパー関数
# --------------------------------------------------------------------------

def _discord(message: str):
    """Discordにメッセージを送信するヘルパー関数。"""
    if not WEBHOOK_URL:
        print("警告: WEBHOOK_URLが設定されていません。")
        return
    try:
        # requestsライブラリを使い、指定されたWebhook URLにPOSTリクエストを送信
        requests.post(WEBHOOK_URL, json={"content": message, "username": "センサー通知Bot"})
    except requests.RequestException as exc:
        print(f"Discordへの通知中にエラーが発生しました: {exc}")


def _update(command: str):
    """
    ローカルブリッジから受信したコマンドに基づいてシステム状態を更新し、
    関連する処理（UI更新、Discord通知、DB記録）をトリガーする。
    """
    global state
    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    msg = None
    slot = None

    # 受信したコマンドに応じて、状態メッセージとスロット番号を決定
    if command == "M2_ON":
        msg = "1回目の配達が行われました。"
        state["m1"] = state["m2"] = True
        slot = 1
    elif command == "M4_ON":
        msg = "2回目の配達が行われました。"
        state["m3"] = state["m4"] = True
        slot = 2
    elif command == "REV_SEQ_START":
        msg = "荷物が回収されました。"
        # 全てのモーターステータスをリセット
        state = {k: False for k in ("m1", "m2", "m3", "m4")}
    elif command == "SENSOR_DETECTED_ALERT":
        msg = "ポストがいっぱいです。"

    # 有効なメッセージが生成された場合
    if msg:
        state["last_message"] = msg
        state["last_updated"] = ts
        # 接続されている全てのクライアント（ブラウザ）に状態更新を通知
        socketio.emit("update_status", state)
        # Discordに通知
        _discord(msg)
        # 配達イベントの場合、データベースに記録
        if slot:
            record_delivery(slot)

# --------------------------------------------------------------------------
# Flask ルート定義
# --------------------------------------------------------------------------

@bp.route("/")
def index():
    """Web UIのメインページ (index.html) をレンダリングして返す。"""
    return render_template("index.html")

@bp.route("/api/deliveries")
def deliveries():
    """最新の配達履歴をJSON形式で返すAPIエンドポイント。"""
    return jsonify(latest())

# --------------------------------------------------------------------------
# Socket.IO イベントハンドラ
# --------------------------------------------------------------------------

@socketio.on("connect")
def _on_connect():
    """
    クライアント（ブラウザやローカルブリッジ）が接続した際のイベントハンドラ。
    接続してきたクライアントに、現在のシステム状態を送信する。
    """
    socketio.emit("update_status", state)
    print("クライアントが接続しました。")

@socketio.on("execute_command")
def _on_exec(data):
    """
    ブラウザからのコマンド実行要求を受け取るイベントハンドラ。
    このコマンドをローカルブリッジに中継する。
    """
    command = data.get("command")
    if command == "TRIGGER_REV_SEQ":
        # 'command_to_local' というイベント名で、ローカルブリッジにデータを送信
        socketio.emit("command_to_local", {"command": command})
        print(f"コマンド '{command}' をローカルブリッジへ中継します。")
        # コマンド送信を試みたことを、操作元のブラウザにフィードバック
        socketio.emit(
            "command_response",
            {
                "status": "success",
                "message": "回収コマンドをローカルPCへ送信しました。",
            },
        )

@socketio.on("serial_data_from_local")
def on_serial_data(data):
    """
    ローカルブリッジからシリアルデータを受信した際のイベントハンドラ。
    Arduinoからの物理的なイベントをクラウド側で処理する起点となる。
    """
    command = data.get("data")
    if command:
        print(f"ローカルブリッジからコマンド '{command}' を受信しました。")
        # 受信したコマンドを使い、システム状態を更新
        _update(command)