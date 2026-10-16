# 棋譜ファイル読み込み・出力・クリップボード機能

棋譜の形式は `KifuFormat`（`src/kifu/kifuformat.h`）の1つの enum で表し、読み込み・保存・変換で共通に使う。形式の判定と名前は `KifuFormats::detect()`（テキストの内容）・`detectFile()`（拡張子と内容）・`fromName()`・`name()`・`isSavable()` が受け持つ。

## 1. ファイル読み込み

### メニュー

- **ファイル(F) → 開く…** (`actionOpenKifuFile`)

### 対応フォーマット

| フォーマット | 拡張子 | 変換クラス |
|---|---|---|
| KIF | `.kif`, `.kifu` | `KifToSfenConverter` |
| KI2 | `.ki2`, `.ki2u` | `Ki2ToSfenConverter` |
| CSA | `.csa` | `CsaToSfenConverter` |
| JKF | `.jkf`（`.json` は内容で判定） | `JkfToSfenConverter` |
| USI | `.usi`, `.sfen` | `UsiToSfenConverter` |
| USEN | `.usen` | `UsenToSfenConverter` |

拡張子で形式が決まらないファイル（`.json` や未知の拡張子）は、`KifuFormats::detectFile()` がファイルの内容から判定する（棋譜の形式と分からなければ KIF とみなす）。指し手のない USI・SFEN は局面だけとして読む。

### 読めない手・指せない手・パス

どの形式も同じ扱いにする（`KifuParseCommon::truncationNotice()`）。

- 表記が読めない手、局面に合わない手、手番と違う符号の手（CSA）、指せない手があれば、その手順はその手の手前で打ち切り、
  読み込みは成功として続ける。警告に理由と「何手目まで読んだか」（`The record was read up to move N.`）を書き、画面には警告として出す。
- パスの手（KIF・KI2 の「パス」）も、対応していない手として同じに打ち切る（`passNotice()`）。
- 変化の手順も同じ。変化の最初の手から読めなければ、その変化は手のない変化になる（本譜は残す）。
- 開始局面が読めないときだけ、読み込みを失敗にする。

### ファイルダイアログのフィルタ

```
Kifu Files (*.kif *.kifu *.ki2 *.ki2u *.csa *.jkf *.json *.usi *.sfen *.usen)
KIF Files (*.kif *.kifu)
KI2 Files (*.ki2 *.ki2u)
CSA Files (*.csa)
JKF Files (*.jkf *.json)
USI/SFEN Files (*.usi *.sfen)
USEN Files (*.usen)
```

### 主要クラス

- **`KifuFileController`** (`src/ui/coordinators/kifufilecontroller.h/.cpp`) — メニューからの読み込みの窓口
  - `chooseAndLoadKifuFile()` — ファイルを選び、未保存の棋譜を確かめてから読み込む（読み込み中は進捗ダイアログを出し、キャンセルできる）
  - `onSfenCollectionPositionSelected(sfen)` — SFEN局面集で選んだ局面を反映する
- **`KifuLoadCoordinator`** (`src/ui/coordinators/kifuloadcoordinator.h/.cpp`) — 読み込んだ結果で分岐ツリーを作り直す
  - `loadFileAsync(filePath)` / `loadTextAsync(content)` — ワーカーで `KifuLoadParser` を呼び、終わったら `loadFinished` を出す
  - `cancelLoad()` — 読み込み中の処理を捨てる
  - `loadFile(filePath)` / `loadText(content)` — 同期版（自動化 API・テスト）
  - `loadPositionFromSfen(sfen)` — SFEN局面を開始局面にする
  - 読んだ棋譜の対局情報は `gameInfoLoaded` で知らせる
- **`KifuLoadParser`** (`src/kifu/kifuloadparser.h/.cpp`) — GUI を含まない読み込み口（結果は値 `KifuLoadResult`）
  - `parseFile(path)` — ファイルを1回だけ読み（`readFileText()`：CSA は CSA の文字コード宣言、それ以外は自動判別）、`parseContent()` に渡す
  - `parseContent(content, format)` — 形式ごとの変換器に振り分ける。指し手のない USI・SFEN と BOD は局面だけを `positionOnly` に返す
