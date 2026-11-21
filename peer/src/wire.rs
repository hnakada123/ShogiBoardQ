//! 長さ付きのメッセージ（4バイトのネットワークバイト順の長さ＋本体）の読み書き
//!
//! 対局ストリーム（game。本体はフレーム）と待ち合わせのストリーム（meet。本体は決定的CBOR）で共通。IPC は
//! tokio の入出力で、長さの範囲と終端の扱いも違うので ipc に置く。

use futures::{AsyncRead, AsyncReadExt, AsyncWrite, AsyncWriteExt};

/// 読み取りの失敗
#[derive(Debug, PartialEq, Eq)]
pub enum ReadError {
    /// 長さを読む前に閉じた
    Closed,
    /// 読み取りの失敗（本体の途中で閉じたときも）
    Io,
    /// 長さが 0 か上限を超える
    Invalid,
}

/// 1つ読む（長さを確かめてから領域を確保する）。max は本体の上限
pub async fn read<R: AsyncRead + Unpin>(reader: &mut R, max: usize) -> Result<Vec<u8>, ReadError> {
    let mut len_buf = [0u8; 4];
    match reader.read_exact(&mut len_buf).await {
        Ok(()) => {}
        Err(e) if e.kind() == std::io::ErrorKind::UnexpectedEof => return Err(ReadError::Closed),
        Err(_) => return Err(ReadError::Io),
    }
    let len = usize::try_from(u32::from_be_bytes(len_buf)).map_err(|_| ReadError::Invalid)?;
    if len == 0 || len > max {
        return Err(ReadError::Invalid);
    }
    let mut body = vec![0u8; len];
    reader.read_exact(&mut body).await.map_err(|_| ReadError::Io)?;
    Ok(body)
}

/// 1つ書く
pub async fn write<W: AsyncWrite + Unpin>(writer: &mut W, body: &[u8]) -> std::io::Result<()> {
    let len = u32::try_from(body.len()).map_err(|_| std::io::Error::other("message too large"))?;
    writer.write_all(&len.to_be_bytes()).await?;
    writer.write_all(body).await?;
    writer.flush().await
}

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn roundtrip_and_limits() {
        let mut buf = Vec::new();
        assert!(write(&mut buf, b"abc").await.is_ok());
        assert_eq!(buf, [0, 0, 0, 3, b'a', b'b', b'c']);
        assert_eq!(
            read(&mut futures::io::Cursor::new(buf.clone()), 3).await,
            Ok(b"abc".to_vec())
        );
        assert_eq!(
            read(&mut futures::io::Cursor::new(buf), 2).await,
            Err(ReadError::Invalid)
        );
        let empty: &[u8] = &[0, 0, 0, 0];
        assert_eq!(
            read(&mut futures::io::Cursor::new(empty), 3).await,
            Err(ReadError::Invalid)
        );
        let nothing: &[u8] = &[];
        assert_eq!(
            read(&mut futures::io::Cursor::new(nothing), 3).await,
            Err(ReadError::Closed)
        );
        let short: &[u8] = &[0, 0, 0, 3, b'a'];
        assert_eq!(read(&mut futures::io::Cursor::new(short), 3).await, Err(ReadError::Io));
    }
}
