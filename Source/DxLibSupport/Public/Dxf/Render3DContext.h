// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_RENDER_3D_CONTEXT_H
#define DXF_RENDER_3D_CONTEXT_H
#include "Dxf/RenderAccess.h"
#include "Dxf/Model.h"
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
	 * 現在のView（次の命令が属する区間のカメラ・表示設定）。
	 */
	FORCEINLINE const FRenderView3D& GetView() const noexcept
	{
		return m_View;
	}
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
	 * 読み込んだモデルを1体描画する。変換と再生状態は受付時点の値を複写する。
	 * 記録した命令が実行または破棄されるまでインスタンスのネイティブモデルを生存させる。
	 * 不透明として扱い、同じビューの形状より先に描画する（透明の並べ替えの対象外）。
	 * @param Instance 描画するインスタンス。
	 */
	TResult<void> DrawModel(const FModelInstance& Instance);
	/**
	 * テクスチャを貼った四角形を現在のViewで描く（同じViewの形状の後に描く）。
	 * @param Quad 四角形（有限の四隅・有効なテクスチャ）。
	 */
	TResult<void> DrawTexturedQuad(const FTexturedQuad3D& Quad);
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
		if (!FitsTarget_Internal(m_View))
		{
			return InvalidGeometry_Internal();
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
	/**
	 * 未実行の3D命令があるか。所有スレッドから、実行前の判定にだけ使う。
	 */
	FORCEINLINE bool HasCommands_Internal() const noexcept
	{
		return !m_Commands.IsEmpty() || !m_Models.IsEmpty() || !m_Quads.IsEmpty();
	}
private:
	/**
	 * @param View 既知の現在描画先へ収まるかを確認するビュー。
	 */
	bool FitsTarget_Internal(const FRenderView3D& View) const noexcept;
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
	 * 一回のFlushまでに保持するモデル描画の上限。
	 */
	static constexpr Toolbox::size_t MaxFrameModels = 4096;
	/**
	 * モデル描画命令の受付時点の値。
	 */
	struct FRecordedModel
	{
		/**
		 * 描画するインスタンス。実行または破棄まで生存させる。
		 */
		Toolbox::TSharedPtr<FModelInstanceResource> Instance;
		/**
		 * 受付時点の変換と再生状態。
		 */
		FModelDraw3D Draw;
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
	/**
	 * 未実行のモデル描画命令。
	 */
	Toolbox::TVector<FRecordedModel> m_Models;
	/**
	 * 記録した四角形とView。
	 */
	struct FRecordedQuad
	{
		/**
		 * 四角形。
		 */
		FTexturedQuad3D Quad;
		/**
		 * View。
		 */
		FRenderView3D View;
		/**
		 * Viewの区間番号。
		 */
		Toolbox::uint64 Serial = 0;
	};
	/**
	 * 記録した四角形。
	 */
	Toolbox::TVector<FRecordedQuad> m_Quads;
};
}
#endif
