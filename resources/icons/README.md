# アプリケーションアイコン

| ファイル | 用途 |
| --- | --- |
| `shogiboardq.png` | 元画像。macOS等のウィンドウとバージョン情報ダイアログで使用 |
| `shogiboardq.icns` | macOSアプリケーションバンドル用 |
| `linux/shogiboardq.png` | Linux用512×512px。絵柄の幅は画像の約89%（元画像は約80%） |
| `windows/shogiboardq.png` | Windows用512×512px。外側の余白なし。高DPI表示とICOの生成元 |
| `shogiboardq.ico` | Windows実行ファイルとウィンドウ用。16/20/24/32/40/48/64/96/128/256pxを収録 |

Linux用はQtリソース、CMakeのhicolor/512x512へのインストール、AppImageで共用します。
Windows用はICOに加えて512pxのPNGもQIconに登録します。
macOS用のPNG・ICNSは今回変更していません。

Linux・Windows用PNGは元画像を参照して内蔵 image_gen で再生成し、
ImageMagickのLanczosフィルターで512×512pxに縮小しています。
ICOはリポジトリのルートで次のコマンドを実行して再生成できます。

```sh
magick resources/icons/windows/shogiboardq.png \\
  -define icon:auto-resize=256,128,96,64,48,40,32,24,20,16 \\
  resources/icons/shogiboardq.ico
```

## Linux用の生成プロンプト

```text
Use case: precise-object-edit
Asset type: ShogiBoardQ Linux desktop application icon.
Input image 1 is the EDIT TARGET, the existing production icon.
Primary request: Recreate this SAME icon with much less transparent outer padding, so the visible icon is larger among Linux application icons. Preserve the existing identity and artwork as faithfully as possible.
Composition/framing: Square 1024x1024 PNG with actual transparent alpha background. The square wooden board should occupy approximately 944x944 pixels, centered with only 40 pixels of transparent padding on each edge (92% canvas width and height). Keep the entire design visible, no clipping. This is a precise enlargement/reframing edit, not a new logo design.
Invariants: Keep the same straight-on square honey-gold wooden board, its wood grain and thin dark grid, the same oversized elegant BLACK capital Q ring and tail, and the same central black Japanese character "将" with identical calligraphic strokes and proportions. Preserve relative placement of all elements, original colors, geometry, sharp square board corners, and original visual style.
Constraints: only reduce outer blank margin and scale the existing artwork to use the space. No extra symbols, text, decorative borders, drop shadows, rounded corners, lighting changes, perspective, mockup, or backdrop. The outside border must be genuinely transparent, never a checkerboard or black fill. Return the single finished icon.
```

## Windows用の生成プロンプト

```text
Use case: precise-object-edit
Asset type: ShogiBoardQ Windows application icon master, square PNG.
Input image 1 is the original production icon and the edit target.
Primary request: Preserve this existing ShogiBoardQ logo design and recreate it to completely FILL THE ENTIRE SQUARE CANVAS. Remove ALL exterior blank/transparent margins. The finished PNG must be opaque wood and logo from edge to edge, with absolutely ZERO padding.
Composition/framing: Exact square 1024x1024, straight-on square golden wooden board. The four wooden board edges coincide exactly with the four image/canvas edges. Cropping away the transparent exterior is the only intended change; keep the original entire board and internal composition. No inset square, no empty framing around the wood.
Invariants: Keep the original honey-gold wooden board grain and thin dark grid lines, the large black elegant capital Q ring and tail, and exact black calligraphic Japanese character "将" centered within it. Preserve recognizable original shape, character strokes, colors and overall design.
Constraints: edge-to-edge/full-bleed square icon, NO transparent margins, NO white margins, NO black margins, no rounding, shadow, new text, ornaments, or mockup. Optimize crisp edges for small Windows taskbar display. Return only the finished icon.
```