- 各形式の変換器（`src/kifu/formats/*tosfenconverter`）はテキストを読むだけで、ファイルは読まない（`parseText` / `detectInitialSfenFromText` / `extractGameInfoFromText`。KIF・KI2 は行に分けた後の `parseLines` / `extractGameInfoFromLines` も持つ）

---

## 2. ファイル出力（保存）

### メニュー

- **ファイル(F) → 上書き保存** (`actionSave`)
- **ファイル(F) → 名前を付けて保存…** (`actionSaveAs`)
- **ファイル(F) → 将棋盤の画像をファイルに保存…** (`actionSaveBoardImage`)
- **ファイル(F) → 評価値グラフの画像をファイルに保存…** (`actionSaveEvaluationGraph`)

### 対応フォーマット（名前を付けて保存）

| フォーマット | 拡張子 | 保存ダイアログでの表示 |
|---|---|---|
| KIF | `.kifu`, `.kif` | KIF形式 UTF-8 (*.kifu) / KIF形式 Shift_JIS (*.kif) |
| KI2 | `.ki2u`, `.ki2` | KI2形式 UTF-8 (*.ki2u) / KI2形式 Shift_JIS (*.ki2) |
| CSA | `.csa` | CSA形式 (*.csa) |
| JKF | `.jkf` | JKF形式 (*.jkf) |
| USEN | `.usen` | USEN形式 (*.usen) |
| USI | `.usi` | USI形式 (*.usi) |

保存形式は拡張子で決まる（`KifuSaveFile::saveFormatForPath()`）。分岐・消費時間・コメントを書けない形式へ保存するときは、失われる情報を示して続けるかを確かめる。

### 画像出力

| 対象 | メニュー | 処理クラス |
|---|---|---|
| 盤面画像 | ファイル(F) → 将棋盤の画像をファイルに保存… | `ImageExportController::saveBoardImage()` → `BoardImageExporter::saveImage()` |
| 評価値グラフ | ファイル(F) → 評価値グラフの画像をファイルに保存… | `ImageExportController::saveEvalGraphImage()` → `BoardImageExporter::saveImage()` |

### 主要クラス

- **`KifuFileController`** (`src/ui/coordinators/kifufilecontroller.h/.cpp`) — メニューからの保存の窓口
  - `saveKifuToFile()` — 名前を付けて保存。保存したパスを上書き保存先（`AppState::saveFileName`）にする
  - `overwriteKifuFile()` — 上書き保存先が無ければ名前を付けて保存する
  - `autoSaveKifuToFile(saveDir)` — 対局の終局時の自動保存（`MatchCoordinator::autoSaveRequested` から）
- **`KifuExportController`** (`src/ui/coordinators/kifuexportcontroller.h/.cpp`) — 保存とクリップボードへの書き出し
  - `saveToFile()` — ダイアログを表示してファイル保存
  - `overwriteFile(filePath)` — 既存ファイルへの上書き保存
  - `autoSaveToDir(saveDir, extension)` — 指定ディレクトリへの自動保存（ダイアログなし。名前は対局者と日時から作る）
  - `saveToPathWithoutDialogs(filePath, errorText)` / `exportLines(format)` — 自動化 API 用
- **`KifuSaveCoordinator`** (`src/ui/coordinators/kifusavecoordinator.h/.cpp`) — 保存で利用者に聞くこと（名前空間）
  - `saveViaDialog()` — 保存ダイアログで全6形式（KIF/KI2/CSA/JKF/USEN/USI）から選ばせる
  - `overwriteExisting()` — `.kif`/`.ki2` に Shift_JIS で表せない文字があれば UTF-8 で保存するか確かめる
  - `confirmDiscardUnsaved()` — 未保存の変更の保存・破棄・キャンセルを確かめる
