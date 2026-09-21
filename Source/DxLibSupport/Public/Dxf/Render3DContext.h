// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_RENDER_3D_CONTEXT_H
#define DXF_RENDER_3D_CONTEXT_H
#include "Dxf/RenderAccess.h"
namespace Dxf
{
/**
 * ワールド座標の3D描画を記録する。各ビュー設定は命令受付時に複写する。
 * 命令は所有スレッドで確定し、Jobへ渡すのは独立した出力領域だけとする。
 */
class FRender3DContext
{
public:
	/**
	 * @param Access フレーム共通の受付・再入状態。
	 */
	explicit FRender3DContext(FRenderAccess& Access) : m_pAccess(&Access)
	{
	}
	/**
	 * 状態を複製しない。
	 */
	FRender3DContext(const FRender3DContext&) = delete;
	FRender3DContext& operator=(const FRender3DContext&) = delete;
	/**
	 * @param View 次の命令群のカメラ・表示設定。既存命令は変更しない。
	 */
	TResult<void> SetView(const FRenderView3D& View);
	/**
	 * @param Command CPU上の独立した形状と設定。
	 */
	TResult<void> Submit(FGeometryCommand3D Command);
	/**
	 * @param Start 始点。
	 * @param End 終点。
	 * @param Options 色・深度。
	 */
	TResult<void> DrawLine(Toolbox::FVector3 Start, Toolbox::FVector3 End, const FDrawStyle3D& Options = {});
	/**
	 * @param A 一番目の頂点。
	 * @param B 二番目の頂点。
	 * @param C 三番目の頂点。
	 * @param Options 色・深度。
	 */
	TResult<void> DrawTriangle(Toolbox::FVector3 A, Toolbox::FVector3 B, Toolbox::FVector3 C, const FDrawStyle3D& Options = {});
	/**
	 * @param Box 箱の位置・大きさ・直交軸。
	 * @param Options 色・深度。
	 */
	TResult<void> DrawBox(const Toolbox::FOBB& Box, const FDrawStyle3D& Options = {});
	/**
	 * @param Sphere 中心・半径。
	 * @param Options 色・深度。
	 * @param Segments 分割数。
	 */
	TResult<void> DrawSphere(const Toolbox::FSphere& Sphere, const FDrawStyle3D& Options = {}, Toolbox::uint32 Segments = 16);
	/**
	 * @param Geometry ワールド座標の面・線群。複写して所有する。
	 * @param Options 色・深度。
	 */
	TResult<void> DrawMesh(const FGeometry3D& Geometry, const FDrawStyle3D& Options = {});
	/**
	 * 設定済みの共有JobSystemで生成し、入力順に一括確定する。
	 * @param Count 入力件数。 @param Generate 専用FGeometryCommand3Dへの生成処理。 @param MinimumBatch 最小分割数。
	 */
	template <typename F> TResult<void> SubmitGenerated(Toolbox::size_t Count, F&& Generate, Toolbox::size_t MinimumBatch = 16)
	{
		if (!m_pAccess->IsAccepting() || m_pAccess->m_pJobs == nullptr)
		{
			return StateError_Internal();
		}
		if (Count > 65536 || Count > 65536 - m_Commands.Size())
		{
			return LimitError_Internal();
		}
		TGuardValue Busy(m_pAccess->m_bBusy, true);
		Toolbox::TVector<FGeometryCommand3D> Slots(Count);
		Toolbox::TVector<TResult<void>> Results(Count);
		if (!Toolbox::ParallelFor(*m_pAccess->m_pJobs, Count, [&](Toolbox::size_t Index)
		{
			Results[Index] = Generate(Index, Slots[Index]);
		}, MinimumBatch))
		{
			return TResult<void>::Failure(EErrorCode::UserException, "3D generation failed");
		}
		Toolbox::size_t AddedPrimitives = 0;
		for (Toolbox::size_t Index = 0; Index < Count; ++Index)
		{
			if (!Results[Index])
			{
				return Results[Index];
			}
			if (!IsValidGeometry3D(Slots[Index]))
			{
				return InvalidGeometry_Internal();
			}
			const Toolbox::size_t Primitives = Slots[Index].Geometry.Lines.Size() + Slots[Index].Geometry.Triangles.Size();
			if (Primitives > MaxFramePrimitives - m_PrimitiveCount - AddedPrimitives)
			{
				return LimitError_Internal();
			}
			AddedPrimitives += Primitives;
		}
		static_assert(noexcept(FRecordedCommand(Toolbox::Move(m_Commands[0]))));
		m_Commands.Reserve(m_Commands.Size() + Count);
		for (auto& Slot : Slots)
		{
			m_Commands.PushBack({Toolbox::Move(Slot), m_View, m_ViewSerial});
		}
		m_PrimitiveCount += AddedPrimitives;
		return {};
	}
	/**
	 * @param Backend 3Dを実行する描画先。各ビューの終了は失敗時にも呼ぶ。
	 */
	TResult<void> Execute_Internal(IRenderBackend& Backend);
	/**
	 * フレーム中断・終了で、未実行の命令を解放する。
	 */
	void Clear_Internal() noexcept;
private:
	/**
	 * 一回のFlushまでに保持する線・三角形の合計上限。
	 */
	static constexpr Toolbox::size_t MaxFramePrimitives = 65536;
	/**
	 * 未実行形状の合計数。
	 */
	Toolbox::size_t m_PrimitiveCount = 0;
	/**
	 * 一命令の受付時点の値。
	 */
	struct FRecordedCommand
	{
		FGeometryCommand3D Command;
		FRenderView3D View;
		Toolbox::uint64 Serial = 0;
	};
	/**
	 * 不正な利用状態。
	 */
	static TResult<void> StateError_Internal();
	/**
	 * フレーム当たりの命令予算超過。
	 */
	static TResult<void> LimitError_Internal();
	/**
	 * 不正な形状・設定。
	 */
	static TResult<void> InvalidGeometry_Internal();
	/**
	 * 所有スレッド・再入境界。
	 */
	FRenderAccess* m_pAccess;
	/**
	 * 次の命令へ複写するビュー。
	 */
	FRenderView3D m_View;
	/**
	 * SetViewを呼ぶたびに増えるパス境界番号。
	 */
	Toolbox::uint64 m_ViewSerial = 0;
	/**
	 * 未実行命令。
	 */
	Toolbox::TVector<FRecordedCommand> m_Commands;
};
}
#endif
