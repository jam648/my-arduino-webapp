# app/models.py
# ---
# データ永続化層。
# Azure Table Storageとの接続と、データの読み書きを行う関数を定義する。
# ---

import os
from datetime import datetime, timezone
from azure.core.exceptions import ResourceExistsError
from azure.data.tables import TableClient

# --- Azure Table Storage 設定 ---
# 環境変数からテーブル名と接続文字列を取得
_TABLE_NAME = os.getenv("AZURE_TABLE_NAME", "Deliveries")
_connection_string = os.environ["AZURE_STORAGE_CONNECTION_STRING"]

# TableClientオブジェクトを初期化
table_client = TableClient.from_connection_string(
    conn_str=_connection_string, table_name=_TABLE_NAME
)

# --- テーブルの初期化 ---
# アプリケーション起動時にテーブルが存在しない場合は作成する
try:
    table_client.create_table()
    print(f"テーブル '{_TABLE_NAME}' を作成しました。")
except ResourceExistsError:
    # テーブルが既に存在する場合は何もしない
    print(f"テーブル '{_TABLE_NAME}' は既に存在します。")
    pass


def record_delivery(slot: int) -> None:
    """
    配達イベントをAzure Table Storageに記録する。
    RowKeyにはUTCのISO 8601形式タイムスタンプを使用し、一意性と時系列順を担保する。
    """
    now_utc_iso = datetime.now(timezone.utc).isoformat()
    entity = {
        # PartitionKeyを固定することで、同じパーティション内にクエリを限定できる
        "PartitionKey": "deliveries",
        "RowKey": now_utc_iso,
        "slot": slot,
        "delivered_at": now_utc_iso,
    }
    table_client.create_entity(entity=entity)
    print(f"配達履歴を記録しました: スロット {slot}")

def latest(limit: int = 20):
    """
    最新の配達履歴を指定された件数だけ取得する。
    取得後にPython側でdelivered_atキーを元に降順ソートする。
    """
    # PartitionKeyが 'deliveries' のエンティティをクエリ
    query_filter = "PartitionKey eq 'deliveries'"
    entities = table_client.query_entities(query_filter, results_per_page=limit)
    
    # 取得したエンティティリストを 'delivered_at' の降順でソートし、上限数でスライス
    return sorted(list(entities), key=lambda e: e["delivered_at"], reverse=True)[:limit]