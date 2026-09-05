# 実MainWindowのGUI回帰テスト

Qt Testで実際のメニュー、ボタン、棋譜欄を操作する。アプリケーション本体のオブジェクトファイルを再利用し、`main.cpp`だけをテスト用の起動処理へ置き換える。MainWindowやコーディネーターのスタブは使用しない。

対象環境はLinux、Qt6、Python3、Xvfb、Release/Ninjaビルド。通常のCTestとは別に実行する。既存の`build/`が別の構成の場合、この手順の対象外となる。

```bash
# 初回のビルド設定
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON

# アプリとGUIテストのビルド
python3 tests/gui/prepare.py

# 全シナリオ
xvfb-run -a -s '-screen 0 1600x1200x24' python3 tests/gui/run.py

# 修正対象のシナリオだけを実行
xvfb-run -a -s '-screen 0 1600x1200x24' python3 tests/gui/run.py \
  currentPositionCopy bodCurrentPosition commentsAndBookmark \
  consideration dockVisibilityAndLock usageLink branchNavigation
```

駒の種類の切り替え、反転表示、別の盤面への反映、設定復元は `pieceStyles` で検証します。
標準・太字・反転時のスクリーンショットも保存します。
木目・白・黒の追加バリエーションは `pieceVariants` で切り替え・反転・設定復元を検証します。

各シナリオは独立したプロセスで実行し、35秒でタイムアウトする。通常のアプリ設定は使用せず、テスト用一時ディレクトリに隔離する。結果は`build/gui-audit/`に保存される。

- `run-results.json`: シナリオ別結果。失敗・タイムアウト時は実行スクリプトも非0で終了する。
- `*.log` / `*.xml`: Qt TestのテキストログとJUnit XML。
- `*.usi.log`: 模擬USIエンジンへ送信したコマンド。
- `menus.json` / `all-clicked-actions.json`: 実行時メニューと操作済みアクション。
- `screenshots/`: 起動画面、各機能の画面、失敗時の画面。

`mock_usi.py`は決まった思考情報と指し手を返す検証用エンジン。実エンジンの評価精度・棋力は検証しない。CSAは設定画面までを確認する。WebリンクはURLハンドラで宛先を検査するため、ブラウザ起動・外部通信は行わない。

現在局面のSFEN/BODコピー、分岐からのコピー、コメント・しおりの保存と再読込、検討開始・中止の繰り返し、全ドックの固定、「使い方」のURL表示を回帰テストに含む。加えて棋譜形式の相互変換、対局、解析、詰み探索、画像保存などを検証する。