- **`KifuSaveFile`** (`src/kifu/kifusavefile.h/.cpp`) — 保存形式の判定と書き出し（文字コードの選択を含む）
- **`GameRecordModel`** (`src/kifu/gamerecordmodel.h/.cpp`) — 分岐ツリーと対局情報から各形式の行を作る
  - `toKifLines()`, `toKi2Lines()`, `toCsaLines()`, `toJkfLines()`, `toUsenLines()`, `toUsiLines()`（いずれも `ExportContext` を受け取る）
- **`BoardImageExporter`** (`src/board/boardimageexporter.h/.cpp`) — 盤面・グラフの画像保存

---

## 3. クリップボードコピー

### メニュー

- **編集(E) → 棋譜コピー** （サブメニュー）
  - KIF形式 (`actionCopyKIF`)
  - KI2形式 (`actionCopyKI2`)
  - CSA形式 (`actionCopyCSA`)
  - USI形式（現在の指し手まで）(`actionCopyUSICurrent`)
  - USI形式（全て）(`actionCopyUSIAll`)
  - JSON棋譜フォーマット (`actionCopyJKF`)
  - USEN (`actionCopyUSEN`)
- **編集(E) → 局面コピー** （サブメニュー）
  - SFEN形式 (`actionCopySFEN`)
  - BOD形式 (`actionCopyBOD`)
- **編集(E) → 将棋盤画像コピー** (`actionCopyBoardToClipboard`)
- **編集(E) → 評価値グラフ画像コピー** (`actionCopyEvalGraphToClipboard`)

### 棋譜コピー

| フォーマット | 内容 | メソッド |
|---|---|---|
| KIF | 棋譜全体をKIF形式でコピー | `KifuExportController::copyKifToClipboard()` |
| KI2 | 棋譜全体をKI2形式でコピー | `KifuExportController::copyKi2ToClipboard()` |
| CSA | 棋譜全体をCSA形式でコピー（分岐は書けないので本譜のみ） | `KifuExportController::copyCsaToClipboard()` |
| USI（現在の指し手まで） | 表示中の手順の、表示中の手までをUSI形式でコピー | `KifuExportController::copyUsiCurrentToClipboard()` |
| USI（全て） | 棋譜全体をUSI形式でコピー（分岐は書けないので本譜のみ） | `KifuExportController::copyUsiToClipboard()` |
| JKF | 棋譜全体をJKF形式でコピー | `KifuExportController::copyJkfToClipboard()` |
| USEN | 棋譜全体をUSEN形式でコピー | `KifuExportController::copyUsenToClipboard()` |

棋譜のコピーは保存と同じ `KifuExportController::exportLines()`（`GameRecordModel` の各形式の出力）で行を作り、クリップボードに書く。

### 局面コピー

| フォーマット | 内容 | メソッド |
|---|---|---|
| SFEN | 盤の局面をSFEN文字列でコピー | `KifuExportController::copySfenToClipboard()` |
| BOD | 盤の局面をBOD（テキスト盤面図）でコピー（`BodTextGenerator`）。駒落ちの棋譜は KIF の局面図と同じく持駒・手番を下手・上手で書く（対局情報の見出しが先手・後手ならそれに合わせる） | `KifuExportController::copyBodToClipboard()` |

### 画像コピー

| 内容 | メソッド |
|---|---|
| 盤面を画像としてクリップボードにコピー | `ImageExportController::copyBoardToClipboard()` → `BoardImageExporter::copyToClipboard()` |
| 評価値グラフを画像としてクリップボードにコピー | `ImageExportController::copyEvalGraphToClipboard()` → `BoardImageExporter::copyToClipboard()` |

### 主要クラス

