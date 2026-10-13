# 依頼：2026-10-13 のリファクタリングで残した作業

2026-10-13 のコード監査をもとに、7段階のリファクタリングを行いました（コミット `d8eaa5d4`〜`f0301b43`）。
このファイルには、そのとき意図して残した作業を課題として書いています。
過去の会話を参照できない状態でも実行できるよう、現状の場所と目標をそれぞれに書いています。

## 使い方

`/clear` したあとで、課題を1つ選んで依頼してください。

```
docs/dev/prompts/task20261013-refactoring-followups.md の課題1をやってください。
```

- 1回の会話では1つの課題だけを扱います。課題1〜3は、それぞれ1課題でも大きな作業です。
- どの課題でも、先に下の「共通事項」を読んでください。
- 終わった課題は、このファイルの「進み具合」の表に完了日とコミットを書いてください。

## 進み具合

| 課題 | 内容 | 状態 |
|---|---|---|
| 1 | KI2 の読み書きを ShogiBoard と合法手で行う | 完了（2026-10-13） |
| 2 | JKF の書き出しを表示用の文字列から作らない | 完了（2026-10-13） |
| 3 | 盤を文字で持つ CSA の読み込みと読み筋の表示を ShogiBoard にする | 完了（2026-10-13） |
| 4 | MatchPlayer と時計の int の手番を整理する | 完了（2026-10-14） |
| 5 | Deps に残った `T**` を整理する | 未着手 |
| 6 | 配線層の任意項目（転送シグナル、フック） | 未着手 |
| 7 | 古い記述と小さな問題の修正 | 未着手 |

---

## 共通事項

### 前回のリファクタリングで使えるようになったもの

| 型・関数 | 場所 | 内容 |
|---|---|---|
| `UsiMove` | `src/core/usimove.h` | USI と1対1に対応する指し手。`UsiMove::parse()`、`toString()` |
| `BoardCoordinates` | `src/core/boardcoordinates.h` | 画面の座標（駒台を含む）と `UsiMove` の変換 |
| `ShogiNotation` | `src/core/shoginotation.h` | 筋・段・駒の漢字・全角・CSA 表記の表（`fileZenkaku`、`rankKanji`、`movePieceName`、`csaPieceCode`、`pieceFromCsaCode` など） |
| `KifMoveText` | `src/core/kifmovetext.h` | 指し手の日本語表記（`KifMoveText::format`） |
| `ShogiBoard` | `src/core/shogiboard.h` | `applyUsiMove`、`trySetSfen`（失敗してもログを出さない） |
| `EngineMoveValidator` | `src/core/enginemovevalidator.h` | 合法手の判定（`isLegalMove`）。合法手の数は返すが、一覧は返さない |
| `fmv::LegalCore` | `src/core/fmvlegalcore.h` | ビットボードの合法手生成（`generateLegalMoves`。`tst_fmv_perft` で perft を確認済み） |
| `SfenPositionTracer` | `src/board/sfenpositiontracer.h` | ShogiBoard の上で指し手を順に適用して局面を追う |
| `RecordLine` | `src/kifu/recordline.h` | 表示中の手順（局面と指し手の列）。棋譜の正本は分岐ツリーとカーソル |
| `TimeControl` | `src/core/timecontrol.h` | 持ち時間・秒読み・加算・遅延 |
| `Setting<T>` | `src/services/setting.h` | 設定の1項目（キーと既定値）の宣言 |

### 守ること

- `CLAUDE.md` の規約に従ってください（`connect()` にラムダを使わない、`std::as_const`、clazy level0/1、
  翻訳の更新と未翻訳0、CMakeLists へのファイルの列挙、設定は `Setting<T>` で宣言する、など）。
- 説明と報告は日本語でお願いします。
- コミットとプッシュは、報告のあとで指示を待ってから行ってください。
- 作業の途中で区切りごとに `git diff HEAD` を作業用のフォルダ（scratchpad）に控えてください。
  新しいファイルは先に `git add -N` してから控えます。
- ソースの文字列を照合するテストは足さないでください。動作を確かめるテストにします。
  直す範囲に文字列を照合する既存のテストがあれば、動作のテストに置き換えます。
- 利用者に見える動作が変わる場合は、報告でそのことを明記してください。

### 検証のコマンド

