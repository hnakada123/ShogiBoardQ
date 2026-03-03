# 現在ソースコードの改善点と修正方法（2026-03-03）

## 1. 目的
本ドキュメントは、現在の `ShogiBoardQ` ソースコードを対象に、
改善優先度の高いポイントと具体的な修正方法を整理したものです。

- 対象範囲: `MainWindow` ライフサイクル、Registry/Wiring、棋譜表示同期、USEN変換、テスト
- 前提方針: **ラムダ方式は採用しない**（`connect` は関数ポインタ、コールバックは `std::bind` / メンバ関数で実装）

---

## 2. 現状サマリ

- ビルド: `cmake --build build -j4` 成功
- テスト: `ctest --test-dir build --output-on-failure` は通過
- 直近で shutdown 安全性の暫定対策は実施済み
  - `src/app/mainwindowlifecyclepipeline.cpp:106-114`
  - `runShutdown()` で `m_queryService` を破棄せず `Deps` を無効化

---

## 3. 優先度付き改善項目

## 3.1 High: `m_queryService` の寿命と callback バインドの安全性を恒久化

### 問題
`std::bind(..., m_queryService.get(), ...)` のように生ポインタを callback に固定している箇所がある。

- `src/app/gamewiringsubregistry.cpp:150-152,161`
- `src/app/mainwindowfoundationregistry.cpp:139`

`runShutdown()` 側で暫定的に依存無効化を入れたが、将来 `reset()` 方針へ戻る変更や再初期化が入ると再び事故化しやすい。

### リスク
- 破棄済みサービスへのアクセス（Use-after-free）
- shutdown タイミングでのクラッシュ再発

### 修正方法（ラムダ不使用）
`GameWiringSubRegistry` / `MainWindowFoundationRegistry` に「安全な中継メンバ関数」を作り、
`std::bind` のターゲットを `this` に統一する。

例（方針）:
- `GameWiringSubRegistry::queryRemainingMsFor(MatchCoordinator::Player)`
- `GameWiringSubRegistry::queryIncrementMsFor(MatchCoordinator::Player)`
- `GameWiringSubRegistry::queryByoyomiMs()`
- `GameWiringSubRegistry::queryIsHumanSide(ShogiGameController::Player)`
- `MainWindowFoundationRegistry::queryIsGameActivelyInProgress()`

上記メソッド内部で `m_mw.m_queryService` の null ガードを行う。

### 実装ステップ
1. 上記メンバ関数を追加（`h/cpp`）
2. 既存 `std::bind(&MatchRuntimeQueryService::..., m_queryService.get(), ...)` を全置換
3. `runShutdown()` は現状の「依存無効化 + `m_playModePolicy.reset()`」を維持
4. クラッシュ再現テストを追加（後述 3.7）

---

## 3.2 High: `closeEvent()` の shutdown 呼び出し順序を見直す

### 問題
`MainWindow::closeEvent()` で `QMainWindow::closeEvent(e)` より先に `runShutdown()` を実行している。

- `src/app/mainwindow.cpp:126-131`

### リスク
- イベント受理前に内部状態を shutdown 済みにするため、終了拒否シナリオで状態不整合が起きうる

### 修正方法（ラムダ不使用）
`closeEvent()` を以下順序に変更:
1. 先に `QMainWindow::closeEvent(e)`
2. `if (!e->isAccepted()) return;`
3. `m_pipeline->runShutdown();`

### 実装ステップ
1. `src/app/mainwindow.cpp` の `closeEvent` を順序変更
2. `saveSettingsAndClose()` / `shutdownAndQuit()` との二重実行は既存ガード (`m_shutdownDone`) で維持
3. 終了拒否ケースを含む runtime テストを追加

---

## 3.3 Medium: `wireSignals()` 系の冪等性強化

### 問題
`KifuDisplayCoordinator::wireSignals()` は複数 `connect` を行うが `Qt::UniqueConnection` や配線済みフラグを持たない。

- `src/ui/coordinators/kifudisplaycoordinator.cpp:145-175`

現状は `BranchNavigationWiring` 側が生成を1回に抑えているが、将来の再配線経路追加で重複接続リスクがある。

- `src/ui/wiring/branchnavigationwiring.cpp:92-99,137`

### リスク
- 同一シグナルの多重配送
- 1操作でハンドラが2回以上実行される不具合

### 修正方法（ラムダ不使用）
以下のいずれかで冪等化:
- `Qt::UniqueConnection` を全 `connect` に付与
- `m_signalsWired` フラグを `KifuDisplayCoordinator` に追加し、2回目以降 return

### 実装ステップ
1. `KifuDisplayCoordinator` に `bool m_signalsWired = false;` 追加
2. `wireSignals()` 冒頭で配線済み判定
3. 主要 `connect` に `Qt::UniqueConnection` を付与
4. 「`wireSignals()` を2回呼んでも1回分しか反応しない」テスト追加

---

## 3.4 Medium: `KifuLoadCoordinator` 再生成時の `deleteLater()` 境界を明確化

