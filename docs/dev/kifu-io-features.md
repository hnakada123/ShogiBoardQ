# 棋譜ファイル読み込み・出力・クリップボード機能

## 1. ファイル読み込み

### メニュー

- **ファイル(F) → 棋譜ファイルを開く** (`actionOpenKifuFile`)

### 対応フォーマット

| フォーマット | 拡張子 | 変換クラス |
|---|---|---|
| KIF | `.kif`, `.kifu`, `.ki2`, `.ki2u` | `KifToSfenConverter`, `Ki2ToSfenConverter` |
| CSA | `.csa` | `CsaToSfenConverter` |
| JKF | `.jkf` | `JkfToSfenConverter` |
| USI | `.usi`, `.sfen` | `UsiToSfenConverter` |
| USEN | `.usen` | `UsenToSfenConverter` |

### ファイルダイアログのフィルタ

```
Kifu Files (*.kif *.kifu *.ki2 *.ki2u *.csa *.jkf *.usi *.sfen *.usen)
KIF Files (*.kif *.kifu *.ki2 *.ki2u)
CSA Files (*.csa)
JKF Files (*.jkf)
USI Files (*.usi *.sfen)
USEN Files (*.usen)
```

### 主要クラス

- **`KifuLoadCoordinator`** (`src/kifu/kifuloadcoordinator.h/.cpp`) — 読み込み全体を統括
  - `loadKifuFromFile(filePath)` — KIF形式
  - `loadKi2FromFile(filePath)` — KI2形式
  - `loadCsaFromFile(filePath)` — CSA形式
  - `loadJkfFromFile(filePath)` — JKF形式
  - `loadUsiFromFile(filePath)` — USI形式
  - `loadUsenFromFile(filePath)` — USEN形式
  - `loadKifuFromString(content)` — テキストから形式を自動判定して読み込み
  - `loadPositionFromSfen(sfenStr)` — SFEN局面の読み込み
  - `loadPositionFromBod(bodStr)` — BOD局面の読み込み（内部でSFENに変換）

---

## 2. ファイル出力（保存）

### メニュー

- **ファイル(F) → 上書き保存** (`actionSave`)
- **ファイル(F) → 名前を付けて保存** (`actionSaveAs`)
- **ファイル(F) → 盤面画像を保存** (`actionSaveBoardImage`)
- **ファイル(F) → 評価値グラフを保存** (`actionSaveEvaluationGraph`)

### 対応フォーマット（名前を付けて保存）

| フォーマット | 拡張子 | 保存ダイアログでの表示 |
|---|---|---|
| KIF | `.kifu`, `.kif` | KIF形式 (*.kifu *.kif) |
| KI2 | `.ki2`, `.ki2u` | KI2形式 (*.ki2 *.ki2u) |
| CSA | `.csa` | CSA形式 (*.csa) |
| JKF | `.jkf` | JKF形式 (*.jkf) |
| USEN | `.usen` | USEN形式 (*.usen) |
| USI | `.usi` | USI形式 (*.usi) |

### 画像出力

| 対象 | メニュー | 処理クラス |
|---|---|---|
| 盤面画像 | ファイル(F) → 盤面画像を保存 | `BoardImageExporter::saveImage()` |
| 評価値グラフ | ファイル(F) → 評価値グラフを保存 | `BoardImageExporter::saveImage()` |

### 主要クラス

- **`KifuExportController`** (`src/kifu/kifuexportcontroller.h/.cpp`) — エクスポート処理全体を管理
  - `saveToFile()` — ダイアログを表示してファイル保存
  - `overwriteFile(filePath)` — 既存ファイルへの上書き保存
  - `autoSaveToDir(saveDir, outPath)` — 指定ディレクトリへの自動保存（ダイアログなし）
- **`KifuSaveCoordinator`** (`src/kifu/kifusavecoordinator.h/.cpp`) — 保存ダイアログ表示と形式選択
  - `saveViaDialogWithUsi()` — 全6形式（KIF/KI2/CSA/JKF/USEN/USI）に対応
- **`GameRecordModel`** (`src/kifu/gamerecordmodel.h/.cpp`) — 内部データから各形式へ変換
  - `toKifLines()`, `toKi2Lines()`, `toCsaLines()`, `toJkfLines()`, `toUsenLines()`, `toUsiLines()`
- **`BoardImageExporter`** (`src/board/boardimageexporter.h/.cpp`) — 盤面・グラフの画像保存

---

## 3. クリップボードコピー

### メニュー