```bash
# ビルド（Release・BUILD_TESTING=ON の build を使う）
ninja -C build

# 単体テスト（134件）
ctest --test-dir build -j8 -E tst_mcp_python

# MCP のテスト（150件成功、5件スキップ）。basetemp はソケットパスの長さ制限のため短くする
SHOGIBOARDQ_CLI=$PWD/build/shogiboardq-cli SHOGIBOARDQ_EXECUTABLE=$PWD/build/ShogiBoardQ \
SHOGIBOARDQ_TEST_CSA_SERVER=/home/nakada/shogi/shogi-server/shogi-server \
SHOGIBOARDQ_TEST_USI_ENGINE=/home/nakada/GitHub/Hayanagi/build/hayanagi \
xvfb-run -a -s '-screen 0 1600x1200x24' env -u WAYLAND_DISPLAY QT_QPA_PLATFORM=xcb \
  python3 -m pytest -q -p no:cacheprovider --basetemp=/tmp/sbqN mcp/tests

# GUI シナリオ（92件。結果は build/gui-audit/run-results.json）
python3 tests/gui/prepare.py
xvfb-run -a -s '-screen 0 1600x1200x24' env -u WAYLAND_DISPLAY python3 tests/gui/run.py

# GUI フロー（prepare.py のあとで実行。tst_tsume_collection_gui には SHOGIBOARDQ_*_HOME を渡さない）
for t in tst_start_game_flow tst_ponder_flow tst_tsume_play_gui tst_tsume_search_dialog \
         tst_tsume_collection_gui tst_tsumeshogi_generator_gui; do
  xvfb-run -a -s '-screen 0 1600x1200x24' env -u WAYLAND_DISPLAY QT_QPA_PLATFORM=xcb \
    build/gui-audit/test-build/$t | grep -E '^Totals|^FAIL'
done

# エンジン終了のハーネス（29件。結果は <出力先>/results.json）
xvfb-run -a -s '-screen 0 1600x1200x24' env -u WAYLAND_DISPLAY QT_QPA_PLATFORM=xcb \
  python3 tests/gui/test_engine_shutdown.py --engine /home/nakada/GitHub/Hayanagi/build/hayanagi --output <出力先>

# 実機エンジン（29件・50件）
SHOGIBOARDQ_TEST_ENGINE=/home/nakada/shogi/apery_rust/apery \
SHOGIBOARDQ_TEST_PONDER_ENGINE=/home/nakada/shogi/Gikou/release ./build/tests/tst_usimatchhandler
SHOGI_REAL_KOMORING=/home/nakada/shogi/KomoringHeights-kh-v1.1.0/source/KomoringHeights-by-gcc ./build/tests/tst_tsume_play

# 翻訳（4言語とも未翻訳0。ja_JP は原文と同じ訳を入れる）
cmake --build build --target update_translations
grep -c 'type="unfinished"' resources/translations/*.ts
```

GUI を使うテストは、必ず `xvfb-run` の上で実行してください。`run.py` を素で実行すると、利用者の画面
（DISPLAY=:1）にウィンドウが出て、クリップボードも書き換えられます。アプリやテストを直接起動するときは、
`env -u WAYLAND_DISPLAY QT_QPA_PLATFORM=xcb` も付けます。

どの課題でも、単体テスト・MCP・GUI シナリオは必ず通してください。課題ごとに追加で必要な検証は、各課題に書いています。

### 報告

1. 変更内容（ファイルと要点）
2. 利用者に見える動作の変化
3. 実行した検証と結果（件数）
4. 直さなかったこと・見つけた別の問題

---

## 課題1：KI2 の読み書きを ShogiBoard と合法手で行う

### 現状

- **読み込み**：`Ki2Lexer`（`src/kifu/formats/ki2lexer.cpp`）は、`QString boardState[9][9]` と
  `QMap<Piece, int>` の持ち駒で盤を独自に持っています。`canPieceMoveTo` で駒の動きだけを見て候補を集め、
  `filterByDirection` で右・左・上・引・寄・直の修飾語を使って絞ります。
  - 王手放置とピンを判定しません。KI2 では、合法な候補が1つなら修飾語を省きます。ところが、ピンされて動けない駒も
    候補に入るため、その手を解決できないことがあります。
  - 「成」「不成」「打」の判定も、文字列の中で独自に行っています。
- **書き出し**：`Ki2Exporter`（`src/kifu/formats/ki2exporter.cpp`）は、棋譜欄の表示文字列（`displayText()`。
  KIF の「▲７六歩(77)」の形式）を `Ki2ToSfenConverter::convertPrettyMoveToKi2`（`ki2tosfenconverter.cpp`）で
  読み直しています。修飾語は、同じ文字の盤の上で `generateModifier` が作ります。
- 本譜の書き出しも、変化の書き出し（`outputKi2VariationFromBranchLine`）も、同じ経路です。

### 目標