### 問題
`createAndWireKifuLoadCoordinator()` で旧 coordinator を `deleteLater()` し、直後に新 coordinator を生成している。

- `src/app/kifusubregistry.cpp:341-345`

### リスク
- 旧インスタンスに積まれた queued signal が次ループで飛ぶ
- 新旧 coordinator のイベントが混在する可能性

### 修正方法（ラムダ不使用）
1. `deleteLater()` 前に `QObject::disconnect(old, nullptr, nullptr, nullptr);`
2. 必要なら `QPointer<KifuLoadCoordinator> old` を残して無効化確認
3. 生成/配線順を固定（旧切断 -> 旧破棄予約 -> 新生成）

### 実装ステップ
1. `src/app/kifusubregistry.cpp` に明示切断を追加
2. factory 返却物の parent を確認（MainWindow配下）
3. 旧 coordinator からの遅延通知が来ないことをテスト化

---

## 3.5 Medium: USENデコード失敗時の扱いを「プレースホルダ継続」から「検知可能な失敗」へ

### 問題
デコード失敗時に `?1` などのプレースホルダを入れて処理継続している。

- `src/kifu/formats/usentosfenconverter_decode.cpp:248-253`

### リスク
- 異常データが正常経路へ混入
- 上位で失敗検知しにくい（品質低下、診断困難）

### 修正方法（ラムダ不使用）
`decodeUsenMoves()` に厳密結果を返す内部APIを追加する。

提案:
- `DecodeResult { QStringList moves; int invalidCount; QString firstError; }`
- 既存公開APIは互換維持しつつ、`invalidCount>0` の場合に `errorMessage` を設定
- `parseWithVariations()` / `convertFile()` は厳密API経由へ切替

### 実装ステップ
1. `DecodeResult` と内部 `decodeUsenMovesStrict` を追加
2. 既存APIを strict API のラッパに変更
3. テストで「不正入力時にエラーが取れる」ことを追加

---

## 3.6 Low: ナビゲーション系デバッグログの出力量を制御

### 問題
`KifuDisplayCoordinator` / `KifuNavigationCoordinator` に高頻度 `qCDebug` が多い。

- `src/ui/coordinators/kifudisplaycoordinator.cpp:184-186,275`
- `src/app/kifunavigationcoordinator.cpp:36,86,157`

### リスク
- 実行時ノイズ増加
- 問題切り分け時に必要ログが埋もれる

### 修正方法（ラムダ不使用）
- 重要イベントだけ `qCInfo` へ格上げ、詳細トレースは category を分離
- `debug-logging-guidelines` に従い、重いログは条件付きに統一

### 実装ステップ
1. 連続発火箇所を整理（ENTER/LEAVE 全ログの削減）
2. category 切り替え（例: `lcUiNavTrace`）
3. 主要シナリオでログ可読性を確認

---

## 3.7 High: ライフサイクルの runtime テストを追加（構造検証偏重の補完）

### 問題
現状はソース文字列解析による「構造検証」テスト比率が高い。

- `tests/tst_app_lifecycle_pipeline.cpp:25-44`（`readSourceFile/readSourceLines`）
- `tests/tst_lifecycle_scenario.cpp:23-41`（同様）

### リスク
- 実行時の寿命バグや queued event 問題を検出しにくい

### 修正方法（ラムダ不使用）
`tests/` に runtime 系を追加:
- `MainWindow` 起動 -> 対局開始相当の初期化 -> `close()`
- close 後の queued 呼び出しでクラッシュしないことを確認
- 可能なら ASan ビルド job を CI に追加

### 実装ステップ
1. `tst_lifecycle_runtime.cpp` 新設
2. 既存スタブを流用して終了パスを通す
3. `ctest` で常時実行、ASan は CI の nightly/optional でも可

---

## 4. 推奨実施順（短期）

1. **3.1** callback 中継メソッド化（生ポインタ bind 撲滅）
2. **3.2** `closeEvent()` 順序修正
3. **3.3** `wireSignals()` 冪等化
4. **3.7** runtime テスト追加
5. 余力で **3.4 / 3.5 / 3.6** を順次実施

---

## 5. 受け入れ基準（Definition of Done）

- `m_queryService.get()` を直接 `std::bind` している箇所がゼロ
- `closeEvent()` が受理後に shutdown 実行
- `wireSignals()` の重複接続が防止される
- runtime ライフサイクルテストが1本以上追加される
- `cmake --build build` 成功
- `ctest --test-dir build --output-on-failure` 全件成功

---

## 6. 参考（今回確認した主要ファイル）

- `src/app/mainwindowlifecyclepipeline.cpp`
- `src/app/mainwindow.cpp`
- `src/app/gamewiringsubregistry.cpp`
- `src/app/mainwindowfoundationregistry.cpp`
- `src/app/kifusubregistry.cpp`
- `src/ui/coordinators/kifudisplaycoordinator.cpp`
- `src/ui/wiring/branchnavigationwiring.cpp`
- `src/kifu/formats/usentosfenconverter_decode.cpp`
- `tests/tst_app_lifecycle_pipeline.cpp`
- `tests/tst_lifecycle_scenario.cpp`
