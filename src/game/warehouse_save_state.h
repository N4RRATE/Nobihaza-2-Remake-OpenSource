#pragma once
#include "combat_scene.h"
#include <cmath>
// Fixed G6 warehouse profile, not an event interpreter or a general scene serializer.
namespace warehouse_save {
using Json=nlohmann::json;
inline void need(bool b,const char* m){if(!b){throw std::runtime_error(m);}}
inline int integer(const Json& j,int lo,int hi){need(j.is_number_integer(),"Warehouse integer expected");if(j.is_number_unsigned()){const auto n=j.get<std::uint64_t>();need(hi>=0&&n<=std::uint64_t(hi)&&(lo<=0||n>=std::uint64_t(lo)),"Warehouse integer outside range");return int(n);}const auto n=j.get<std::int64_t>();need(n>=lo&&n<=hi,"Warehouse integer outside range");return int(n);}
inline bool boolean(const Json& j){need(j.is_boolean(),"Warehouse boolean expected");return j.get<bool>();}
inline double number(const Json& j,double lo,double hi){need(j.is_number(),"Warehouse number expected");const double n=j.get<double>();need(std::isfinite(n)&&n>=lo&&n<=hi,"Warehouse number outside range");return n;}
inline std::string string(const Json& j){need(j.is_string()&&j.get<std::string>().size()<=160,"Warehouse ID invalid");return j.get<std::string>();}
inline void keys(const Json& j,std::initializer_list<const char*> names){need(j.is_object()&&j.size()==names.size(),"Warehouse fields missing or unknown");for(const auto* n:names){need(j.contains(n),"Warehouse field missing");}}
inline GridPosition grid(const Json& j,double lo=-4096,double hi=4096){need(j.is_array()&&j.size()==2,"Warehouse position shape");return {number(j[0],lo,hi),number(j[1],lo,hi)};}
inline Json xy(GridPosition p){return Json::array({p.x,p.y});}
}
class WarehouseSaveState {
public:
 static nlohmann::json motion(const CharacterMotion&);
 static CharacterMotion motion(const nlohmann::json&,int width,int height);
 static nlohmann::json capture(const CombatScene&);
 static nlohmann::json pendingVisual(const CombatScene&);
 static void restore(CombatScene&,const nlohmann::json&);
private:
 static nlohmann::json actor(const CharacterState&);
 static CharacterState actor(const CombatScene&,const nlohmann::json&,int version);
 static nlohmann::json route(const RoutePlayer&);
 static RoutePlayer route(const CombatScene&,const nlohmann::json&,bool ambient);
};
void validateWarehouseVisual(const CombatContent&,const nlohmann::json&,const SoundDefinition&);
