# wsgi.py
# wsgi.pyはcreate_appがうまく実行されないため作成したものであり、場合によっては不要（スタートアップを書き換えてください）
from app import create_app

# Gunicornはこの 'app' という名前の変数を見つけて実行します。
app = create_app()