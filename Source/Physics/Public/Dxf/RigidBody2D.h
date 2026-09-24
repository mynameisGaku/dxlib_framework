// SPDX-License-Identifier: NOASSERTION
#ifndef DXF_PHYSICS_RIGID_BODY_2D_H
#define DXF_PHYSICS_RIGID_BODY_2D_H
#include "Dxf/BodyType.h"
#include "Dxf/WorldSegmentHit2D.h"
#include "Dxf/WorldQueryFilter.h"
#include "Toolbox/Optional.h"
#include "Dxf/PhysicsSnapshot.h"
#include "Dxf/PhysicsExecution.h"
#include "Toolbox/Contact2D.h"
#include "Toolbox/Variant.h"
#include "Toolbox/UniquePtr.h"
#include "Toolbox/Utility.h"
namespace Dxf
{
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
	/**
	 * 連続衝突で移動区間を調べるか。高速なDynamicに指定する。
	 */
	bool bUseContinuous = false;
	/**
	 * 速度が落ちた休止を許可するか。
	 */
	bool bAllowSleep = true;
};
/**
 * 剛体へ取り付ける平面形状と材質。形状の中心は重心からの相対位置。
 */
struct FColliderDescription2D
{
	/**
	 * 取り付ける形状。円または回転矩形。
	 */
	Toolbox::TVariant<Toolbox::FCircle2D, Toolbox::FOrientedBox2D> Shape;
	/**
	 * 有限な非負の摩擦係数。
	 */
	Toolbox::f32 Friction = 0.5f;
	/**
	 * 0〜1の反発係数。
	 */
	Toolbox::f32 Restitution = 0;
	/**
	 * 問い合わせ（線分のRaycastClosest・範囲のOverlapAll）用のカテゴリのビット集合。複数ビットの所属も可。
	 * 0はこのWorldの問い合わせから外す指定。接触・物理更新・Snapshotには影響しない。
	 */
	Toolbox::uint32 QueryCategory = 1u;
};
/**
 * 接触拘束の解決設定。プロジェクトの試験条件に合わせた初期値。
 */
struct FContactSettings2D
{
	/**
	 * 許容する貫通量。メートル単位の有限な非負値。
	 */
	Toolbox::f32 ContactSlop = 0.005f;
	/**
	 * 位置補正の緩和係数。0〜1。
	 */
	Toolbox::f32 BaumgarteBeta = 0.2f;
	/**
	 * 一分割で許す最大補正量。メートル単位の有限な正値。
	 */
	Toolbox::f32 MaxCorrection = 0.05f;
	/**
	 * 反発を適用する衝突前速度。メートル毎秒単位の有限な非負値。
	 */
	Toolbox::f32 RestitutionThreshold = 1.0f;
	/**
	 * 速度拘束の反復数。1〜64。
	 */
	Toolbox::uint32 VelocityIterations = 8;
};
/**
 * 連続衝突の反復設定。
 */
struct FContinuousSettings2D
{
	/**
	 * 移動区間の接触解決を行うか。
	 */
	bool bEnabled = false;
	/**
	 * 一分割の接触解決回数。1〜32。
	 */
	Toolbox::uint32 MaxIterations = 4;
	/**
	 * 進行とみなす最小秒数。有限な非負値。
	 */
	Toolbox::f64 MinAdvanceSeconds = 1e-9;
};
/**
 * 直近更新の連続衝突診断。未処理時間は保守停止で残した秒数。
 */
struct FContinuousDiagnostics2D
{
	/**
	 * 接触走査の実行回数。
	 */
	Toolbox::uint32 ToiIterations = 0;
	/**
	 * 解決した最初接触の回数。
	 */
	Toolbox::uint32 HitsResolved = 0;
	/**
	 * 対象外で離散処理へ回した組数。
	 */
	Toolbox::uint32 FallbackPairs = 0;
	/**
	 * 保守停止で進めなかった秒数。
	 */
	Toolbox::f64 UnprocessedSeconds = 0;
};
/**
 * 休止の条件。プロジェクトの試験条件に合わせた初期値。
 */
struct FSleepSettings2D
{
	/**
	 * 速度低下による休止を行うか。
	 */
	bool bEnabled = true;
	/**
	 * 休止までの接触継続秒数。有限な正値。
	 */
	Toolbox::f32 TimeoutSeconds = 0.5f;
	/**
	 * 休止可能な速度。メートル毎秒単位の有限な非負値。
	 */
	Toolbox::f32 LinearSpeedLimit = 0.05f;
	/**
	 * 休止可能な角速度。ラジアン毎秒単位の有限な非負値。
	 */
	Toolbox::f32 AngularSpeedLimit = 0.05f;
};
/**
 * 2D Worldの値所有Snapshot。形状の世代はCollider IDで識別する。
 */
using FPhysicsSnapshot2D = TPhysicsSnapshot<FBodyId2D, FColliderId2D,
    Toolbox::FVector2, Toolbox::f32, Toolbox::f32, decltype(FColliderDescription2D::Shape)>;
