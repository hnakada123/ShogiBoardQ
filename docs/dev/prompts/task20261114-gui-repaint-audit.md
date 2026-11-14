# 調査依頼：読み筋ウィンドウなど、まだ確かめていない GUI の描画に無駄な描き直しが無いかを確認する

ShogiBoardQ は、将棋盤・棋譜欄・思考表などを「変わった所だけ描き直す」ように高速化しています。
2026-11-14 に、盤・駒台・背景・棋譜欄・縦のスクロールバー・CSA 通信対局のステータスバーを実アプリで測り、
見つけた無駄な描き直しを直しました（下の「確認済みの範囲」）。
**それ以外の描画部分（読み筋ウィンドウ、思考表、検討タブ、評価値グラフ、分岐ツリー、通信ログ、各ダイアログなど）に、
無駄な描き直しが無いかを実アプリで測って確認してください。**

この依頼は、過去の会話を参照できない状態でも実行できるようにまとめています。説明・報告・コミットメッセージは日本語でお願いします。

## 依頼の範囲

1. 下の「確認する対象」を実アプリで測り、部品ごとの描き直しの回数と範囲を表にまとめる。
2. 無駄な描き直しを見つけたら、どの処理が頼んでいるかまで調べて報告する。
3. **修正は、報告を見てから指示します**（依頼に「修正まで行ってください」とあれば、下の「直すとき」に従って修正まで行う）。
4. コミットと push は指示を待つ。

### 「無駄な描き直し」とするもの

- 見た目が変わらないのに描き直している
- 変わった所より広い範囲（表示領域の全体など）を描き直している
- 不透明でない部品の描き直しで、その下にある親の部品まで同じ範囲を描いている
- 1回の操作で同じ内容を2回以上描いている
- 何も操作していないのに、定期的に描き直している（時計の秒の表示のような、見た目が変わるものは除く）

量が小さくても挙げてください。直すかどうかは、面積・頻度（1手ごと・思考情報1行ごと・毎秒など）・直しやすさを添えて判断材料にします。

## 作業の前に読むもの

- `CLAUDE.md`（構成と決まり）
- メモリ `ui-performance-measurement`（測り方・Qt の描き直しの落とし穴・2026-11-14 の結果）、
  `p2p-two-app-local-verification`（2つのアプリでの P2P対局、CSA、gdb での観測）、
  `docs-screenshot-workflow`・`live-game-verification-workflow`（Xvfb と自動化 API での操作）
- `docs/dev/reports/board-input-performance-2026-10-11.md`（2026-10-11〜12 の高速化の記録。項目ごとの原因と対策）
- `scripts/docs/README.md`（`launch.sh`・`rpc.py`）

## 確認済みの範囲（やり直さない）

2026-11-14 に、通常対局（人間同士）・CSA 通信対局・P2P対局の「1手指す」「相手の手が届く」「何もしない3秒」を測りました。
同じ6手（歩を突く・駒を取って成る・駒を取る・駒を打つ）で、3つの対局とも次の値です。

| 部分 | 1手あたりの描き直し | 直したコミット |
|---|---|---|
| 盤のマス | 歩を突く手で 6.7%、駒を取って成る手で 23.8%（合法な移動先の印を含む） | `13c5eddb`（確認待ちの表示の後に同じマスを描き直さない） |
| 駒台 | 変わったセルだけ（16 セル中の1つ = 6.2%） | 同上 |
| 盤の縁・背景 | 縁は駒の影のはみ出し分だけ。背景は対局者カード（手番の札・時計）の下だけ | — |
| 棋譜欄の表示領域（420×505） | 2行分（19,740 画素）。直す前は全体（212,100 画素） | `67d71e24` |
| 棋譜欄の見出し | 描き直さない。直す前は全体 | `67d71e24` |
| 棋譜欄・思考表の縦のスクロールバー | 行が収まっている間は描き直さない | `1aebc8f3` |
| CSA のステータスバー | 1手ごとには描き直さない | `353079c0` |

2026-10-11〜12 には、駒のドラッグ・クリック・着手、対局外で指したときの棋譜欄と評価値グラフ、思考表の行の追加、
思考情報の表示（エンジン情報の帯・読み筋のセル）を高速化しています（上の記録）。

**ただし、その後の作り直しで元に戻っていた所がありました。** 棋譜欄は、表の部品（`IncrementalTableView`）だけのテストは
通っていたのに、プレゼンタ（`KifuDisplayPresenter`）を通すと毎手、全体を描き直していました（2026-10-16 の作り直し
`ebe78485` で入った）。10月に高速化した所も、**部品の単体ではなく実アプリの経路で**測り直してください。

