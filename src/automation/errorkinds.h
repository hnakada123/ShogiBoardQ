#ifndef ERRORKINDS_H
#define ERRORKINDS_H

/// @file errorkinds.h
/// @brief CLI・自動化 API・MCP で共有するエラーの分類と語（GUI 非依存）

#include <QJsonArray>
#include <QString>

/**
 * @brief エラーの分類
 *
 * 同じ失敗はどの経路でも同じ語（slug）になる。CLI は `{"error":{"code":slug}}`、自動化 API は
 * JSON-RPC の `error.code`（数値）と `error.data.slug` で知らせ、MCP サーバーはその語をそのまま返す。
 */
enum class ErrorKind {
    InvalidJson,       ///< 要求が JSON でない（invalid_json）
    InvalidRequest,    ///< JSON-RPC の形が正しくない・大きすぎる（invalid_request）
    UnknownOperation,  ///< 知らないメソッド・コマンド（unknown_operation）
    InvalidArgument,   ///< 引数の誤り（invalid_argument）
    InternalError,     ///< 予期しない失敗（internal_error）
    NotAllowed,        ///< 許可されない操作（not_allowed）
    InvalidState,      ///< 今の状態ではできない（invalid_state）
    FileError,         ///< ファイルの読み書きの失敗（file_error）
    FileExists,        ///< 上書きの許可なしに既存のファイルへ書こうとした（file_exists）
    UnsavedChanges,    ///< 未保存の変更がある（unsaved_changes）
    NotFound,          ///< 対象（ファイル・ウィンドウ・部品など）が見つからない（not_found）
    InvalidContent,    ///< 棋譜や局面の中身が読めない（parse_error）
    EngineNotFound,    ///< 登録エンジンが見つからない（engine_not_found）
    EngineError,       ///< エンジンの起動・探索の失敗（engine_error）
    RenderError,       ///< 盤面画像を描けない（render_error）
};

/// 分類つきの失敗。GUI に依存しないサービスが返し、CLI（CliOutput::fail）と自動化 API（AutomationError）は
/// 分類をそのまま写す（呼び出し側で分類を推し量らない）
struct Failure {
    ErrorKind kind = ErrorKind::InternalError;
    QString message;
};

namespace ErrorKinds {

/// 分類の語
QString slug(ErrorKind kind);
/// JSON-RPC のエラーコード（AutomationErrorCode の値）
int rpcCode(ErrorKind kind);
/// 全分類の `[{slug, rpc_code}]`（shogiboardq-cli environment が出す）
QJsonArray toJson();

} // namespace ErrorKinds

#endif // ERRORKINDS_H