- 読み込み・書き出しとも、盤は `ShogiBoard`、指し手は `UsiMove` にします。
- 候補は合法手から選びます。合法手の一覧を返す関数がないので、`fmv::LegalCore::generateLegalMoves` か
  `EngineMoveValidator` の Context 経路の上に作ります。作った関数には単体テストを付けてください。
- 書き出しは、分岐ツリーのノードの指し手（USI）と局面から修飾語を作ります。表示文字列は読み直しません。
  `KifuBranchNode` が持つ指し手の情報を確かめてから決めてください。
- 修飾語の付け方は、日本将棋連盟の棋譜表記の規則（右・左・直、上・引・寄、竜と馬の特例）に合わせます。
- 文字の盤（`boardState`）と、それを使う `initBoardFromSfen`・`applyMoveToBoard`・`canPieceMoveTo`・
  `collectCandidates` などは、使われなくなったら消します。

### 注意

- KI2 の読み込みのテストは `tests/` に複数あります（`grep -l Ki2 tests/*.cpp` で探してください）。
  KIF→KI2→読み込みの往復が同じ手順になることを、修飾語が必要な局面（金2枚が同じマスへ行ける、竜2枚、
  ピンされた駒がある）で確かめるテストを足してください。
- 旧版で書き出した KI2 も読めることを確かめてください（`tests/fixtures/` の KI2）。

### 完了の条件

共通の検証に加えて、KI2 の読み書きのテストが通り、ピンされた駒と修飾語の省略のテストを足していること。

---

## 課題2：JKF の書き出しを表示用の文字列から作らない

### 現状

- `JkfExporter`（`src/kifu/formats/jkfexporter.cpp`）は、分岐ツリーのノードの `displayText()` を
  `KifDisplayItem::prettyMove` に入れています。`JkfFormatter::convertMoveToJkf`（`jkfformatter.cpp` 264行付近）は、
  その日本語の文字列を1文字ずつ読み直して JKF の `move`（from・to・piece・same・promote・capture）を作ります。
- `jkfformatter.cpp` は、`kanjiToCsaPiece`・`zenkakuToNumber`・`kanjiToNumber` という独自の表を持っています。
  これは `ShogiNotation` と重複しています。
- 取った駒（capture）や成りの有無を、文字列から推測しています。

### 目標

- 指し手は、ノードの USI と直前の局面（`ShogiBoard` か `SfenPositionTracer`）から作ります。piece・capture・
  promote・same は、局面と指し手から決めます。
- 独自の表は消し、駒の表記が要るところは `ShogiNotation` を使います。
- 終局の語（投了・中断など）の変換（`japaneseToJkfSpecial`）は、分岐ツリーが終局を持つ方法を確かめてから、
  文字列に頼らない形にできるか検討してください。

### 完了の条件

共通の検証に加えて、JKF の書き出しと読み込みの往復テスト（成り・不成・打・同・取る手・変化を含む）が通ること。

---

## 課題3：盤を文字で持つ CSA の読み込みと読み筋の表示を ShogiBoard にする

### 現状

- **CSA の読み込み**：`CsaLexer`（`src/kifu/formats/csalexer.h`・`csalexer.cpp`・`csalexer_position.cpp`）は、
  独自の `Board { Cell sq[10][10]; }` を持っています。そこへ P1〜P9 行・P+/P- 行・AL を適用して、
  `toSfenBoard` で SFEN を作ります。指し手の適用（`parseMoveLine`）も、この盤の上で行います。
- **読み筋の表示**：`ShogiEngineInfoParser`（`src/engine/shogiengineinfoparser.h/.cpp`）は、読み筋の USI を
  日本語にするために、`QList<QChar>` の盤（`clonedBoardData`）を持っています。この盤は
  `ThinkingInfoPresenter`・`UsiMatchHandler`・`Usi` の `setClonedBoardData` で受け渡され、
  `pieceCharacter`・`setData`・`movePieceToSquare` で動かしています。
- 文字の盤はほかにもあります。
  - `src/ui/sfendiffutils.h`：`QString grid[9][9]`。局面の差分からハイライトを決める
  - `src/analysis/tsumeshogipositiongenerator.h`：`QChar m_board[9][9]`
  - `src/analysis/tsumeshogigenerator.h`：`QString cells[9][9]`

  詰将棋の2つは生成器の内部表現なので、手を入れるかどうかは読んでから判断してください。

### 目標