## 確認する対象

優先度の高い順です。対象ごとに、利用者がよく行う操作（シナリオ）で測ってください。

### 1. 読み筋ウィンドウ（最優先）

`src/dialogs/pvboarddialog`（`PvBoardDialog`）。盤は `src/views/boardviewerwidget`（`BoardViewerWidget`）、
上の帯は `src/widgets/pvmovestrip`（`PvMoveStrip`）。思考表・検討タブ・棋譜解析の結果の表の「盤面」ボタンで開く。

- 局面を1手送る・戻す・先頭と末尾へ飛ぶ（ボタン、帯の語のクリック）: 盤は変わったマスと駒台だけか。帯は、今の局面の語と
  前の語だけを塗り直しているか（全部の語を描き直していないか）
- 拡大・縮小・回転（全体を描き直すのは正当。2回描いていないか）
- 開いたまま、元の思考表が更新され続けるとき（対局中・検討中）: 読み筋ウィンドウが描き直されていないか
- 開いた直後に何回描くか

### 2. エンジンが動いている間の表示

対局（人間対エンジン・エンジン同士）と検討で、思考情報が1秒に何行も届く間を測る。

- 思考表（`ThinkingPanel`・`ThinkingTableView`・`PvTableView`）と、上の情報帯（`EngineInfoWidget`）
- 検討タブ（`ConsiderationPanel`）
- 盤の矢印（`CandidateArrowPresenter` → `ShogiViewHighlighting::setArrows`。矢印が変わらないのに描き直していないか）
- USI通信ログ（`UsiLogPanel`・`LogPanel`）。タブの裏にあるときも測る
- 評価値グラフ（`EvaluationChartWidget`）: 1手ごとに何回・どの範囲を描くか
- ツールバー・メニューバー・ステータスバー: 手番が替わるたびに、項目の有効・無効の切り替えで描き直していないか
  （`UiStatePolicyManager`）

### 3. 棋譜欄のまわり（2026-11-14 の計測で気になった所）

- 分岐候補欄（`kifuBranchTable` 120×507）が、一部の手で表示領域の全体（約6万画素）を描いていた
- 局面送りのボタンの列（`StepButtons`・`kifuNav*`）が描き直される手があった
- 対局情報（`GameInfoPane`）・コメント欄（`CommentEditorPanel`）・分岐ツリー（`BranchTreeView`）: 1手ごと、
  棋譜をたどるとき

### 4. 棋譜をたどる操作

棋譜を読み込んで、局面送りのボタン・棋譜欄のクリック・分岐ツリーのクリック・評価値グラフのクリックで移る。
盤（局面を丸ごと替えたときに違うマスだけを描く）は確認済みなので、**ほかのドック**が何を描くかを見る。
長い棋譜（200手以上）で、棋譜欄がスクロールするときも測る。

### 5. そのほかの画面

- 棋譜解析（`KifuAnalysisController`・`AnalysisResultsPanel`）: 1局面の結果が出るたび
- 定跡ウィンドウ（`JosekiWindow`・`JosekiTableView`）: 局面が変わるたび
- 詰将棋を解く画面（`TsumePlayDialog`）・詰将棋問題一覧（`TsumePositionPreview`）・局面集ビューア（`SfenCollectionDialog`）
- P2P対局のウィンドウ（`P2pWindow`）: 司令塔が 250ms ごとに `statusChanged` を出す。文字が同じなら描き直していないか
- CSA 通信対局の待機ダイアログ・通信ログの窓
- 対局中・検討中・待機中に、何も操作しない10秒

時間が足りなければ、1〜4を先に終えて、5は測れた分と残りを分けて報告してください。

## 測り方

### 計測コード（一時的。コミットしない）

`src/app/main.cpp` にアプリ全体のイベントフィルタを入れ、Paint イベントごとに1行を、環境変数 `SBQ_PAINT_LOG` のファイルへ書きます。
2026-11-14 に使ったものが `build/docwork/paintcheck/apply_probe.py` に残っていれば、それを使ってください
（`build/` は git 管理外。無ければ下のコードを `main.cpp` に入れる）。

