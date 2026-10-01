// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_TEST_CONTENT_RENDER_BACKEND_H
#define DXF_TEST_CONTENT_RENDER_BACKEND_H
#include "Support/FakeBackend.h"
namespace Dxf::Testing
{
/**
 * Contentが送った位置と色を記録する。実GPUの画素検査とは別の描画境界。
 */
class FContentRenderBackend final : public IRenderBackend
{
public:
	/**
	 * 資源と画面操作の記録先を借りる。
	 */
	explicit FContentRenderBackend(FFakeBackend& Backend) : m_pBackend(&Backend)
	{
	}
	/**
	 * 実行した三角形の値。描画受付時の値を検査する。
	 */
	Toolbox::TVector<FTriangleCommand2D> Triangles;
	/**
	 * 実行した円の値。
	 */
	Toolbox::TVector<FCircleCommand2D> Circles;
	/**
	 * 実行した画像の値。
	 */
	Toolbox::TVector<FSpriteCommand> Sprites;
	/**
	 * 実行した3D領域の値。
	 */
	Toolbox::TVector<FRenderView3D> Views;
	/**
	 * 実行した3D形状の値。
	 */
	Toolbox::TVector<FPreparedGeometry3D> Geometry;
	/**
	 * モデル描画で採用された再生時刻。実GPU検査ではない。
	 */
	Toolbox::TVector<Toolbox::f32> ModelTimes;
	/**
	 * 個体数の確認に使うNative境界の番号。
	 */
	Toolbox::TVector<Toolbox::int32> ModelHandles;
	bool SupportsModels3D() const noexcept override
	{
		return true;
	}
	TResult<void> DrawModel3D(const FModelDraw3D& Command) override
	{
		ModelTimes.PushBack(Command.NativeTime);
		ModelHandles.PushBack(Command.pInstance->GetHandle_Internal());
		return DrawResult();
	}
	TResult<void> SetTarget(Toolbox::int32 Handle, Toolbox::int32 Width, Toolbox::int32 Height) override
	{
		return m_pBackend->SetTarget(Handle, Width, Height);
	}
	TResult<void> Clear(FColor Color) override
	{
		return m_pBackend->Clear(Color);
	}
	TResult<void> ResetState(Toolbox::int32 Width, Toolbox::int32 Height) override
	{
		return m_pBackend->ResetState(Width, Height);
	}
	TResult<void> DrawSprite(const FSpriteCommand& Command) override
	{
		Sprites.PushBack(Command);
		return m_pBackend->DrawSprite(Command);
	}
	TResult<void> DrawText(const FTextCommand& Command) override
	{
		return m_pBackend->DrawText(Command);
	}
	TResult<void> DrawRectangle(const FRectangleCommand& Command) override
	{
		return m_pBackend->DrawRectangle(Command);
	}
	bool SupportsShapes2D() const noexcept override
	{
		return true;
	}
	bool SupportsClip2D() const noexcept override
	{
		return true;
	}
	bool SupportsGeometry3D() const noexcept override
	{
		return true;
	}
	bool SupportsViewports3D() const noexcept override
	{
		return true;
	}
	TResult<void> SetClip2D(bool Enabled, FIntRect Rect) override
	{
		return m_pBackend->SetClip2D(Enabled, Rect);
	}
	TResult<void> DrawTriangle2D(const FTriangleCommand2D& Command) override
	{
		Triangles.PushBack(Command);
		return DrawResult();
	}
	TResult<void> DrawCircle2D(const FCircleCommand2D& Command) override
	{
		Circles.PushBack(Command);
		return DrawResult();
	}
	TResult<void> BeginView3D(const FRenderView3D& View) override
	{
		Views.PushBack(View);
		return DrawResult();
	}
	TResult<void> DrawGeometry3D(const FPreparedGeometry3D& Packet) override
	{
		Geometry.PushBack(Packet);
		return DrawResult();
	}
	TResult<void> Present() override
	{
		return m_pBackend->Present();
	}

private:
	// 描画境界へ到達した後の故障だけを返す。
	TResult<void> DrawResult() const
	{
		return m_pBackend->GetTrace().bFailDraw
		           ? TResult<void>::Failure(EErrorCode::BackendFailure, "Content test draw failed")
		           : TResult<void>::Success();
	}
	// 別所有の資源・画面記録先。
	FFakeBackend* m_pBackend;
};
} // namespace Dxf::Testing
#endif