- **編集(E) → 棋譜コピー** （サブメニュー）
  - KIF形式でコピー (`actionCopyKIF`)
  - KI2形式でコピー (`actionCopyKI2`)
  - CSA形式でコピー (`actionCopyCSA`)
  - USI形式でコピー（現在の手まで）(`actionCopyUSICurrent`)
  - USI形式でコピー（全手）(`actionCopyUSIAll`)
  - JKF形式でコピー (`actionCopyJKF`)
  - USEN形式でコピー (`actionCopyUSEN`)
- **編集(E) → 局面コピー** （サブメニュー）
  - SFEN形式でコピー (`actionCopySFEN`)
  - BOD形式でコピー (`actionCopyBOD`)
- **編集(E) → 盤面画像をコピー** (`actionCopyBoardToClipboard`)

### 棋譜コピー

| フォーマット | 内容 | メソッド |
|---|---|---|
| KIF | 棋譜全体をKIF形式でコピー | `KifuExportController::copyKifToClipboard()` |
| KI2 | 棋譜全体をKI2形式でコピー | `KifuExportController::copyKi2ToClipboard()` |
| CSA | 棋譜全体をCSA形式でコピー | `KifuExportController::copyCsaToClipboard()` |
| USI（現在の手まで） | 現在表示中の手までをUSI形式でコピー | `KifuExportController::copyUsiCurrentToClipboard()` |
| USI（全手） | 棋譜全体をUSI形式でコピー | `KifuExportController::copyUsiToClipboard()` |
| JKF | 棋譜全体をJKF形式でコピー | `KifuExportController::copyJkfToClipboard()` |
| USEN | 棋譜全体をUSEN形式でコピー | `KifuExportController::copyUsenToClipboard()` |

### 局面コピー

| フォーマット | 内容 | メソッド |
|---|---|---|
| SFEN | 現在の局面をSFEN文字列でコピー | `KifuExportController::copySfenToClipboard()` |
| BOD | 現在の局面をBOD（テキスト盤面図）でコピー | `KifuExportController::copyBodToClipboard()` |

### 盤面画像コピー

| 内容 | メソッド |
|---|---|
| 盤面を画像としてクリップボードにコピー | `BoardImageExporter::copyToClipboard()` |

### 主要クラス

- **`KifuExportController`** (`src/kifu/kifuexportcontroller.h/.cpp`) — コピー操作の窓口
- **`KifuClipboardService`** (`src/kifu/kifuclipboardservice.h/.cpp`) — クリップボード操作の実処理
  - `copyKif()`, `copyKi2()`, `copyCsa()`, `copyUsi()`, `copyUsiCurrent()`, `copyJkf()`, `copyUsen()`, `copySfen()`, `copyBod()`
- **`BoardImageExporter`** (`src/board/boardimageexporter.h/.cpp`) — 盤面画像のクリップボードコピー

---

## 4. クリップボード貼り付け

### メニュー

- **編集(E) → 棋譜貼り付け** (`actionPasteKifu`)

### 自動判定対応フォーマット

クリップボードのテキストから以下の形式を自動判定して読み込む（`KifuClipboardService::detectFormat()`）。

| フォーマット | 判定基準 |
|---|---|
| KIF | `手数`、`指し手` などのパターンを含む日本語テキスト |
| KI2 | `▲` または `△` で始まるテキスト |
| CSA | `V2`, `N+`, `N-`, `P1` で始まる、またはCSA形式の指し手パターン |
| USI | `moves` キーワードを含む |
| JKF | `{` または `[` で始まるJSON形式 |
| USEN | 20文字以上の `[A-Za-z0-9+/=]` で構成されるBase64風テキスト |
| SFEN | `position sfen`, `sfen`, `startpos` で始まる |

### 主要クラス

- **`KifuClipboardService`** (`src/kifu/kifuclipboardservice.h/.cpp`)
  - `getClipboardText()` — クリップボードからテキスト取得
  - `detectFormat(text)` — 形式の自動判定
- **`KifuLoadCoordinator`** (`src/kifu/kifuloadcoordinator.h/.cpp`)
  - `loadKifuFromString(content)` — 自動判定した形式で読み込み

---

## 5. クラス構成図

```
ファイル読み込み:
  MainWindow → KifuLoadCoordinator → 各形式 Converter (KifToSfenConverter 等)

ファイル保存:
  MainWindow → KifuExportController → KifuSaveCoordinator → KifuIoService
                                     → GameRecordModel (各形式への変換)

クリップボードコピー:
  UiActionsWiring → KifuExportController → KifuClipboardService
                                          → GameRecordModel (各形式への変換)

クリップボード貼り付け:
  MainWindow → KifuClipboardService (形式判定)
             → KifuLoadCoordinator (読み込み)

画像出力:
  MainWindow → BoardImageExporter (保存/クリップボードコピー)
```
