#pragma once
#include "combat_rules.h"
struct AttackDefinition;
// Scene-local tile cursor is distinct from the source event's screen sample.
AttackSample sampleProjectile(const AttackDefinition&,GridPosition cursor,Direction,bool muzzle,int advances);
