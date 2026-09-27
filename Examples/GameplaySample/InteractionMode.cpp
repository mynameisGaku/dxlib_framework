// SPDX-License-Identifier: NOASSERTION
#include "InteractionMode.h"
#include "SampleHud.h"
namespace Dxf::GameplaySample
{
void InteractionModeText(const FInteractionMode& Mode, char (&Out)[256]) noexcept
{
	snprintf(Out, sizeof(Out), "shape %s  push %s  [C] crouch (capsule)  [F1] change",
	         Mode.IsCapsule() ? "Capsule" : "Round", Mode.IsPush() ? "On" : "Off");
}
} // namespace Dxf::GameplaySample
