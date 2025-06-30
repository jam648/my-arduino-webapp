// static/js/app.js
// ---
// フロントエンドのメインJavaScriptファイル。
// 1. Socket.IOサーバーへの接続とイベントハンドリング。
// 2. ユーザー操作（ボタンクリック）の検知とサーバーへのコマンド送信。
// 3. サーバーからの状態更新を受け取り、リアルタイムで画面表示を変更する。
// ---

// サーバーのSocket.IOエンドポイントに接続
const socket = io();

// --- DOM要素のキャッシュ ---
// 頻繁にアクセスするHTML要素をあらかじめ取得しておく
const lastMsgEl = document.getElementById("last-message");
const lastUpdEl = document.getElementById("last-updated");
const deliveriesEl = document.getElementById("deliveries");
const btnReverse = document.getElementById("btn-reverse");

// --- イベントリスナー ---
// 「ポストを元に戻す」ボタンがクリックされたときの処理
btnReverse.addEventListener("click", () => {
  // ユーザーに確認を求める
  if (confirm("荷物を取り出し、ポストを初期状態に戻しますか？")) {
    // 'execute_command'イベントをサーバーに送信
    socket.emit("execute_command", { command: "TRIGGER_REV_SEQ" });
  }
});

// --- Socket.IO イベントハンドラ ---
// サーバーから'update_status'イベントを受信したときの処理
socket.on("update_status", (state) => {
  console.log("状態更新を受信:", state);
  // 現在の状態メッセージと更新日時を画面に反映
  lastMsgEl.textContent = state.last_message;
  lastUpdEl.textContent = state.last_updated
    ? `更新日時: ${state.last_updated}`
    : "";

  // 状態が更新されたら、配達履歴も再取得して表示を更新する
  fetchDeliveries();
});

/**
 * 配達履歴をサーバーから取得し、画面に描画する関数
 */
function renderDeliveries(deliveryList) {
  // 履歴リストのコンテナを一旦空にする
  deliveriesEl.innerHTML = "";
  // 取得した履歴データ一件ごとにカードを生成
  deliveryList.forEach((d) => {
    const card = document.createElement("div");
    card.className =
      "border border-gray-300 rounded-2xl p-4 shadow-sm hover:shadow-md transition-shadow";
    card.innerHTML = `
        <h3 class="text-lg font-semibold text-blue-700 mb-1">
            スロット ${d.slot}
        </h3>
        <p class="text-gray-700">${new Date(
          d.delivered_at
        ).toLocaleString()}</p>
    `;
    // コンテナに生成したカードを追加
    deliveriesEl.appendChild(card);
  });
}

/**
 * /api/deliveries エンドポイントにリクエストを送信し、
 * 取得したデータを renderDeliveries 関数に渡す
 */
function fetchDeliveries() {
  fetch("/api/deliveries")
    .then((response) => response.json())
    .then(renderDeliveries)
    .catch(error => console.error('配達履歴の取得に失敗しました:', error));
}


// --- 初期化処理 ---
// ページが読み込まれたときに、一度配達履歴を取得して表示する
fetchDeliveries();