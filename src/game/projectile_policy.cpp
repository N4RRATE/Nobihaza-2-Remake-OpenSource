#include "projectile_policy.h"
#include "expanded_content.h"
#include <algorithm>
AttackSample sampleProjectile(const AttackDefinition& rule,GridPosition cursor,Direction direction,bool muzzle,int advances){
    const auto delta=adjacentTile({0,0},direction);
    const bool shotgun=rule.strategy==ProjectileStrategy::ShotgunDistance;
    const int offset=shotgun?(muzzle?rule.muzzleForward[static_cast<int>(direction)]:rule.forwardOffset+rule.sampleLeadPixels):rule.forwardOffset;
    const GridPosition center{(cursor.x+.5)*16+delta.x*offset,(cursor.y+.5)*16+delta.y*offset};
    if(!shotgun){return {center,rule.halfPixels,AttackKind::De,rule.responsePage,advances,false};}
    const bool vertical=direction==Direction::Up||direction==Direction::Down;
    return {center,muzzle?rule.muzzleHalf:vertical?rule.verticalHalf:rule.horizontalHalf,
        AttackKind::Shotgun,muzzle?5:rule.responseByAdvance[std::clamp(advances,0,3)],advances,false};
}
