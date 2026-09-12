// SPDX-License-Identifier: NOASSERTION
#include "TestCases.h"
#include "Toolbox/Contact2D.h"
#include "Toolbox/Contact3D.h"
using namespace Toolbox;
namespace
{
bool Near_Internal(f64 A, f64 B, f64 Absolute)
{
	return Abs(A - B) <= Absolute;
}
void CirclePairSeparated_Internal()
{
	const FCircle2D A{{0, 0}, 1};
	const FCircle2D B{{2.5f, 0}, 1};
	const FContactPoint2D Hit = FindContact(A, B);
	PHYSICS_REQUIRE(Near_Internal(Hit.Separation, 0.5, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Hit.Normal.X, -1, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Hit.Normal.Y, 0, 1e-5));
	PHYSICS_REQUIRE(Hit.FeatureId == 0);
	// 順序を入れ替えると法線だけが反転する。
	const FContactPoint2D Swapped = FindContact(B, A);
	PHYSICS_REQUIRE(Near_Internal(Swapped.Separation, 0.5, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Swapped.Normal.X, 1, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Swapped.Normal.Y, 0, 1e-5));
}
void CirclePairOverlapping_Internal()
{
	const FCircle2D A{{0, 0}, 1};
	const FCircle2D B{{1.5f, 0}, 1};
	const FContactPoint2D Hit = FindContact(A, B);
	PHYSICS_REQUIRE(Near_Internal(Hit.Separation, -0.5, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Hit.Normal.X, -1, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Hit.Normal.Y, 0, 1e-5));
}
void CircleBoxFace_Internal()
{
	const FCircle2D Circle{{0, 3}, 1};
	const FOrientedBox2D Box{{0, 0}, {2, 1}, 0};
	const FContactPoint2D Hit = FindContact(Circle, Box);
	// 箱上面(y=1)から円中心まで2、半径1で分離距離は1。法線は箱から円へ。
	PHYSICS_REQUIRE(Near_Internal(Hit.Separation, 1, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Hit.Normal.X, 0, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Hit.Normal.Y, 1, 1e-5));
	const FContactPoint2D Swapped = FindContact(Box, Circle);
	PHYSICS_REQUIRE(Near_Internal(Swapped.Separation, 1, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Swapped.Normal.Y, -1, 1e-5));
}
void CircleBoxCorner_Internal()
{
	const FCircle2D Circle{{4, 3}, 1};
	const FOrientedBox2D Box{{0, 0}, {2, 1}, 0};
	const FContactPoint2D Hit = FindContact(Circle, Box);
	// 角(2,1)から中心(4,3)への距離はsqrt(8)で、分離距離はsqrt(8)-1。
	PHYSICS_REQUIRE(Near_Internal(Hit.Separation, 1.8284271, 1e-5));
	const f32 Inverse = 1.0f / 1.41421356f;
	PHYSICS_REQUIRE(Near_Internal(Hit.Normal.X, Inverse, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Hit.Normal.Y, Inverse, 1e-5));
}
void CircleBoxPenetrating_Internal()
{
	const FCircle2D Circle{{0, 1.5f}, 1};
	const FOrientedBox2D Box{{0, 0}, {2, 1}, 0};
	const FContactPoint2D Hit = FindContact(Circle, Box);
	// 中心が箱上面から0.5内側なので貫通深度は0.5、法線は円を押し出す上向き。
	PHYSICS_REQUIRE(Near_Internal(Hit.Separation, -0.5, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Hit.Normal.X, 0, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Hit.Normal.Y, 1, 1e-5));
}
void CircleBoxCenterInside_Internal()
{
	const FCircle2D Circle{{1.5f, 0}, 0.5f};
	const FOrientedBox2D Box{{0, 0}, {2, 1}, 0};
	const FContactPoint2D Hit = FindContact(Circle, Box);
	// 中心が内部で+x面まで0.5なので、半径0.5と合わせて深度1で+xへ押し出す。
	PHYSICS_REQUIRE(Near_Internal(Hit.Separation, -1, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Hit.Normal.X, 1, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Hit.Normal.Y, 0, 1e-5));
}
void SpherePairSeparated_Internal()
{
	const FSphere A{{0, 0, 0}, 1};
	const FSphere B{{0, 3.25f, 0}, 1};
	const FContactPoint3D Hit = FindContact(A, B);
	PHYSICS_REQUIRE(Near_Internal(Hit.Separation, 1.25, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Hit.Normal.X, 0, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Hit.Normal.Y, -1, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Hit.Normal.Z, 0, 1e-5));
	const FContactPoint3D Swapped = FindContact(B, A);
	PHYSICS_REQUIRE(Near_Internal(Swapped.Normal.Y, 1, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Swapped.Separation, 1.25, 1e-5));
}
void SphereBoxFace_Internal()
{
	const FSphere Sphere{{0, 0, 4}, 1};
	FOBB Box;
	Box.Center = {0, 0, 0};
	Box.HalfExtents = {2, 2, 1};
	const FContactPoint3D Hit = FindContact(Sphere, Box);
	PHYSICS_REQUIRE(Near_Internal(Hit.Separation, 2, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Hit.Normal.X, 0, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Hit.Normal.Y, 0, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Hit.Normal.Z, 1, 1e-5));
}
void SphereBoxPenetrating_Internal()
{
	const FSphere Sphere{{0, 0, 1.25f}, 1};
	FOBB Box;
	Box.Center = {0, 0, 0};
	Box.HalfExtents = {2, 2, 1};
	const FContactPoint3D Hit = FindContact(Sphere, Box);
	PHYSICS_REQUIRE(Near_Internal(Hit.Separation, -0.75, 1e-5));
	PHYSICS_REQUIRE(Near_Internal(Hit.Normal.Z, 1, 1e-5));
}
void SphereRotatedBox_Internal()
{
	// Z軸回りに90度回転した箱はXとYの半辺長が入れ替わったのと等しい。
	FOBB Box;
	Box.Center = {0, 0, 0};
	Box.HalfExtents = {2, 1, 1};
	Box.Axes[0] = {0, 1, 0};
	Box.Axes[1] = {-1, 0, 0};
	Box.Axes[2] = {0, 0, 1};
	const FSphere Sphere{{0, 3, 0}, 1};
	const FContactPoint3D Hit = FindContact(Sphere, Box);
	// 回転後の+Y方向の半辺長は2なので、中心距離3から半径1と半辺長2を引いて0。
	PHYSICS_REQUIRE(Near_Internal(Hit.Separation, 0, 1e-4));
	PHYSICS_REQUIRE(Near_Internal(Hit.Normal.Y, 1, 1e-4));
}
void InvalidContactInputsThrow_Internal()
{
	const FCircle2D Broken{{TNumericLimits<f32>::QuietNaN(), 0}, 1};
	const FCircle2D Whole{{0, 0}, 1};
	bool bThrown = false;
	try
	{
		(void)FindContact(Broken, Whole);
	}
	catch (const FException&)
	{
		bThrown = true;
	}
	PHYSICS_REQUIRE(bThrown);
	const FOrientedBox2D BrokenBox{{0, 0}, {-1, 1}, 0};
	bThrown = false;
	try
	{
		(void)FindContact(Whole, BrokenBox);
	}
	catch (const FException&)
	{
		bThrown = true;
	}
	PHYSICS_REQUIRE(bThrown);
}
const PhysicsTest::FCase ContactCases_Internal[] = {
    {"circle pair reports separation and normal", &CirclePairSeparated_Internal},
    {"circle pair reports negative penetration", &CirclePairOverlapping_Internal},
    {"circle and box face contact", &CircleBoxFace_Internal},
    {"circle and box corner contact", &CircleBoxCorner_Internal},
    {"circle penetrating box face", &CircleBoxPenetrating_Internal},
    {"circle center inside box", &CircleBoxCenterInside_Internal},
    {"sphere pair reports separation and normal", &SpherePairSeparated_Internal},
    {"sphere and box face contact", &SphereBoxFace_Internal},
    {"sphere penetrating box face", &SphereBoxPenetrating_Internal},
    {"sphere and rotated box contact", &SphereRotatedBox_Internal},
    {"invalid contact inputs throw", &InvalidContactInputsThrow_Internal},
};
} // namespace
const PhysicsTest::FCase* PhysicsTest::GetContactCases(Toolbox::size_t& Count) noexcept
{
	Count = sizeof(ContactCases_Internal) / sizeof(ContactCases_Internal[0]);
	return ContactCases_Internal;
}