```cpp
// #include <QIcon> の後に入れる
#include "shogiview.h"
#include <QDateTime>
#include <QPaintEvent>
#include <QWidget>
#include <cstdio>
namespace {
class PaintProbe : public QObject
{
public:
    explicit PaintProbe(const QByteArray& path) : m_file(std::fopen(path.constData(), "w")) {}
protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (m_file && event->type() == QEvent::Paint && watched->isWidgetType()) {
            const auto* widget = static_cast<QWidget*>(watched);
            const QRegion region = static_cast<QPaintEvent*>(event)->region();
            qint64 area = 0;
            QByteArray rects;
            const auto* view = qobject_cast<const ShogiView*>(widget);
            for (const QRect& rect : region) {
                area += qint64(rect.width()) * rect.height();
                if (view) rects += QByteArray::number(rect.x()) + ',' + QByteArray::number(rect.y()) + ','
                              + QByteArray::number(rect.width()) + ',' + QByteArray::number(rect.height()) + '|';
            }
            if (view && view->board()) {
                const BoardGeometry& g = view->boardGeometry();
                const QRect b = g.board(), bs = g.stand(Turn::Black), ws = g.stand(Turn::White);
                const QRect sf = g.surface().toAlignedRect();
                std::fprintf(m_file, "GEOM\t%d,%d,%d,%d\t%d,%d,%d,%d\t%d,%d,%d,%d\t%d,%d,%d,%d\t%d,%d\n",
                             b.x(), b.y(), b.width(), b.height(), bs.x(), bs.y(), bs.width(), bs.height(),
                             ws.x(), ws.y(), ws.width(), ws.height(), sf.x(), sf.y(), sf.width(), sf.height(),
                             view->width(), view->height());
            }
            const QWidget* top = widget->window();
            std::fprintf(m_file, "%lld\t%s\t%s\t%s\t%lld\t%d\t%dx%d\t%s\n",
                         static_cast<long long>(QDateTime::currentMSecsSinceEpoch()),
                         widget->metaObject()->className(), qPrintable(widget->objectName()),
                         top->metaObject()->className(), static_cast<long long>(area),
                         static_cast<int>(region.rectCount()), widget->width(), widget->height(), rects.constData());
            std::fflush(m_file);
        }
        return false;
    }
private:
    std::FILE* m_file;
};
} // namespace

// main() の QApplication a(argc, argv); の直後に入れる
if (qEnvironmentVariableIsSet("SBQ_PAINT_LOG")) a.installEventFilter(new PaintProbe(qgetenv("SBQ_PAINT_LOG")));
```

1行は、時刻（ミリ秒）・クラス・objectName・トップレベルのクラス・面積・矩形の数・部品の大きさ・矩形の一覧（盤 `ShogiView` だけ）を
タブで区切ったものです。盤を描くたびに、盤のマス・先手と後手の駒台・盤の縁の位置を `GEOM` の行にも書きます
（盤の描き直しを、マス・駒台・縁・背景に分けて集計するため）。

読み筋ウィンドウのように、同じクラスの部品が複数あるものは、トップレベルのクラス（4列目）で見分けます。
矩形を記録する部品を増やしたいとき（例: `PvMoveStrip`）は、`view` の判定を広げてください。

### 計測用の本体の作り方

P2P対局も自動化 API から操作できる撮影用のビルド `build-p2p-docs`（`-DENABLE_P2P_DOCS_AUTOMATION=ON`、RelWithDebInfo）を使います。

```bash
cp src/app/main.cpp build/docwork/main.cpp.orig
python3 build/docwork/paintcheck/apply_probe.py          # または上のコードを手で入れる
ninja -C build-p2p-docs ShogiBoardQ
cp build-p2p-docs/ShogiBoardQ build-p2p-docs/ShogiBoardQ-probe   # 同じフォルダに置く（通信プロセスと訳を見つけるため）
cp build/docwork/main.cpp.orig src/app/main.cpp          # すぐ戻す
git status --short src/app                                # 何も出ないこと
ninja -C build-p2p-docs ShogiBoardQ                       # 元のビルドに戻す
# 測り終えたら rm build-p2p-docs/ShogiBoardQ-probe
```

修正を確かめるときは、修正を入れた状態でもう一度この手順で作り直します。

### 起動と操作

- 起動は `scripts/docs/launch.sh`（Xvfb、`--automation`、設定・キャッシュ・データの隔離）。
  `SBQ_APP=build-p2p-docs/ShogiBoardQ-probe SBQ_WORK=<repo>/build/docwork/... SBQ_FRESH=1 SBQ_PAINT_LOG=<ログ>` を付けて
  `launch.sh <名前> ja_JP 97 1300 980`。エンジンを使うときは `SBQ_ENGINES=1`（`build/Hayanagi/hayanagi` を「Hayanagi」として登録）。
- 操作は自動化 API（`scripts/docs/rpc.py`。引数は `build/shogiboardq-cli describe` で確かめる）:
  `board.click`・`action.trigger`・`widget.setValue`・`widget.click`・`dialog.clickButton`（引数は `target`）・`dialog.list`・
  `widget.text`・`kifu.load`・`kifu.goto`・`app.state`・`screenshot.capture`。UI を操作するメソッドは応答の後に実行されるので、
  `ui.interaction` で終わったことを確かめる。
