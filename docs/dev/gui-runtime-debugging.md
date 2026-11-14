# GUI ランタイムデバッグ手法

静的コード解析では特定できないGUIバグを、ランタイムログの監視とユーザーのGUI操作を組み合わせて特定する手法。

## 前提条件

- `qCDebug()` によるカテゴリ別ログがコードに埋め込まれていること
- Debug ビルドであること（Release ビルドでは `QT_NO_DEBUG_OUTPUT` により qCDebug が無効化される）
- カスタムメッセージハンドラが `debug.log` にログを書き出す設定（`src/app/main.cpp` 参照）

対局中の挙動を GUI 操作なしで再現・集計したい場合は、仮想ディスプレイ上で自動操作する
[live-game-verification.md](live-game-verification.md) を参照。

## ロギングカテゴリ一覧

| カテゴリ名 | 定義ファイル | 対象 |
|---|---|---|
| `shogi.app` | `mainwindow.cpp` | MainWindow全般 |
| `shogi.game` | `matchcoordinator.cpp` | 対局制御・ゲーム開始 |
| `shogi.ui` | `kifudisplaycoordinator.cpp` | UI層・プレゼンタ |
| `shogi.ui.policy` | `uistatepolicymanager.cpp` | UI状態管理 |
| `shogi.navigation` | `kifunavigationcontroller.cpp` | 棋譜ナビゲーション |
| `shogi.kifu` | `kifuloader.cpp` | 棋譜読み込み |
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

## 描き直しの確認

操作のたびに、どの部品がどの範囲を描き直しているかは、実アプリで測る（コードを読むだけでは、Qt の中で広がる描き直しに気づけない）。

### 測り方

1. `src/app/main.cpp` に一時的なアプリ全体のイベントフィルタを入れる（**コミットしない**）。`QEvent::Paint` を受けるたびに、部品のクラス・`objectName`・ウィンドウ・描き直した範囲（`QPaintEvent::region()` の矩形と面積）・親をたどった道筋をファイルに書く。原因を追うときは `LayoutRequest`・`Resize`・`Move`・`WindowActivate`・`StyleChange`・`PaletteChange` も書く
2. `build-p2p-docs`（`-DENABLE_P2P_DOCS_AUTOMATION=ON`）でビルドして別名に写し、`main.cpp` を戻して作り直す（`git status` で戻したことを確かめる）
3. `scripts/docs/launch.sh`（Xvfb、設定の隔離）で起動し、自動化 API（`scripts/docs/rpc.py`）で操作して、操作の前後の時刻でログを区切って部品ごとに集計する。1倍で測り、計測を2つ同時に走らせない
4. タブの裏のドックには `Paint` が来ない。`dock.configure` でドックを並べて、全部が見える配置で測る
5. 見た目は、変更前の実行ファイルと同じ操作をして画面を画素で比べる（部分描画では、半透明の合成の丸めで ±1 の差が出ることがある）

「無駄」とするのは、見た目が変わらないのに描く・変わった所より広く描く・不透明でない部品の下の親まで同じ範囲を描く・1回の操作で同じ内容を2回以上描く・何もしていないのに定期的に描く、のどれか。

### Qt の中で描き直しが広がる所

| 仕組み | 起きること | 対処 |
|---|---|---|
| `QTabWidget` は `LayoutRequest` を受けるたびに `setUpLayout()` で自分の全体を `update()` する | タブの中の `QLabel::setText` のたびに、タブの部品の全体 | ページを `LayoutBoundaryWidget` で包む |
| `QAbstractScrollArea::layoutChildren()` は縦横のスクロールバーの入れ物を順に `raise()` する | 両方が出ていると、範囲が変わるたびに両方と下の親 | 片方を自分で置く（`LogPanel` の横のスクロールバー） |
| モデルのリセットは、その場と、遅らせた配置のやり直しの時の2回 `update()` し、見出しも作り直す | 表示領域の全体を2回と、見出しの全体 | 行の追加・削除で知らせる。リセットは `IncrementalTableView` が1回にまとめる |
| `QWidget::event` は `WindowActivate`・`WindowDeactivate` で、前面と後ろでパレットの違う部品を `update()` する（`QGraphicsWidget` は常に） | ダイアログを開く・閉じるたびに、そのウィンドウと後ろのウィンドウのほとんど | `ApplicationRendering::useActiveColorsWhenInactive`、評価値グラフは知らせを受けない |
| `QAbstractItemView` は、`WindowActivate`・`WindowDeactivate`（`viewportEvent`）とフォーカスの出入り（`focusInEvent`・`focusOutEvent`）で、パレットによらず表示領域の全体を `update()` する。見出し（`QHeaderView`）も同じ | ダイアログを開く・閉じるたびに、見えている表と見出しの全体 | `IncrementalTableView` は現在の行と選んだ行だけ、`TableHeaderView` は描き直さない（前面と後ろでパレットが同じとき） |
| `QDockWidget` は `WindowActivate`・`WindowDeactivate` でタイトルの範囲を `update()` する | ダイアログを開く・閉じるたびに、見えているドックのタイトルと下のメインウィンドウ | `DockRegistry` のドックは、収まっている間は描き直さない |
| `QWidget::show()` は「後で全体を描き直す」知らせ（`UpdateLater`）を予約する | 画面に出た知らせが先に届くと、開くたびに全体を2回 | モーダルでないウィンドウは `DialogUtils::showWindow` |
| 不透明でない部品を描くと、下にある祖先も同じ範囲を描く（スタイルシートで背景を付けた表の表示領域も不透明でなくなる） | 小さな部品の描き直しが、ドックやメインウィンドウまで | `OpaqueBackground::keep`・`DockRegistry` の `opaqueContent` |
| Fusion は範囲のない `QProgressBar` を毎秒60回描き直す。動きの知らせを断ると、動きを作り直して描き直しを続ける（Qt 6.12 の `QCommonStylePrivate::removeAnimation`） | 待つ間ずっと、帯と下のダイアログ | 知らせを受けて描かず、動きを一時停止する（通信対局の待機ダイアログ） |
| Qt Charts は、点を入れ直すと同じ点でも線を描き直す | 表示中の手を移すたびに 0 の線の全体 | 変わった線だけを入れ直す |

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

> 2026-10-15 注記: 今の `BoardSyncPresenter` は sfenRecord を持たず、分岐ツリーのカーソル（`KifuNavigationState`）から局面を読む。`ensureBoardSyncPresenter()` を含むレジストリ層は `MainWindowComposer` に置き換えた（[composition-root.md](composition-root.md)）。上の記述は当時の記録である。
