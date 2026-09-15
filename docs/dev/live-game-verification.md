# 対局中挙動の実機検証手順（仮想ディスプレイ自動操作）

人間対エンジンの対局を Xvfb 上で自動操作し、`debug.log` を区間集計して「対局中に何が何回起きたか」を確認する手順。
手番管理・盤面再設定・分岐ツリー更新・棋譜欄同期など、対局ロジックと UI 同期が絡む挙動は単体テストで再現しにくく、
2026-09-15 の分岐ツリー系修正ではこの方法が最も確実だった。

ユーザーの実デスクトップ・実設定・スピーカーには一切触れない構成にしてある。
ログカテゴリの一覧やユーザー操作との併用は [gui-runtime-debugging.md](gui-runtime-debugging.md) を参照。

---

## 目次

1. [前提: Debug ビルドと debug.log](#1-前提-debug-ビルドと-debuglog)
2. [環境の準備](#2-環境の準備)
3. [X 操作ヘルパー scripts/x11ctl.py](#3-x-操作ヘルパー-scriptsx11ctlpy)
4. [座標表と座標の再導出](#4-座標表と座標の再導出)
5. [基本フロー](#5-基本フロー)
6. [ログの区間集計と判定文字列](#6-ログの区間集計と判定文字列)
7. [スクリーンショットで確認する観点](#7-スクリーンショットで確認する観点)
8. [後始末と注意事項](#8-後始末と注意事項)
9. [実績](#9-実績)

---

## 1. 前提: Debug ビルドと debug.log

- Release ビルドでは `QT_NO_DEBUG_OUTPUT` により `qCDebug()` がコンパイル時に除去され、メッセージハンドラも空になる。
  ログを見るには **Debug ビルドが必須**。
- Debug ビルドは起動時のカレントディレクトリに `debug.log` を**追記**する（`src/app/main.cpp`、`.gitignore` 済み）。
  `QT_LOGGING_RULES` は不要（既定で全カテゴリの debug が有効）。
- 通常の `build/`（Release）とは別に `build-debug/` を作る。テストは不要なので `BUILD_TESTING=OFF` で約3分。

```bash
cmake -B build-debug -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=OFF
ninja -C build-debug ShogiBoardQ
```

`build-debug/` は gitignore の対象（`build*`）なので残しておいてよい。ソース変更後は `ninja -C build-debug ShogiBoardQ` で差分ビルドする。

## 2. 環境の準備

必要なツール: `Xvfb`、ImageMagick の `import`（スクリーンショット）、`python3`、libX11/libXtst（通常インストール済み）。
`xdotool` は使わない（無い環境が前提）。座標の再導出に Pillow と numpy を使う（任意）。

### 隔離した設定ディレクトリ

設定は `QStandardPaths::AppConfigLocation` から読むので、`XDG_CONFIG_HOME` を作業用ディレクトリに向ければ実設定を汚さない。
ユーザーの ini をコピーしてウィンドウサイズだけ固定する。

```bash
S=/path/to/scratch/live            # 作業用ディレクトリ（任意の場所）
mkdir -p "$S/cfg/ShogiBoardQ"
cp ~/.config/ShogiBoardQ/ShogiBoardQ.ini "$S/cfg/ShogiBoardQ/"
cp ~/.config/kdeglobals "$S/cfg/" 2>/dev/null       # フォント・配色を実環境に揃える（任意）
sed -i 's/^mainWindowSize=.*/mainWindowSize=@Size(1920 1080)/' "$S/cfg/ShogiBoardQ/ShogiBoardQ.ini"
```

対局条件は ini の `[GameSettings]` がそのまま対局開始ダイアログに反映される。確認しておく項目:

| キー | 意味 | 検証で使った値 |
|---|---|---|
| `isHuman1` / `isEngine2` | 先手=人間、後手=エンジン | `true` / `true` |
| `engineNumber2` | `[Engines]` の **0 始まり**の index | `1`（Gikou 2） |
| `byoyomiSec2` | エンジンの秒読み | `3`（約3秒以内に応手） |
| `basicTimeMinutes1` / `byoyomiSec1` | 人間側の持ち時間 | `5` / `10` |
| `startingPositionNumber` | 開始局面 | `0`（現在の局面） |

エンジンは `[Engines]` に登録済みのものを使う。Gikou 2（`/home/nakada/shogi/Gikou/release`）は応手が速く検証向き。
アプリはエンジンの作業ディレクトリを自動で設定するので、評価ファイル等の配置は登録済みなら問題ない。

### 仮想ディスプレイとアプリの起動

```bash
Xvfb :99 -screen 0 1920x1080x24 &
sleep 1

cd /path/to/ShogiBoardQ                # debug.log はこのディレクトリに書かれる
DISPLAY=:99 QT_QPA_PLATFORM=xcb QT_QPA_PLATFORMTHEME=kde XDG_CURRENT_DESKTOP=KDE \
XDG_CONFIG_HOME="$S/cfg" XDG_CACHE_HOME="$S/cache" \
nohup ./build-debug/ShogiBoardQ > "$S/app.log" 2>&1 &

W=$(DISPLAY=:99 python3 scripts/x11ctl.py wait "将棋盤Q" 30)
DISPLAY=:99 python3 scripts/x11ctl.py move "$W" 0 0 1920 1080
DISPLAY=:99 python3 scripts/x11ctl.py focus "$W"
```

ウィンドウマネージャが無いので、キーボードショートカット（Alt+G など）は効かない。`focus` の後にメニュータイトルを直接クリックする。

## 3. X 操作ヘルパー scripts/x11ctl.py

ctypes で libX11/libXtst を呼ぶだけの小さなツール。`DISPLAY` で対象を指定する。

| コマンド | 内容 |
|---|---|
| `list` | マップ済みトップレベルを `id name x y w h or=` で一覧。ポップアップメニューやダイアログも出る |
| `wait <substr> [sec]` | 名前に `substr` を含むトップレベルが現れるまで待ち、id を出力 |
| `geom <id>` | ルート座標での `x y w h` |
| `move <id> x y w h` | `XMoveResizeWindow` |
| `focus <id>` | `XRaiseWindow` + `XSetInputFocus` |
| `click x y [button]` | XTest によるクリック（ルート座標） |
| `key <keysym>...` | XTest によるキー入力（例: `Return Down Escape`） |

Qt は日本語のウィンドウタイトルを `_NET_WM_NAME`（UTF8_STRING）に置くため `XFetchName` では空になる。ヘルパーはその場合 `_NET_WM_NAME` を読む。
ダイアログの出現は `list` の出力で確認できる（例: 対局開始ダイアログ `'対局'`、終局メッセージ `'対局終了'` 270x104）。

## 4. 座標表と座標の再導出

ウィンドウ 1920x1080、`[SizeRelated] squareSize=55`（既定）、既定ドックレイアウト（棋譜ドック右、思考ドック下）での値。
設定やフォントが違えばずれるので、最初のスクリーンショットで必ず確認する。

### 盤面

格子線は `x = 141 + 55 * i`、`y = 90 + 60 * j`（i, j = 0..9）。マスの中心は次の式で求める（筋は 9→1 が左→右、段は 1→9 が上→下）。

```bash
sq() { echo $((141 + 55*(9-$1) + 28)) $((90 + 60*($2-1) + 30)); }   # sq 筋 段 → x y
mv1() { set -- $(sq $1 $2) $(sq $3 $4); X click $1 $2; sleep 0.4; X click $3 $4; }   # mv1 from筋 from段 to筋 to段
```

人間の手は「移動元をクリック → 移動先をクリック」の2クリック。成りが発生する手は確認ダイアログが出るので避ける。

### メニュー・ダイアログ・ペイン

| 対象 | 座標 (x, y) | 備考 |
|---|---|---|
| メニュー 対局(G) | (220, 12) | クリックでポップアップ |
| メニュー項目 対局（開始ダイアログ） | (235, 37) | ポップアップの先頭 |
| メニュー項目 投了 | (235, 90) | 対局中のみ有効 |
| 対局開始ダイアログ OK | (1245, 764) | ダイアログは 450,210 1000x580 |
| 終局メッセージ OK | (1025, 527) | 「先手の投了。後手の勝ちです。」など |
| 棋譜欄の行 | (840, 132 + 22 × 手数) | 0 手目 = 開始局面行 |
| ナビボタン ▲（1手戻る）/ ▼（1手進む） | (1771, 325) / (1771, 352) | 列は上から ▲\| ▲▲ ▲ ▼ ▼▼ ▼\| |
| 下部タブ 分岐ツリー | (521, 1041) | 思考 (101, 1041)、評価値グラフ (615, 1041) |

### 座標の再導出

盤面の格子はスクリーンショットから機械的に検出できる。閾値 100 で暗い画素を列・行ごとに数え、最大値の 6 割以上を線とみなす。

```python
from PIL import Image
import numpy as np
im = np.asarray(Image.open("shot.png").convert("L")).astype(int)
reg = im[85:645, 130:650]                       # 盤のおおよその範囲
dark = reg < 100
def lines(v, off):
    m = v.max(); idx = [i + off for i, c in enumerate(v) if c > m * 0.6]
    out = []
    for i in idx:
        if out and i - out[-1][-1] <= 2: out[-1].append(i)
        else: out.append([i])
    return [sum(g) / len(g) for g in out]
print("vx", lines(dark.sum(axis=0), 130))       # 10 本
print("hy", lines(dark.sum(axis=1), 85))        # 10 本
```

メニューやボタンの座標は `import -window root shot.png` を撮って画像から読み取る。

## 5. 基本フロー

以下は「開始局面から人間側3手 → 投了 → 終了」の例。エンジン応手は秒読み3秒なので、人間の手の後は 6 秒待つ。

```bash
X() { DISPLAY=:99 python3 scripts/x11ctl.py "$@"; }
L=debug.log

startgame() { X click 220 12; sleep 1; X click 235 37; sleep 2; X click 1245 764; sleep 7; }
resign()    { X click 220 12; sleep 1; X click 235 90; sleep 2; X click 1025 527; sleep 2; }

startgame
GS=$(wc -l < "$L")                 # 対局開始時点のログ行数（区間集計用）

mv1 7 7 7 6; sleep 6               # ▲７六歩
mv1 2 7 2 6; sleep 6               # ▲２六歩
mv1 2 6 2 5; sleep 6               # ▲２五歩
DISPLAY=:99 import -window root "$S/after3.png"

tail -n +$((GS+1)) "$L" > "$S/moves.log"
resign
```

途中局面からの再対局は、棋譜欄の行（`X click 840 $((132 + 22*N))`）で N 手目へ移動してから `startgame` する
（開始ダイアログの「開始局面」は既定で「現在の局面」）。同じ手を指し直すとノードが再利用され、別の手を指すと分岐ができる。

## 6. ログの区間集計と判定文字列

`debug.log` は追記されるので、操作前に行数を控え `tail -n +N` で区間を切り出して `grep -c` する。
対局中の挙動を判定するのに使える文字列:

| 文字列 | 出所 | 意味・期待値の例 |
|---|---|---|
| `appendKifuLine ENTER: text=` | GameRecordUpdateService | 記録された手。人間手＋応手＋投了の数と一致すること |
| `HvE engineTurnNow=` | HumanVsEngineStrategy | 人間手の後のエンジン手番判定。毎回 `true` |
| `addMove: added to tree` / `addMove: reused existing node` | LiveGameSession | ツリーへの追加／指し直しによる再利用 |
| `startLiveGameSession: started from node, ply=` | PreStartCleanupHandler | 途中局面からの開始手数 |
| `rebuildBranchTree: m_rows.size` | BranchTreeManager | 分岐ツリーの全再構築。対局中は新規ライン発生時のみ |
| `loadBoardWithHighlights ENTER` | BoardSyncPresenter | ナビゲーション経由の盤面再設定。対局中は 0 |
| `syncBoardAndHighlightsAtRow ENTER` | KifuNavigationCoordinator | 棋譜欄行変更経由の盤面同期。対局中は 0 |
| `shogiboard_sfen.cpp` かつ `setSfen:` | ShogiBoard | 盤面の SFEN 再設定そのもの。対局中は 0 |
| `onPositionChanged ENTER` | KifuDisplayCoordinator | 棋譜欄行変更による位置同期。対局中は 0 |
| `POST-NAVIGATION INCONSISTENCY` | KifuDisplayCoordinator | 表示一致性検証の警告。0 件。直後の `Reason:` 行に理由 |
| `[WARN]` | 全般 | 警告の総数 |

```bash
echo "moves=$(grep -c 'appendKifuLine ENTER' "$S/moves.log") \
turn=$(grep -o 'HvE engineTurnNow= [a-z]*' "$S/moves.log" | sort | uniq -c | tr '\n' ' ') \
rebuild=$(grep -c 'rebuildBranchTree: m_rows.size' "$S/moves.log") \
setSfen=$(grep -c 'shogiboard_sfen.cpp.*setSfen:' "$S/moves.log") \
inconsistency=$(grep -c 'POST-NAVIGATION INCONSISTENCY' "$S/moves.log") \
warn=$(grep -c '\[WARN\]' "$S/moves.log")"
grep -n "POST-NAVIGATION INCONSISTENCY" -A 12 "$S/moves.log" | grep "Reason:"
```

対局終了（投了）ではセッションの commit は行われず、`sessionCommitted` は出ない。終局後にナビボタンで戻る・進むを行うと
`goBack LEAVE resultPly=` / `goForward: step ... displayText=` で辿った経路を確認できる。

## 7. スクリーンショットで確認する観点

`DISPLAY=:99 import -window root file.png` で全画面を撮る。確認する点:

- 棋譜欄の行と盤面の駒配置が一致し、最新手の行が黄色でハイライトされている
- 「次の手番」ボックスが正しい側（人間手の直後はエンジン、応手後は人間）
- 思考欄の読み筋の手番記号（後手の手が `△` で表示されること。盤面モデルの手番が古いと `▲` になる）
- 分岐ツリータブ: 指した手が順に追加され、指し直しでは重複ノードが増えず、分岐した手数に「+」が付く
- 終局後の戻る・進むで、棋譜欄・ツリー・盤面が同じ手に同期する

## 8. 後始末と注意事項

```bash
# 自分が :99 で起動したインスタンスだけ終了する（ユーザーが実デスクトップで起動中でも巻き込まない）
for p in $(pgrep -x ShogiBoardQ); do
  tr '\0' '\n' < /proc/$p/environ | grep -q '^DISPLAY=:99$' && kill $p
done
sleep 2
pkill -f '^Xvfb :99'
```

- `pkill -f "build/ShogiBoardQ"` のようなパターンは呼び出し元のシェル自身に一致して落ちる。プロセス名（`pgrep -x`）か環境変数で絞る。
- エンジン（Gikou など）はアプリ終了で止まる。残っていれば `pgrep -f '^/path/to/engine'` で確認する。
- ユーザーの `~/.config/ShogiBoardQ/ShogiBoardQ.ini` は読み取りのみ。作業用コピーはアプリ終了時に更新されるが実設定には影響しない。
- `debug.log` は肥大するので、検証が終わったら削除してよい（gitignore 済み）。
- 座標は設定（`squareSize`、ドック配置、フォント）に依存する。ダイアログが出ない・手が入らないときは、まずスクリーンショットで位置を確認する。

## 9. 実績

2026-09-15、分岐ツリー・ライブ対局周りの修正（重複ノード、`findBySfen`、盤面再設定の停止、盤面手番の GC 同期、
分岐ツリーの差分描画、手番補正コードの削除）をこの手順で検証した。人間側 3〜4 手の対局を計 5 局行い、
`setSfen` 0 回、`engineTurnNow= true` が毎手、一致性警告 0 件、分岐ツリー全再構築は新規ライン発生時の 1 回のみ、
分岐発生時の「+」と候補欄の内容が正しいことをログとスクリーンショットで確認した。
