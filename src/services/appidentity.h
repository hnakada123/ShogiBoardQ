#ifndef APPIDENTITY_H
#define APPIDENTITY_H

/// @file appidentity.h
/// @brief このアプリ（P2P対局の実験版）の名前。通常版と設定・データ・待受が混ざらないよう、ここだけで決める
///
/// 実験版は通常版の ShogiBoardQ と同じ PC に入れて使う（開発方針 4章）。アプリ名を変えると、Qt の標準の場所と
/// SHOGIBOARDQ_*_HOME の下のフォルダが変わり、設定・データ・キャッシュが分かれる。自動化 API のソケットのように
/// アプリ名によらない場所を使う待受は、ここの名前を使って分ける。

namespace AppIdentity {

/// Qt のアプリ名（設定・データ・キャッシュのフォルダ名）
inline constexpr char kApplicationName[] = "ShogiBoardQ-P2P";
/// 設定ファイルの名前
inline constexpr char kSettingsFileName[] = "ShogiBoardQ-P2P.ini";
/// デスクトップエントリの名前（拡張子なし。Linux の Wayland でアイコンと関連付ける）
inline constexpr char kDesktopFileName[] = "shogiboardq-p2p";
/// 実行時のフォルダ（XDG_RUNTIME_DIR の下）と、Windows の名前付きパイプの名前の接頭辞
inline constexpr char kRuntimeName[] = "shogiboardq-p2p";

} // namespace AppIdentity

#endif // APPIDENTITY_H
