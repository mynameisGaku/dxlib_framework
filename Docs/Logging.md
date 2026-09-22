# ログ（DXF_LOG）

`Toolbox/Log.h`は、デバッグ時に状態遷移や失敗理由を追うための軽量なログ機構です。STLを使わず、書式展開はスタック上の固定領域（1件1024バイト）で行います。

## 使い方

```cpp
#include "Toolbox/Log.h"

DXF_LOG_VERBOSE("Physics", "step %llu", static_cast<unsigned long long>(Step));
DXF_LOG_INFO("Scene", "Activating scene with task scope %u/%llu", Index, Generation);
DXF_LOG_WARNING("PhysicsDebug", "3D snapshot capture refused: %s", Reason);
DXF_LOG_ERROR("Application", "Frame failed: %s", Error.Message.CStr());
```

- 第1引数は分類名、続けてprintf形式のUTF-8書式と引数。GCC / Clangでは書式と引数の不一致を警告にします。
- 既定の出力先は、Windowsでは`OutputDebugStringW`（Visual Studioの出力ウィンドウ。日本語はUTF-16へ変換）と標準エラーです。
  形式は`[INFO][Scene] 本文 (SceneNavigator.cpp:212)`です。
- `SetLogSink(Sink, UserData)`で出力先を差し替えます。`nullptr`で既定に戻ります。出力先の呼出しは内部Mutexで直列化されます。
  出力先の中で発行したログは再入防止のため捨てます。出力先は例外を送出しないでください。

## 重要度の制御

| 設定 | 内容 |
|---|---|
| `DXF_LOG_COMPILE_LEVEL` | コンパイル時に残す最小重要度。0=Verbose、1=Info、2=Warning、3=Error、4=すべて除去。既定はDebugで0、Release（`NDEBUG`）で1 |
| `SetLogLevel(Level)` | 実行時の最小重要度。既定はコンパイル時設定と同じ。`ELogLevel::Off`ですべて停止 |

コンパイル時に除去された呼出しと、実行時に無効な重要度の呼出しでは、引数を評価しません。ホットループに置く場合もVerboseにすれば、Releaseでは実行コストがありません。

## 現在の記録箇所

| 分類 | 内容 | 重要度 |
|---|---|---|
| Application | 開始の成否（実行レーン数）、フレームを止めた最初の失敗、終了、所有スレッド境界への終了延期 | Info / Error / Verbose |
| Scene | Scene有効化時のTask Scope、Scope退役の失敗、遷移の失敗理由 | Info / Error / Warning |
| PhysicsDebug | 2D/3D Snapshot採取の拒否理由（途中失敗したStep後・上限超過など） | Warning |
| RenderDebug | F8/F9の観察切替、物理Worldの再生成 | Info |

TaskDispatcherのJob内部やPhysicsのStep内部には、並行性と性能への影響を避けるため記録を置いていません。

## 制約

- 1件の本文は1023バイトまでで、超えた分はUTF-8の文字境界で切り詰めて`...`を付けます（`FLogRecord::bTruncated`）。
- ファイル出力、非同期キュー、分類ごとの有効化は未実装です。必要になった時点で出力先として追加します。
- 回帰テストは`Tests/LogTests.cpp`（`dxf_tests`に登録）です。
