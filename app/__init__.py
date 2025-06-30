# app/__init__.py

# FlaskとFlask-SocketIOから、必要なクラスをインポートします。
from flask import Flask
from flask_socketio import SocketIO

# SocketIOのインスタンスを生成します。
# このインスタンスは、後でFlaskアプリケーションと連携して、リアルタイム通信機能を提供します。
# cors_allowed_origins="*"という設定は、CORS（Cross-Origin Resource Sharing）ポリシーを緩和するものです。
# これにより、どのドメイン（オリジン）からでもこのサーバーへのSocket.IO接続が許可されます。
# 開発中は便利ですが、本番環境では特定のドメインに限定することがセキュリティ上推奨されます。
socketio = SocketIO(cors_allowed_origins="*")

def create_app():
    """
    Flaskアプリケーションを生成し、設定を行うファクトリ関数。
    """
    # アプリケーションのインスタンスを生成します。
    # - `__name__`: このファイル自身のモジュール名を渡しており、Flaskがリソース（テンプレートなど）を探す際の基準点となります。
    # - `static_folder`: CSSやJavaScriptなどの静的ファイルを格納するフォルダ名を指定します。
    # - `template_folder`: HTMLテンプレートを格納するフォルダ名を指定します。
    app = Flask(__name__, static_folder="static", template_folder="templates")

    # アプリケーションの設定を環境変数から読み込みます。
    # 例えば、`FLASK_SECRET_KEY=your_secret_value` のような環境変数が設定されていれば、
    # それが `app.config["SECRET_KEY"]` として読み込まれます。
    app.config.from_prefixed_env()

    # アプリケーションの設定項目にデフォルト値を設定します。
    # "SECRET_KEY" がまだ設定されていない場合（環境変数などから読み込まれなかった場合）に、"change-me" という値が設定されます。
    # SECRET_KEYは、セッション情報の暗号化などに使われるため、本番環境では必ず推測されにくい値に変更する必要があります。
    app.config.setdefault("SECRET_KEY", "change-me")

    # 同じディレクトリにある`main.py`ファイルから、`bp`という名前で定義された
    # Blueprintオブジェクトを`main_bp`という名前でインポートします。
    from .main import bp as main_bp

    # インポートしたBlueprintをアプリケーションに登録します。
    # Blueprintを使うことで、ルーティングなどの機能をファイル単位でモジュール化でき、
    # 大規模なアプリケーションの管理がしやすくなります。
    app.register_blueprint(main_bp)

    # 最初に作成したSocketIOインスタンスを、Flaskアプリケーションと紐付けて初期化します。
    # `async_mode="eventlet"` は、非同期処理のライブラリとして `eventlet` を使用することを指定しています。
    # `eventlet` は多数の同時接続を効率的に処理できるため、Socket.IOを使ったリアルタイムアプリケーションでよく利用されます。
    socketio.init_app(app, async_mode="eventlet")

    # 全ての設定が完了したアプリケーションインスタンスを返します。
    return app