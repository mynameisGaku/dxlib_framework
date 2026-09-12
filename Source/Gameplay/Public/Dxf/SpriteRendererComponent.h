#pragma once
#include "Dxf/GameObjectComponent.h"
#include "Dxf/RenderContext.h"
#include "Toolbox/Utility.h"

namespace Dxf
{
/**
 * 独立した位置を持つ任意の2Dスプライト。他のオブジェクトに座標変換を要求しない。
 */
class DSpriteRendererComponent : public DGameObjectComponent
{
public:
	/**
	 * 必要な依存関係を受け取り、初期状態を構築する。
	 * @param Texture 描画するテクスチャ。
	 * @param Position 描画位置。
	 * @param Options 処理に適用する設定。
	 */
	explicit DSpriteRendererComponent(FTexture Texture = {}, FVector2 Position = {}, FSpriteDrawOptions Options = {})
	    : m_Texture(Toolbox::Move(Texture)), m_Position(Position), m_Options(Options)
	{
	}
	/**
	 * 描画するテクスチャを設定する。
	 * @param Texture 描画するテクスチャ。
	 */
	void SetTexture(FTexture Texture)
	{
		m_Texture = Toolbox::Move(Texture);
	}
	/**
	 * 描画するテクスチャを取得する。
	 */
	FORCEINLINE const FTexture& GetTexture() const noexcept
	{
		return m_Texture;
	}
	/**
	 * 描画位置を取得する。
	 */
	FORCEINLINE FVector2& GetPosition() noexcept
	{
		return m_Position;
	}
	/**
	 * 描画位置を取得する。
	 */
	FORCEINLINE const FVector2& GetPosition() const noexcept
	{
		return m_Position;
	}
	/**
	 * 処理に適用する設定を取得する。
	 */
	FORCEINLINE FSpriteDrawOptions& GetOptions() noexcept
	{
		return m_Options;
	}
	/**
	 * 処理に適用する設定を取得する。
	 */
	FORCEINLINE const FSpriteDrawOptions& GetOptions() const noexcept
	{
		return m_Options;
	}

protected:
	/**
	 * 現在の状態を描画する。
	 * @param Render 現在の描画コンテキスト。
	 */
	void OnDraw(FRenderContext& Render) const override
	{
		if (!m_Texture.IsValid())
		{
			return;
		}
		// 処理結果。
		auto Result = Render.Draw(m_Texture, m_Position, m_Options);
		if (!Result)
		{
			// ライフサイクルの入口で例外をTResultへ変換する。
			throw Toolbox::FException(Result.Error().Message);
		}
	}

private:
	/**
	 * 描画するテクスチャ。
	 */
	FTexture m_Texture;
	/**
	 * 描画位置。
	 */
	FVector2 m_Position;
	/**
	 * 処理に適用する設定。
	 */
	FSpriteDrawOptions m_Options;
};
} // namespace Dxf
