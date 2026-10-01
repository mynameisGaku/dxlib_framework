// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_TEST_CONTENT_MODEL_BACKEND_H
#define DXF_TEST_CONTENT_MODEL_BACKEND_H
#include "Dxf/ModelBackend.h"
namespace Dxf::Testing
{
/**
 * 実ufbx取込後のNative作成・個体・解放を記録する。実DxLibとは別の境界。
 */
class FContentModelBackend final : public IModelBackend
{
public:
	/**
	 * 共有モデル作成の回数。
	 */
	Toolbox::uint32 Loads = 0;
	/**
	 * 個体作成の回数。
	 */
	Toolbox::uint32 Instances = 0;
	/**
	 * 終了の回数。終了で確保しない。
	 */
	Toolbox::uint32 Deletes = 0;
	/**
	 * 生存するNative境界の番号数。
	 */
	Toolbox::uint32 Live = 0;
	/**
	 * @param Model 実取込済みのクリップ情報。
	 * @param Directory 読込元。CPU試験では画像を作らない。
	 */
	TResult<FModelAllocation> LoadModel(const FImportedModel& Model, const Toolbox::FString&) override
	{
		FModelAllocation Allocation;
		Allocation.NativeHandle = m_Next++;
		for (const auto& Clip : Model.Clips)
		{
			Allocation.NativeClipDurations.PushBack(Clip.DurationSeconds * 30);
		}
		++Loads;
		++Live;
		return TResult<FModelAllocation>::Success(Toolbox::Move(Allocation));
	}
	/**
	 * 個体ごとに独立した番号を返す。
	 */
	TResult<Toolbox::int32> DuplicateModel(Toolbox::int32) override
	{
		++Instances;
		++Live;
		return TResult<Toolbox::int32>::Success(m_Next++);
	}
	/**
	 * 既存Registryの所有側終了を数える。
	 */
	void DeleteModel(Toolbox::int32) noexcept override
	{
		++Deletes;
		--Live;
	}

private:
	/**
	 * 疑似番号。実際のDxLib番号ではない。
	 */
	Toolbox::int32 m_Next = 10000;
};
} // namespace Dxf::Testing
#endif
