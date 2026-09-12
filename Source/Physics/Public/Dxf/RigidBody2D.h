// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_RIGID_BODY_2D_H
#define DXF_PHYSICS_RIGID_BODY_2D_H
#include "Dxf/BodyType.h"
#include "Toolbox/Vector2.h"
#include "Toolbox/UniquePtr.h"
#include "Toolbox/Utility.h"
namespace Dxf
{
/**
 * 平面剛体を識別する、世代付きの非所有ハンドル。
 */
struct FBodyId2D
{
	/**
	 * 登録先ワールドの識別子。
	 */
	Toolbox::uint64 World = 0;
	/**
	 * 登録スロットの番号。
	 */
	Toolbox::size_t Index = 0;
	/**
	 * 同じスロットを再使用した際の世代。
	 */
	Toolbox::uint64 Generation = 0;
	/**
	 * 同じ登録を指すか調べる。
	 */
	bool operator==(const FBodyId2D&) const = default;
};
/**
 * 平面剛体の初期条件。位置は重心を基準とするメートル単位。
 */
struct FBodyDescription2D
{
	/**
	 * 運動区分。
	 */
	EBodyType Type = EBodyType::Dynamic;
	/**
	 * 重心の初期位置。単位はメートルでY軸が上向き。
	 */
	Toolbox::FVector2 Position;
	/**
	 * 初期姿勢。ラジアン単位で反時計回りが正。
	 */
	Toolbox::f32 Angle = 0;
	/**
	 * 重心の初期速度。単位はメートル毎秒。
	 */
	Toolbox::FVector2 Velocity;
	/**
	 * 初期角速度。ラジアン毎秒で反時計回りが正。
	 */
	Toolbox::f32 AngularVelocity = 0;
	/**
	 * Dynamicだけが使う質量。キログラム単位の有限な正値。
	 */
	Toolbox::f32 Mass = 1;
	/**
	 * Dynamicだけが使う重心回りの慣性。キログラム平方メートル単位の有限な正値。
	 */
	Toolbox::f32 Inertia = 1;
	/**
	 * 速度の減衰率。1毎秒単位の有限な非負値。
	 */
	Toolbox::f32 LinearDamping = 0;
	/**
	 * 角速度の減衰率。1毎秒単位の有限な非負値。
	 */
	Toolbox::f32 AngularDamping = 0;
	/**
	 * ワールド重力への追従倍率。無次元の有限値。
	 */
	Toolbox::f32 GravityScale = 1;
};
/**
 * 力・重力・Impulseで動く平面剛体を所有する。半陰的Eulerで更新する。
 * 単一スレッドで使用し、DxLibや描画を知らない。接触応答は行わない。
 */
class FPhysicsWorld2D
{
public:
	/**
	 * 空のワールドを作る。
	 */
	FPhysicsWorld2D();
	/**
	 * 登録剛体を破棄する。
	 */
	~FPhysicsWorld2D();
	/**
	 * 登録IDの混同を避けるためコピーを禁止する。
	 */
	FPhysicsWorld2D(const FPhysicsWorld2D&) = delete;
	/**
	 * 登録IDの混同を避けるためコピー代入を禁止する。
	 */
	FPhysicsWorld2D& operator=(const FPhysicsWorld2D&) = delete;
	/**
	 * 剛体を所有して登録する。不正な値は例外で通知し、状態を変更しない。
	 * @param Description 初期条件。
	 */
	FBodyId2D CreateBody(const FBodyDescription2D& Description);
	/**
	 * 登録を削除し、そのIDを失効させる。期限切れIDはfalseを返す。
	 * @param Id 登録を識別する世代付きID。
	 */
	bool DestroyBody(FBodyId2D Id) noexcept;
	/**
	 * IDが有効な登録を指すか調べる。
	 * @param Id 登録を識別する世代付きID。
	 */
	bool IsAlive(FBodyId2D Id) const noexcept;
	/**
	 * 重心位置を返す。期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 */
	Toolbox::FVector2 GetPosition(FBodyId2D Id) const;
	/**
	 * 姿勢角をラジアンで返す。期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 */
	Toolbox::f32 GetAngle(FBodyId2D Id) const;
	/**
	 * 重心速度を返す。期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 */
	Toolbox::FVector2 GetVelocity(FBodyId2D Id) const;
	/**
	 * 角速度を返す。期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 */
	Toolbox::f32 GetAngularVelocity(FBodyId2D Id) const;
	/**
	 * 重心回りの角運動量を返す。非Dynamicは追跡しないためゼロ。
	 * 期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 */
	Toolbox::f32 GetAngularMomentum(FBodyId2D Id) const;
	/**
	 * 速度を指定する。Staticへの指定と期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param Velocity 毎秒メートル単位の速度。
	 */
	void SetVelocity(FBodyId2D Id, Toolbox::FVector2 Velocity);
	/**
	 * 角速度を指定する。Staticへの指定と期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param AngularVelocity 毎秒ラジアン単位の角速度。
	 */
	void SetAngularVelocity(FBodyId2D Id, Toolbox::f32 AngularVelocity);
	/**
	 * 次の更新で使う力を加算する。Dynamic以外と期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param Force ニュートン単位の力。
	 */
	void ApplyForce(FBodyId2D Id, Toolbox::FVector2 Force);
	/**
	 * 次の更新で使うトルクを加算する。Dynamic以外と期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param Torque ニュートンメートル単位のトルク。反時計回りが正。
	 */
	void ApplyTorque(FBodyId2D Id, Toolbox::f32 Torque);
	/**
	 * 速度へ即時反映する力積を与える。Dynamic以外と期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param Impulse ニュートン秒単位の力積。
	 */
	void ApplyLinearImpulse(FBodyId2D Id, Toolbox::FVector2 Impulse);
	/**
	 * 角速度へ即時反映する力積モーメントを与える。Dynamic以外と期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param Impulse ニュートンメートル秒単位の力積モーメント。反時計回りが正。
	 */
	void ApplyAngularImpulse(FBodyId2D Id, Toolbox::f32 Impulse);
	/**
	 * 重心外の点へ力積を与え、並進と回転の両方を生む。Dynamic以外と期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param Impulse ニュートン秒単位の力積。
	 * @param WorldPoint 力積を与えるワールド位置。メートル単位。
	 */
	void ApplyImpulseAtPoint(FBodyId2D Id, Toolbox::FVector2 Impulse, Toolbox::FVector2 WorldPoint);
	/**
	 * ワールド重力を設定する。非有限値は例外で通知する。
	 * @param Gravity メートル毎秒毎秒単位の重力加速度。
	 */
	void SetGravity(Toolbox::FVector2 Gravity);
	/**
	 * ワールド重力を返す。
	 */
	Toolbox::FVector2 GetGravity() const noexcept;
	/**
	 * 指定秒数だけ物理状態を進める。力とトルクは更新後に一度だけ消去する。
	 * 非有限・非正の秒数と範囲外の分割数は例外で通知し、状態を変更しない。
	 * @param DeltaSeconds 有限な正の秒数。
	 * @param SubSteps 1〜1024の分割数。
	 */
	void Step(Toolbox::f64 DeltaSeconds, Toolbox::uint32 SubSteps = 1);

private:
	/**
	 * 実装と登録データの所有領域。
	 */
	struct FImpl;
	/**
	 * 公開ヘッダーから隠した登録データ。
	 */
	Toolbox::TUniquePtr<FImpl> m_pImpl;
};
} // namespace Dxf
#endif