- **`KifuExportController`** (`src/ui/coordinators/kifuexportcontroller.h/.cpp`) — 棋譜・局面のコピー（形式ごとの行の生成とクリップボードへの書き込み）
- **`ImageExportController`** (`src/ui/controllers/imageexportcontroller.h/.cpp`) — 画像の保存・コピーの窓口
- **`BoardImageExporter`** (`src/board/boardimageexporter.h/.cpp`) — 盤面・グラフ画像のクリップボードコピー

---

## 4. クリップボード貼り付け

### メニュー

- **編集(E) → 棋譜貼り付け…** (`actionPasteKifu`)

`KifuFileController::pasteKifuFromClipboard()` が `KifuPasteDialog` を開く。ダイアログでクリップボードの内容を貼り付けて「取り込む」を押すと、`importRequested` を受けた `KifuFileController::onKifuPasteImportRequested()` が未保存の棋譜を確かめてから `KifuLoadCoordinator::loadTextAsync()` で読み込む。貼り付けた棋譜はファイル由来ではないので、上書き保存先は消す。

### 自動判定対応フォーマット

貼り付けたテキストの形式は `KifuFormats::detect()` が次の順に判定する（`KifuLoadParser::parseText()` から呼ぶ）。

| フォーマット | 判定基準 |
|---|---|
| SFEN | テキスト全体が SFEN 局面（盤・手番・持駒・手数）だけ |
| JKF | `{` または `[` で始まるJSON形式 |
| USI | `position`、`startpos`、`sfen ` または SFEN の盤で始まる行を含む |
| USEN | テキスト全体が USEN の構造（`~` で区切った指し手列） |
| CSA | `V2`/`V3`、`PI`、`P1`〜`P9`、`N+`/`N-`、`$EVENT:` などのヘッダ行、`'` で始まる、または `+`/`-` と数字で始まる指し手行を含む |
| KIF | `手数----` の見出し、または数字で始まる指し手行を含む |
| KI2 | `▲`・`△` などで始まる指し手行を含む |
| BOD | 持駒の行（`先手の持駒` など）、盤の枠（`+---+`）、`|v`・`| ・` を含む |

どれにも当たらなければ KIF として読む。SFEN と BOD、指し手のない USI は局面だけとして読む。

### 主要クラス

- **`KifuPasteDialog`** (`src/dialogs/kifupastedialog.h/.cpp`) — 貼り付けるテキストの入力（クリップボードからの貼り付けボタンを持つ）
- **`KifuFileController`** (`src/ui/coordinators/kifufilecontroller.h/.cpp`)
  - `pasteKifuFromClipboard()` — ダイアログを開く
  - `onKifuPasteImportRequested(content)` — 取り込む
- **`KifuLoadCoordinator`** (`src/ui/coordinators/kifuloadcoordinator.h/.cpp`)
  - `loadTextAsync(content)` — 自動判定した形式で読み込み
- **`KifuLoadParser`** (`src/kifu/kifuloadparser.h/.cpp`)
  - `parseText(content)` — `KifuFormats::detect()` で形式を判定し、`parseContent(content, format)` でテキストのまま解析する（一時ファイルは作らない）

---

## 5. クラス構成図

```
ファイル読み込み:
  MenuActionsWiring → KifuFileController → KifuLoadCoordinator → KifuLoadParser (ワーカー)
                                                                 → 各形式 Converter (KifToSfenConverter 等)
                                         KifuLoadCoordinator → KifuBranchTree (分岐ツリーの作り直し)

ファイル保存:
  MenuActionsWiring → KifuFileController → KifuExportController → KifuSaveCoordinator → KifuSaveFile
                                                               → GameRecordModel (各形式への変換)

クリップボードコピー:
  MenuActionsWiring → KifuExportController → GameRecordModel (各形式への変換)
                                           → QClipboard

クリップボード貼り付け:
  MenuActionsWiring → KifuFileController → KifuPasteDialog (入力)
                                         → KifuLoadCoordinator → KifuLoadParser (形式判定と読み込み)

画像出力:
  MenuActionsWiring → ImageExportController → BoardImageExporter (保存/クリップボードコピー)
```
