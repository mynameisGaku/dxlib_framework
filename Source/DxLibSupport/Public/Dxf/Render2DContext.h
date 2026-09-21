// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_RENDER_2D_CONTEXT_H
#define DXF_RENDER_2D_CONTEXT_H
#include "Dxf/RenderAccess.h"
namespace Dxf
{
/**
 * 画面座標の2D描画だけを公開する借用窓口。コピー・保存はしない。
 */
class FRender2DContext
{
public:
	/**
	 * @param Queue 命令の所有先。
	 * @param Access 共通の所有・再入境界。
	 */
	FRender2DContext(FRenderQueue2D& Queue, FRenderAccess& Access) : m_pQueue(&Queue), m_pAccess(&Access)
	{
	}
	/**
	 * 参照元と実行中状態を複製しない。
	 */
	FRender2DContext(const FRender2DContext&) = delete;
	FRender2DContext& operator=(const FRender2DContext&) = delete;
	/**
	 * @param Command 確定済みの2D描画命令。
	 */
	TResult<void> Submit(FRenderCommand Command)
	{
		if (!m_pAccess->IsAccepting())
		{
			return StateError_Internal();
		}
		return m_pQueue->Submit(Toolbox::Move(Command));
	}
	/**
	 * @param Texture 描画資源。
	 * @param Position 画面位置。
	 * @param Options 表示設定。
	 */
	TResult<void> DrawSprite(FTexture Texture, FVector2 Position, const FSpriteDrawOptions& Options = {})
	{
		return Submit(FSpriteCommand{Toolbox::Move(Texture), Position, Options});
	}
	/**
	 * @param Font フォント。
	 * @param Text UTF-8文字列。
	 * @param Position 画面位置。
	 * @param Options 表示設定。
	 */
	TResult<void> DrawText(FFont Font, Toolbox::FString Text, FVector2 Position, const FDrawStyle& Options = {})
	{
		return Submit(FTextCommand{Toolbox::Move(Font), Toolbox::Move(Text), Position, Options});
	}
	/**
	 * @param Rectangle 塗りつぶす矩形。
	 * @param Options 表示設定。
	 */
	TResult<void> FillRectangle(FIntRect Rectangle, const FDrawStyle& Options = {})
	{
		return Submit(FRectangleCommand{Rectangle, Options});
	}
	/**
	 * @param Rectangle 外周を描く矩形。
	 * @param Options 表示設定。
	 */
	TResult<void> DrawRectangle(FIntRect Rectangle, const FDrawStyle& Options = {});
	/**
	 * @param Start 始点。
	 * @param End 終点。
	 * @param Options 表示設定。
	 */
	TResult<void> DrawLine(FVector2 Start, FVector2 End, const FDrawStyle& Options = {})
	{
		return Submit(FLineCommand2D{Start, End, Options});
	}
	/**
	 * @param Center 中心。
	 * @param Radius 半径。
	 * @param Options 表示設定。
	 */
	TResult<void> DrawCircle(FVector2 Center, Toolbox::f32 Radius, const FDrawStyle& Options = {})
	{
		return Submit(FCircleCommand2D{Center, Radius, false, Options});
	}
	/**
	 * @param Center 中心。
	 * @param Radius 半径。
	 * @param Options 表示設定。
	 */
	TResult<void> FillCircle(FVector2 Center, Toolbox::f32 Radius, const FDrawStyle& Options = {})
	{
		return Submit(FCircleCommand2D{Center, Radius, true, Options});
	}
	/**
	 * @param A 一番目の頂点。
	 * @param B 二番目の頂点。
	 * @param C 三番目の頂点。
	 * @param Options 表示設定。
	 */
	TResult<void> DrawTriangle(FVector2 A, FVector2 B, FVector2 C, const FDrawStyle& Options = {})
	{
		return Submit(FTriangleCommand2D{A, B, C, false, Options});
	}
	/**
	 * @param A 一番目の頂点。
	 * @param B 二番目の頂点。
	 * @param C 三番目の頂点。
	 * @param Options 表示設定。
	 */
	TResult<void> FillTriangle(FVector2 A, FVector2 B, FVector2 C, const FDrawStyle& Options = {})
	{
		return Submit(FTriangleCommand2D{A, B, C, true, Options});
	}
	/**
	 * 設定済みの共有JobSystemを利用する。未設定は明示的に失敗する。
	 * @param Count 命令数。 @param Generate 専用命令領域への生成処理。 @param MinimumBatch 最小分割数。
	 */
	template <typename F> TResult<void> SubmitGenerated(Toolbox::size_t Count, F&& Generate, Toolbox::size_t MinimumBatch = 16)
	{
		if (!m_pAccess->IsAccepting() || m_pAccess->m_pJobs == nullptr)
		{
			return StateError_Internal();
		}
		TGuardValue Busy(m_pAccess->m_bBusy, true);
		return m_pQueue->SubmitGenerated(*m_pAccess->m_pJobs, Count, Toolbox::Forward<F>(Generate), MinimumBatch);
	}
private:
	/**
	 * 不正な実行状態を結果へ変換する。
	 */
	static TResult<void> StateError_Internal()
	{
		return TResult<void>::Failure(EErrorCode::InvalidState, "2D context unavailable or reentrant");
	}
	/**
	 * 命令の所有先。
	 */
	FRenderQueue2D* m_pQueue;
	/**
	 * 共通境界。
	 */
	FRenderAccess* m_pAccess;
};
}
#endif