- CSA の読み込みは、局面の組み立てを `ShogiBoard` で行い、指し手は `UsiMove` として適用します。
- 読み筋の表示は、`ShogiBoard`（またはコピーできる `SfenPositionTracer`）と `UsiMove`・`KifMoveText` で
  日本語にします。`QList<QChar>` の受け渡しはなくします。思考情報の表示は1秒に何度も更新されます。
  `docs/dev/board-input-performance-2026-10-11.md` の項目12・26（思考情報の表示と、読み筋の漢字への変換の高速化）を
  読み、変更の前後で読み筋の変換が遅くならないことを測って確かめてください。

### 完了の条件

共通の検証に加えて、次がすべて通ること。

- CSA の読み込みのテスト（平手、駒落ち、P+/P-、AL を含む局面）
- 読み筋の表示のテスト
- 実機エンジンの `tst_usimatchhandler`

---

## 課題4：MatchPlayer と時計の int の手番を整理する

### 現状

- 手番は、前回 `Turn`（`Turn::Black`／`Turn::White`）にまとめました。ただし、次の2つは残しています。
  - `MatchPlayer`（`src/game/matchtypes.h`、P1=1、P2=2）。`MatchCoordinator::Player`、`EngineLifecycleManager::Player`、
    `MatchTimekeeper`・`MatchTurnHandler`・`GameEndHandler`・`GameStartOrchestrator` の `Player` は、この型の別名です。
  - `ShogiClock`（`src/core/shogiclock.h`）の API は、`int player`（1 か 2）で受けます。
- 残した理由：`MatchPlayer` は「先手・後手」と「エンジン1・エンジン2の枠」の両方の意味で使われています。
  たとえば人間対エンジンでエンジンが後手のときも、エンジンは `m_usi1` に入ります。
  このため、機械的に `Turn` に置き換えると意味が変わります。

### 目標

- 使っている箇所を読み、「手番（先手・後手）」の意味のものは `Turn` にします。
- 「エンジンの枠」の意味のものは、別の名前の型（例：`EngineSlot`）にするか、今のまま残すかを決めます。
- 時計は `Turn` で受けるようにします。

### 完了の条件

共通の検証に加えて、次が通ること。

- GUI フロー（特に `tst_start_game_flow`・`tst_ponder_flow`）
- エンジン終了のハーネス
- 実機エンジンの `tst_usimatchhandler`

---

## 課題5：Deps に残った `T**` を整理する

### 現状

前回、Deps の `T**` の多くは「呼ばれた時点の部品を返すゲッター（`std::function`）」に替えました。
次の箇所は、相手の変数に書き込む出力用に使っているものもあるため、残しています。

| ファイル | メンバ |
|---|---|
| `src/ui/wiring/branchnavigationwiring.h` | `branchTree`・`navState`・`kifuNavController`・`displayCoordinator`・`liveGameSession` |
| `src/ui/wiring/dialoglaunchwiring.h` | `csaGameDialog`・`csaGameCoordinator`・`gameInfoController`・`analysisModel`・`menuWindowDock` |
| `src/app/mainwindowcoreinitcoordinator.h` | `gameController`・`shogiView` |
| `src/kifu/kifuapplyservice.h` | `shogiView`・`branchTreeManager`・`branchTree`・`navState` |
| `src/network/csamoveprogresshandler.h` | `engineController`・`sfenHistory` |

### 目標

- 1つずつ、読むだけのものと書き込むものを分けます。
- 読むだけのものはゲッターにします。
- 書き込むものは、作る側（レジストリ）で生成して渡す形にするか、作った部品を返す関数にします。
  `docs/dev/deps-hooks-refs-pattern.md`（実物は `docs/dev/done/` にある）と、`docs/dev/ownership-guidelines.md`
  の作法に合わせてください。
- 課題が大きくなりすぎるときは、ファイル単位で区切って報告してください。

---

## 課題6：配線層の任意項目（転送シグナル、フック）

前回の配線層の整理（MatchCoordinator と KifuLoadCoordinator を一度だけ作る、仲介クラスの削除）では、
次の項目を任意として残しました。

- **転送シグナルの集約**：`MatchCoordinatorWiring`（`src/ui/wiring/matchcoordinatorwiring.h`）は、
  `MatchCoordinator` と `GameStartCoordinator` のシグナルを14個の自分のシグナルに付け替えて転送しています
  （`gameOverStateChanged`、`matchGameEnded`、`requestAppendGameOverMove` など）。司令塔が一度だけ作られるように
  なったので、受け手が司令塔のシグナルへ直接つなげるものは、転送をやめられる可能性があります。
  `tests/tst_wiring_contracts.cpp` が転送を確かめているので、合わせて直してください。
