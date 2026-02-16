# GUI ランタイムデバッグ手法

静的コード解析では特定できないGUIバグを、ランタイムログの監視とユーザーのGUI操作を組み合わせて特定する手法。

## 前提条件

- `qCDebug()` によるカテゴリ別ログがコードに埋め込まれていること
- Debug ビルドであること（Release ビルドでは `QT_NO_DEBUG_OUTPUT` により qCDebug が無効化される）
- カスタムメッセージハンドラが `debug.log` にログを書き出す設定（`src/app/main.cpp` 参照）

## ロギングカテゴリ一覧

| カテゴリ名 | 定義ファイル | 対象 |
|---|---|---|
| `shogi.app` | `mainwindow.cpp` | MainWindow全般 |
| `shogi.game` | `matchcoordinator.cpp` | 対局制御・ゲーム開始 |
| `shogi.ui` | `kifudisplaycoordinator.cpp` | UI層・プレゼンタ |
| `shogi.ui.policy` | `uistatepolicymanager.cpp` | UI状態管理 |
| `shogi.navigation` | `kifunavigationcontroller.cpp` | 棋譜ナビゲーション |
| `shogi.kifu` | `kifuloadcoordinator.cpp` | 棋譜読み込み |
| `shogi.board` | `boardinteractioncontroller.cpp` | 盤面操作 |
| `shogi.core` | `shogiboard.cpp` | コアロジック |
| `shogi.engine` | `usi.cpp` | USIエンジン通信 |
| `shogi.analysis` | `analysisflowcontroller.cpp` | 解析機能 |
| `shogi.network` | `csaclient.cpp` | CSA通信 |
| `shogi.view` | `shogiview.cpp` | 盤面描画 |
| `shogi.clock` | `shogiclock.cpp` | 対局時計 |

## 手順

### 1. Debug ビルドの準備

```bash
# Debug ビルドに切り替え
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug -G Ninja
ninja -C build
```

### 2. ログ有効化してGUIを起動

```bash
# 全カテゴリ有効化（debug.log に出力される）
rm -f debug.log
QT_LOGGING_RULES="shogi.*.debug=true" nohup ./build/ShogiBoardQ > /dev/null 2>&1 &

# 特定カテゴリのみ有効化する場合
QT_LOGGING_RULES="shogi.game.debug=true;shogi.app.debug=true" nohup ./build/ShogiBoardQ > /dev/null 2>&1 &
```

### 3. ユーザーにGUI操作手順を伝え、ステップごとにログを確認

各操作ステップの完了をユーザーに報告してもらい、その都度ログを確認する。

```bash
# ログのリアルタイム確認
tail -30 debug.log

# 特定キーワードでフィルタ
grep -i -E "キーワード1|キーワード2" debug.log | tail -30
```

### 4. テスト完了後、Release ビルドに戻す

```bash
kill $(pgrep ShogiBoardQ) 2>/dev/null
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -G Ninja
ninja -C build
```

## ログ確認の観点

### NULLポインタ
```bash
grep -i "0x0\|null\|ABORT" debug.log
```

### 値の追跡（例: m_startSfenStr の変遷）
```bash
grep -i "startSfenStr" debug.log
```

### 特定フロー全体の確認（例: 対局開始フロー）
```bash
grep -i -E "initializeGame|initPosNo|prepareData|performCleanup|ensureBranch|seed" debug.log
```

## 実績: BoardSyncPresenter NULLポインタバグ（2026-02-16）

### 症状
局面編集後に対局→終了→「開始局面」クリックで編集済み局面が表示されない。

### 静的解析の限界
データフロー（sfenRecord、m_startSfenStr）を全経路追跡したが、ロジック上は正しく動作するはずだった。自動テスト（18件）も全パス。

### ランタイムデバッグで判明した原因
```
syncBoardAndHighlightsAtRow ABORT: sfenRecord*= 0x0 sfenRecord.empty?= true
```
`BoardSyncPresenter` が初期化時に受け取った `sfenRecord` ポインタが NULL（`MatchCoordinator` 未生成時に作成されたため）。対局開始で新しい `MatchCoordinator` が作られてもポインタは未更新のまま、盤面描画が常にスキップされていた。

### 修正
- `BoardSyncPresenter::setSfenRecord()` を追加
- `ensureBoardSyncPresenter()` で毎回 sfenRecord ポインタを同期
