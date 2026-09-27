// SPDX-License-Identifier: NOASSERTION
#include "InteractionHud.h"
#include "InteractionLayout.h"
namespace Dxf::GameplaySample
{
void InteractionRulesText(const FInteractionRules& Rules, char (&Text)[256])
{
	snprintf(Text, sizeof(Text),
	         "collected %d/%d  door %s (plate %d)  crate %s (begin %d, stay %d, end %d)  checkpoint %d  respawns %d",
	         Rules.GetCollected(), InteractionLayout::PickupCount, Rules.IsDoorOpen() ? "open" : "closed",
	         Rules.GetPlateOccupants(), Rules.IsTouchingCrate() ? "touching" : "-", Rules.GetCrateBegins(),
	         Rules.GetCrateStays(), Rules.GetCrateEnds(), Rules.GetCheckpoint(), Rules.GetRespawns());
}
} // namespace Dxf::GameplaySample