- **フックからシグナルへ**：`MatchCoordinatorHooks`（`src/game/matchcoordinatorhooks.h`）の `UI` と `Game` には、
  `std::function` のフックが並んでいます。このうち通知だけのもの（戻り値がなく、呼び出し側が結果を待たないもの。
  `updateTurnDisplay`、`showMoveHighlights`、`appendEvalP1/P2` など）は、MatchCoordinator のシグナルにできます。
  問い合わせ（`usesHandicapNames` など、戻り値を使うもの）はフックのまま残します。

どちらも、実施する価値があるかを読んで判断し、判断の理由を報告してください。実施するときは、
終局処理の順序（終局の記録 → アイドル状態への遷移）が変わらないことを確かめてください。
`docs/dev/ui-state-policy.md` に順序の説明があります。

---

## 課題7：古い記述と小さな問題の修正

まとめて1回で扱える小さな修正です。

### 文書

- `CLAUDE.md`
  - `docs/dev/deps-hooks-refs-pattern.md` へのリンクが切れています（実物は `docs/dev/done/deps-hooks-refs-pattern.md`）。
  - `mainwindowuibootstrapper.cpp/.h` とありますが、`.h` はありません。
- `docs/dev/developer-guide.md`
  - 14.2 の `m_testHelper`、14.5.1 の TestAutomationHelper の節、14.6 のスロット表。TestAutomationHelper は存在しません。
  - 17.3 は、ensure* を mainwindow.cpp に足すという古い手順のままです（今は MainWindowServiceRegistry とサブレジストリ）。
  - 1.6 の規模の表が古いです。
  - 4.1 の ConsiderationWiring::Deps の例が、`analysisTab` のままです（実物は `considerationTabManager` と `thinkingInfo1`）。
  - 第1章と 17.1 に「`GLOB_RECURSE` でソースを自動で取り込む」とありますが、実際の CMakeLists.txt はファイルを列挙しています。
  - 16.4 は、`main.cpp` の古いコードと3言語の前提のままです（今は4言語）。
  - UiActionsWiring の例、`resetPiecesToStand`（今は `resetPiecesToBox`）。
- `docs/dev/registry-boundary.md`：存在しない GameSubRegistry・GameSessionSubRegistry・GameWiringSubRegistry が
  出てきます（実際は `gamesubregistry*.cpp` で、MainWindowServiceRegistry の分割ファイル）。上限の値（SR ≤ 11、
  Foundation ≤ 17、Kifu ≤ 10）も、KPI テスト（25／15／8）と合っていません。
- `docs/dev/game-play-to-resignation-flow.md` の708行と869行：`kifuLoadCoordinator->resetBranchContext()` は
  削除済みのメソッドです。
- `docs/dev/game-start-flow.md`：プリセットのコード片に `c2.startDlg` が残っています。対局時計のパスと行番号も古いです。
- `docs/dev/move-and-record-data-structures.md`
  - 1.2〜1.4 の座標と QChar の盤の説明が古いです。
  - 8.3 に、プリセットで始めるときの流れが書かれていません。`requestBranchTreeResetForNewGame` がライブセッションを
    捨て、最初の着手で `LiveGameSessionUpdater` が作り直す流れです。

### コード

- clazy level0/1 の既存の警告。前回の変更ファイルだけを調べた結果です。ほかのファイルにもあるかもしれないので、
  全体を調べてから直してください。
  ```bash
  git ls-files 'src/*.cpp' | xargs -P 12 -n 3 clazy-standalone -p build --checks=level0,level1 2>&1 | grep -E '\[-Wclazy-'
  ```
  - range-loop-detach：`automationcommands_kifu.cpp:167`、`boardcolordialog.cpp:151`・`:174`、
    `tsumecollectiondialog.cpp:95`、`gameinfopanecontroller.cpp:172`、`jkfmoveparser.cpp:75`
  - non-pod-global-static：`appsettings.cpp:189`（`kAppearanceSections`）、`branchtreemanager.cpp:28-29`（QPen）
  - qmap-with-pointer-key：`menuwindow.cpp:315`
  - use-static-qregularexpression：`menuwindow.cpp:174`
  - strict-iterators：`thinkinginfopresenter.cpp:245`
- `AppSettings::migrateSettingsIfNeeded()`（`src/services/appsettings.cpp`）がどこからも呼ばれていません。
  起動時に呼ぶか、消すかを決めてください。
- `AnalysisSettings::kifuAnalysisResultsWindowSize` は保存するだけで、読み込まれていません
  （`src/analysis/analysisresultspresenter.cpp:457`）。棋譜解析の結果ウィンドウ（ドック）の大きさを
  復元するか、設定を消すかを決めてください。

### 完了の条件

共通の検証に加えて、clazy level0/1 の警告が0件であること（警告を止めた場合は、その理由を報告に書くこと）。
