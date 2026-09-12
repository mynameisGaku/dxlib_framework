#pragma once
#include "Toolbox/UniquePtr.h"
#include "Dxf/Application.h"
#include "Toolbox/Platform.h"
namespace Dxf
{
/**
 * 終了要求までフレーム処理を繰り返す実行器を管理する型。
 */
class FAppRunner
{
public:
	/**
	 * 終了要求までアプリケーションを実行する。
	 * @param Application アプリケーションの実行状態。
	 * @param InitialScene 最初に開始するシーン。
	 */
	TResult<void> Run(FApplication& Application, Toolbox::TUniquePtr<DScene> InitialScene)
	{
		/**
		 * 開始処理が完了しているか。
		 */
		auto Started = Application.Start(Toolbox::Move(InitialScene));
		if (!Started)
		{
			return Started;
		}
		/**
		 * 元の状態。
		 */
		const auto Origin = Toolbox::MonotonicNanoseconds();
		while (Application.IsRunning())
		{
			/**
			 * 計測した現在時刻。
			 */
			const Toolbox::f64 Now = static_cast<Toolbox::f64>(Toolbox::MonotonicNanoseconds() - Origin) / 1000000000.0;
			/**
			 * 1フレームの実行結果。
			 */
			auto Step = Application.Step(Now);
			if (!Step)
			{
				return TResult<void>::Failure(Step.Error());
			}
			if (!Step.Value())
			{
				break;
			}
		}
		Application.Shutdown();
		return {};
	}
};
} // namespace Dxf