/**
 * 力・重力・Impulseで動く平面剛体を所有し、接触拘束を解く。
 * 単一スレッドで使用し、DxLibや描画を知らない。
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
	 * コライダーを剛体へ取り付ける。不正な値は例外で通知し、状態を変更しない。
	 * @param Body 取り付け先の剛体。
	 * @param Description 取り付ける形状と材質。
	 */
	FColliderId2D AttachCollider(FBodyId2D Body, const FColliderDescription2D& Description);
	/**
	 * コライダーを外してIDを失効させる。期限切れIDはfalseを返す。
	 * @param Id 登録を識別する世代付きID。
	 */
	bool DetachCollider(FColliderId2D Id) noexcept;
	/**
	 * 接触拘束の解決設定を変更する。不正な値は例外で通知する。
	 * @param Settings 接触拘束の解決設定。
	 */
	void SetContactSettings(const FContactSettings2D& Settings);
	/**
	 * 接触拘束の解決設定を返す。
	 */
	FContactSettings2D GetContactSettings() const noexcept;
	/**
	 * 連続衝突の反復設定を変更する。不正な値は例外で通知する。
	 * @param Settings 連続衝突の反復設定。
	 */
	void SetContinuousSettings(const FContinuousSettings2D& Settings);
	/**
	 * 連続衝突の反復設定を返す。
	 */
	FContinuousSettings2D GetContinuousSettings() const noexcept;
	/**
	 * 剛体の連続衝突の利用を切り替える。期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param bEnabled 移動区間の接触解決を行うか。
	 */
	void SetContinuous(FBodyId2D Id, bool bEnabled);
	/**
	 * 剛体が連続衝突を利用するかを調べる。期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 */
	bool IsContinuous(FBodyId2D Id) const;
	/**
	 * 二つのコライダー組の線形CCD対応を調べる。期限切れIDは例外で通知する。
	 * @param A 一つ目のコライダー。
	 * @param B 二つ目のコライダー。
	 */
	EContinuousSupport QueryContinuousSupport(FColliderId2D A, FColliderId2D B) const;
	/**
	 * 直近更新の連続衝突診断を返す。
	 */
	FContinuousDiagnostics2D GetContinuousDiagnostics() const noexcept;
	/**
	 * Step内部で借用するJob Systemと並列化の指定を変更する。
	 * Job SystemはWorldより長く生存させ、Step中の所有権は呼び出し側が保つ。
	 * @param Settings Step中だけ使う並列実行設定。
	 */
	void SetExecutionSettings(const FPhysicsExecutionSettings& Settings) noexcept;
	/**
	 * Step内部で使う並列実行設定を返す。
	 */
	FPhysicsExecutionSettings GetExecutionSettings() const noexcept;
	/**
	 * 直近更新の並列実行診断を返す。
	 */
	FPhysicsExecutionDiagnostics GetExecutionDiagnostics() const noexcept;
	/**
	 * 休止の条件を変更する。不正な値は例外で通知する。
	 * @param Settings 休止の条件。
	 */
	void SetSleepSettings(const FSleepSettings2D& Settings);
	/**
	 * 休止の条件を返す。
	 */
	FSleepSettings2D GetSleepSettings() const noexcept;
	/**
	 * 剛体が休止しているかを調べる。期限切れIDは例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 */
	bool IsSleeping(FBodyId2D Id) const;
	/**
	 * 剛体を起こす。期限切れIDはfalseを返す。
	 * @param Id 登録を識別する世代付きID。
	 */
	bool WakeUp(FBodyId2D Id) noexcept;
	/**
	 * 剛体の姿勢を直接設定する。速度と蓄積力は変更しない。
	 * テレポート後は接触キャッシュの消去と補間履歴の破棄を呼び出し元が行う。
	 * 期限切れIDと非有限値は例外で通知する。
	 * @param Id 登録を識別する世代付きID。
	 * @param Position メートル単位の重心位置。
	 * @param Angle ラジアン単位の姿勢角。
	 */
	void SetBodyTransform(FBodyId2D Id, Toolbox::FVector2 Position, Toolbox::f32 Angle);
	/**
	 * 前回Impulseの再利用記録を消去する。
	 */
	void ClearContactCache() noexcept;
	/**
	 * コライダーIDが有効な登録を指すか調べる。
	 * @param Id 登録を識別する世代付きID。
	 */
	bool IsColliderAlive(FColliderId2D Id) const noexcept;
	/**
	 * 指定秒数だけ物理状態を進める。力とトルクは更新後に一度だけ消去する。
	 * 取り付け済みのコライダー同士の接触拘束も解く。箱同士は最大二点の多様体になる。
	 * 非有限・非正の秒数と範囲外の分割数は例外で通知し、状態を変更しない。
	 * @param DeltaSeconds 有限な正の秒数。
	 * @param SubSteps 1〜1024の分割数。
	 */
	void Step(Toolbox::f64 DeltaSeconds, Toolbox::uint32 SubSteps = 1);
	/**
	 * 生存Body・Colliderを全件複製する。戻り値はWorld破棄後も保持できる。
	 * 明示呼出し時だけ採取し、上限超過・確保失敗は例外で通知する。
	 * Stepや他のWorld操作と並行実行しない。中断したStepの後は正常Step完了まで拒否する。
	 * StepIndexは正常完了したStep呼出し数であり、SubStepsや描画フレーム数ではない。
	 * 形状はローカル座標。現在の公開APIでは同一Collider ID中の形状は不変。
	 * @param Limits 生存Body・Colliderの最大保持件数。
	 */
	FPhysicsSnapshot2D CaptureSnapshot(const FPhysicsSnapshotLimits& Limits = {}) const;
	/**
	 * 現在の円/回転矩形と有限線分の最短交点を返す。非交差は空、異常はFException。
	 * 座標はメートル単位・Y上向きの2D物理ワールド座標（ピクセルではない）。
	 * 同距離はColliderスロット昇順。通常経路は配列確保なし、削除済みを含むスロット数に対しO(n)。
	 * 全ビットのFWorldQueryFilterを指定した4引数版と同じ。QueryCategoryを0にしたColliderは対象にならない。
	 * Step中/途中失敗後は拒否。変更・Stepと外側で直列化する。問い合わせで起床や採取を行わない。
	 * @param Start ワールド始点。有限値を要求する。
	 * @param End ワールド終点。ゼロ長・表現不能な変位は拒否する。
	 * @param ExcludedBody 任意の自己Body。指定済みの無効/別World/旧世代IDは拒否する。
	 */
	Toolbox::TOptional<FWorldSegmentHit2D> RaycastClosest(Toolbox::FVector2 Start, Toolbox::FVector2 End,
	                                                      Toolbox::TOptional<FBodyId2D> ExcludedBody = {}) const;
	/**
	 * RaycastClosestに、調べるColliderの種類の絞り込みを加える。絞り込みは最短候補の選定前に行う。
	 * ColliderのQueryCategoryとFilter.IncludeCategoriesが1ビットも重ならないColliderは、形状の変換・交差計算をしない。
	 * 線分・World状態・除外IDの検証は、マスク0や対象なしの場合も省略しない。その他の契約は3引数版と同じ。
	 * @param Start ワールド始点。有限値を要求する。
	 * @param End ワールド終点。ゼロ長・表現不能な変位は拒否する。
	 * @param ExcludedBody 任意の自己Body。指定済みの無効/別World/旧世代IDは拒否する。除外しない場合は空Optional。
	 * @param Filter 対象にする問い合わせカテゴリ。
	 */
	Toolbox::TOptional<FWorldSegmentHit2D> RaycastClosest(Toolbox::FVector2 Start, Toolbox::FVector2 End,
	                                                      Toolbox::TOptional<FBodyId2D> ExcludedBody,
	                                                      const FWorldQueryFilter& Filter) const;
	/**
	 * Colliderの問い合わせカテゴリを変更する。問い合わせの候補だけに影響し、追加Stepなしで次の問い合わせへ反映する。
	 * ID・世代・形状・姿勢・速度・力・接触キャッシュ・休止・StepIndexは変更しない。
	 * 無効/別World/削除済み/旧世代のID、Step中/途中失敗後はFExceptionで拒否し、値を変更しない。
	 * @param Id 対象Collider。
	 * @param Categories 新しいカテゴリのビット集合。0は問い合わせ対象外。
	 */
	void SetColliderQueryCategory(FColliderId2D Id, Toolbox::uint32 Categories);
	/**
	 * Colliderの問い合わせカテゴリを返す。IDとStep状態の検査はSetColliderQueryCategoryと同じ。
	 * @param Id 対象Collider。
	 */
	Toolbox::uint32 GetColliderQueryCategory(FColliderId2D Id) const;
	/**
	 * 範囲（円）と重なる（接触を含む）現在の全ColliderのIDを返す。範囲の中に重心があるかではなく、形状との重なりで判定する。
	 * 結果は値所有で、生存する対象Colliderのスロット昇順。同じBodyの複数ColliderはそれぞれのIDを返す（Body単位にはまとめない）。
	 * 交点・割合・法線は返さない。非交差は空配列。許容距離は0（範囲を膨らませない）。半径0は点の問い合わせ。
	 * 対象はQueryCategoryとFilterが重なるColliderで、自己Bodyの全Colliderは除外する。対象外の形状は変換・計算しない。
	 * 範囲の不正、明示した除外IDの無効/別World/旧世代、Step中/途中失敗後、対象形状の計算不能、結果の確保失敗はFException。
	 * 失敗時に途中までの結果は返さない。問い合わせでStep・起床・採取・力の消去を行わない。
	 * 走査は削除済みを含むスロット数nに対しO(n)、追加領域は結果件数kに対しO(k)。変更・Stepと外側で直列化する。
	 * @param Area 2D物理ワールド座標の範囲。
	 * @param ExcludedBody 任意の自己Body。除外しない場合は空Optional。
	 * @param Filter 対象にする問い合わせカテゴリ。既定は全ビット。
	 */
	Toolbox::TVector<FColliderId2D> OverlapAll(const Toolbox::FCircle2D& Area,
	                                           Toolbox::TOptional<FBodyId2D> ExcludedBody = {},
	                                           const FWorldQueryFilter& Filter = {}) const;

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