- 2026-11-14 の手順は `build/docwork/paintcheck/paint_check.py`（通常対局・CSA・P2P で6手指して集計する）、
  P2P対局の始め方は `build/docwork/p2pcheck/check.py`、CSA は `build/docwork/csacheck/csa_check.py` に残っています（あれば流用する）。
  通常対局は「対局」→ `actionStartGame` → `StartGameDialog` の `comboBoxPlayer1`・`comboBoxPlayer2`（0 が人間）→「対局開始」。
- 見本の棋譜は `scripts/docs/sample_kifu/`。

### 集計

操作の直前と、落ち着いた後（0.7秒ほど）の時刻でログを区切り、主画面（トップレベルが `MainWindow`）と各ダイアログについて、
部品（クラスと objectName）ごとに「回数・面積の合計・部品の大きさに対する割合」を出します。
ログを 600ms 以上あいた所で区切ると、1回の操作の分を取り出せます。

### 原因の調べ方

- どの手順が描き直しを頼むかは、テストの実行ファイルに一時的な関数を足して、手順を1つずつ実行して切り分けるのが早い
  （モデルに行を足す → 強調の行を移す → 選択を移す → スクロールする、のように分け、アプリ全体のイベントフィルタで
  Paint の面積を部品ごとに出す。プレゼンタありとなしを比べる）。
- Qt の中の呼び出しは、ソース `build/qtsrc/qtbase` で確かめる（例: `QHeaderViewPrivate::resizeSections` は最後に必ず
  見出し全体を描き直す。`QAbstractSlider::sliderChange` は見た目を見ずに描き直す）。
- 呼び出し元を実アプリで知りたいときは、`launch.sh` の `SBQ_APP` に `exec gdb -q -batch -x <script> --args <本体> "$@"` という
  包みを渡す（`build/docwork/csacheck/gdbapp.sh`）。

## 直すとき（「修正まで」と依頼されたとき）

1. **テストを先に書き、修正前に失敗することを確かめる。** 描き直しの範囲（Paint イベントの範囲）と、描き残しが無いこと
   （画面の取り込みと、全体を描き直した後の取り込みを比べる。部分描画の半透明の合成は ±1 の差を許す）の両方を見る。
   見本は `tests/tst_board_drag_repaint.cpp`・`tests/tst_incremental_table_view.cpp`・`tests/tst_move_record_updates.cpp`。
   部品の単体だけでなく、プレゼンタ・コントローラを通した経路をテストに入れる。
2. 修正する。見た目や動作が変わる修正（例: ステータスバーに出していた文をやめる）は、変わる点を報告に明記する。
   迷うものは、直す前に確認する。
3. 実アプリで測り直し、修正前後を表にする。
4. `ctest` 全体を通す（`cd build && ctest -j8 --output-on-failure`。約5分）。CSA 通信対局を変えたら、実 shogi-server の
   結合テストも流す（メモリ `csa-protocol-test-harness`）。`tr()` を変えたら翻訳ファイルを更新し、未翻訳0を保つ。
5. ガイドの画像に写る所が変わったら、撮り直す（Skill `guide-page`）。

## 注意

- **計測コードはコミットしない。** `main.cpp` を戻したことを `git status` で確かめ、計測用の本体は測り終えたら消す。
- 利用者が動かしている ShogiBoardQ（引数なしで起動したもの）を止めない。自分が起動したものだけを `app.quit` で止める。
- 自分が立てた `Xvfb :97` は PID を確かめて止める（`pgrep -x Xvfb` には MCP の headless サーバーの Xvfb も出る。そちらは触らない）。
- 計測は1倍で行う。小数倍率（`QT_SCALE_FACTOR=1.25`・`1.5`）では、Qt が画面のずらしを使えず全体の描き直しに切り替える所が
  ある（既知。無駄には数えない）。
- 思考情報の表示は、エンジンの出力の速さで回数が変わる。同じ局面・同じ時間で比べる（`go movetime` や、検討の秒数を固定する）。
- テストや計測を2つ同時に走らせない（ログが混ざり、負荷で偽の失敗が出る）。

## 報告の形

- 対象ごとの表（操作 × 部品: 回数・面積・部品に対する割合）
- 無駄と判断したもの: 何が・いつ・どれだけ・どの処理が頼んでいるか・直し方の案・直したときに変わる見た目や動作
- 無駄ではないと判断したもの（理由を1行で）
- 測れなかった対象と、その理由
