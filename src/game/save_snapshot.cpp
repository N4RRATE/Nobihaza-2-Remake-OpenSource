#include "warehouse_save_state.h"
#include "save_snapshot.h"
#include "full_content.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <limits>
#include <stdexcept>
using Json=nlohmann::json;
namespace{
    constexpr std::size_t maxBytes=8*1024*1024;
    int saveVersion(const std::string& identity){
        if(identity=="f3-normal-1"||identity=="f4a-normal-1"||identity=="f4bc-normal-1"){return 1;}
        if(identity=="f4-normal-1"){return 2;}
        if(identity=="post-f5-g8-late-4"||identity=="post-f5-g8-region-3"||identity=="post-f5-g8-first-part-2"||identity=="post-f5-g8-initialization-1"||identity=="post-f5-g0-normal-1"||identity=="post-f5-g0-normal-2"||identity=="post-f5-g1-normal-1"||identity=="post-f5-g1-normal-2"||identity=="post-f5-g1-normal-3"||identity=="post-f5-g1-normal-4"||identity=="post-f5-g1-normal-5"||identity=="post-f5-g1-normal-6"||identity=="post-f5-g1-normal-7"||identity=="post-f5-g1-normal-8"||identity=="post-f5-g1-normal-9"||identity=="post-f5-g2-normal-1"||identity=="post-f5-g2-normal-2"||identity=="post-f5-g2-normal-3"||identity=="post-f5-g2-normal-4"||identity=="post-f5-g2-normal-5"||identity=="post-f5-g2-normal-6"||identity=="post-f5-g3-normal-1"||identity=="post-f5-g3-normal-2"||identity=="post-f5-g3-normal-3"||identity=="post-f5-g3-normal-4"||identity=="post-f5-g3-normal-5"||identity=="post-f5-g3-normal-6"||identity=="post-f5-g3-normal-7"||identity=="post-f5-g4-normal-1"||identity=="post-f5-g4-normal-2"||identity=="post-f5-g7-normal-7"||identity=="post-f5-g7-normal-6"||identity=="post-f5-g7-normal-4"||identity=="post-f5-g7-normal-1"||identity=="post-f5-g6-normal-5"||identity=="post-f5-g6-normal-4"||identity=="post-f5-g6-normal-3"||identity=="post-f5-g6-normal-2"||identity=="post-f5-g6-normal-1"||identity=="post-f5-g5-normal-3"||identity=="post-f5-g5-normal-2"||identity=="post-f5-g5-normal-1"||identity=="post-f5-g4-normal-3"){return 3;}
        throw IncompatibleSave("Unknown save content identity");
    }
    const std::set<int> persistentFlags{1,18,21,37,46,47,50};
    const std::set<int> persistentVariables{35,41,382,399,630};
    void need(bool ok,const char* message){if(!ok){throw std::runtime_error(message);}}
    void keys(const Json& j,std::initializer_list<const char*> names,std::initializer_list<const char*> extra={}){
        need(j.is_object()&&j.size()==names.size()+extra.size(),"Save object has missing or unknown fields");
        for(const auto* name:names){need(j.contains(name),"Save field missing");}
        for(const auto* name:extra){need(j.contains(name),"Save field missing");}
    }
    int integer(const Json& j){
        need(j.is_number_integer(),"Save integer expected");
        if(j.is_number_unsigned()){need(j.get<std::uint64_t>()<=std::uint64_t(INT_MAX),"Save integer overflow");}
        else{need(j.get<std::int64_t>()>=INT_MIN&&j.get<std::int64_t>()<=INT_MAX,"Save integer overflow");}
        return j.get<int>();
    }
    int number(const Json& j,const char* key){return integer(j.at(key));}
    std::string string(const Json& j){need(j.is_string(),"Save string expected");return j.get<std::string>();}
    bool boolean(const Json& j){need(j.is_boolean(),"Save boolean expected");return j.get<bool>();}
    template<class K,class V> Json pairs(const std::map<K,V>& values){
        Json out=Json::array();for(const auto& [key,value]:values){out.push_back({key,value});}return out;
    }
    template<class K,class V> std::map<K,V> readPairs(const Json& j){
        need(j.is_array(),"Save collection expected");std::map<K,V> out;
        for(const auto& row:j){need(row.is_array()&&row.size()==2,"Save pair expected");K key;V value;
            if constexpr(std::is_same_v<K,int>){key=integer(row[0]);}else{key=string(row[0]);}
            if constexpr(std::is_same_v<V,int>){value=integer(row[1]);}else{value=boolean(row[1]);}
            need(out.emplace(key,value).second,"Duplicate save ID");
        }return out;
    }
    const std::array<std::string,6> timerSources{"none","training","power","guard-first","guard-final","access-key"};
    Json sessionJson(const SessionEntry& e,bool full,bool chapter){
        Json result={{"actor",e.actor},{"name",e.actorName},{"level",e.level},{"difficulty",e.difficulty},{"party",e.party},{"save_count",e.saveCount},
            {"inventory",pairs(e.inventory)},{"magazines",pairs(e.magazines)},{"uses",pairs(e.itemUses)},{"flags",pairs(e.flags)},{"variables",pairs(e.variables)},
            {"world_flags",pairs(e.worldFlags)},{"world_values",pairs(e.worldValues)},{"skills",e.skills},
            {"equipment",{e.mainWeapon,e.auxiliaryWeapon,e.armor,e.accessory,e.otherEquipment}},
            {"hp",e.hp},{"max_hp",e.maxHp},{"tp",e.tp},{"max_tp",e.maxTp},
            {"attributes",{e.baseAttack,e.baseDefense,e.baseAgility,e.baseSpirit}},
            {"conditions",{e.conditions.bleedingTicks,e.conditions.invulnerableTicks,e.conditions.recoveryTicks,e.conditions.walkedSteps}},{"quickshot_ticks",e.quickshotTicks}};
        result["common_effects"]={{"bleeding_active",e.commonEffectsKnown?e.conditions.bleedingActive:e.conditions.bleedingTicks>0},{"quickshot_active",e.commonEffectsKnown?e.quickshotActive:e.quickshotTicks>0}};
        if(chapter){result["actor_face"]={{"image",e.actorFace},{"index",e.actorFaceIndex}};}
        if(full){
            const auto& timer=e.eventTimer;
            result["event_timer"]={{"source",timerSources.at(static_cast<int>(timer.source))},{"remaining_ticks",timer.remainingTicks},{"running",timer.running},{"visible",timer.visible},{"battle",timer.battle}};
            result["pursuer_health"]=e.pursuerHealth;
            result["poisoned"]=e.conditions.poisoned;result["poison_wait_ticks"]=e.conditions.poisonWaitTicks;
            result["neural_paralysis"]=e.conditions.neuralParalysis;
        }
        return result;
    }
    std::vector<int> ints(const Json& j,int length=-1){need(j.is_array()&&(length<0||j.size()==std::size_t(length)),"Save array length invalid");std::vector<int> v;for(const auto& x:j){v.push_back(integer(x));}return v;}
    SessionEntry readSession(Json j,bool full,bool chapter,bool effects){
        bool bleeding=false,quick=false;
        if(effects){const auto& x=j.at("common_effects");keys(x,{"bleeding_active","quickshot_active"});bleeding=boolean(x.at("bleeding_active"));quick=boolean(x.at("quickshot_active"));j.erase("common_effects");}
        std::string face="face-actor1";int index=1;if(chapter){const auto& f=j.at("actor_face");keys(f,{"image","index"});face=string(f.at("image"));index=number(f,"index");j.erase("actor_face");}
        keys(j,{"actor","name","level","difficulty","party","save_count","inventory","magazines","uses","flags","variables","world_flags","world_values","skills","equipment","hp","max_hp","tp","max_tp","attributes","conditions","quickshot_ticks"},full?std::initializer_list<const char*>{"event_timer","pursuer_health","poisoned","poison_wait_ticks","neural_paralysis"}:std::initializer_list<const char*>{});
        SessionEntry e;e.actorFace=face;e.actorFaceIndex=index;e.actor=number(j,"actor");e.actorName=string(j.at("name"));e.level=number(j,"level");e.difficulty=string(j.at("difficulty"));e.party=ints(j.at("party"));e.saveCount=number(j,"save_count");
        e.inventory=readPairs<int,int>(j.at("inventory"));e.magazines=readPairs<int,int>(j.at("magazines"));e.itemUses=readPairs<int,int>(j.at("uses"));e.flags=readPairs<int,bool>(j.at("flags"));e.variables=readPairs<int,int>(j.at("variables"));e.worldFlags=readPairs<std::string,bool>(j.at("world_flags"));e.worldValues=readPairs<std::string,int>(j.at("world_values"));
        auto equipment=ints(j.at("equipment"),5);e.mainWeapon=equipment[0];e.auxiliaryWeapon=equipment[1];e.armor=equipment[2];e.accessory=equipment[3];e.otherEquipment=equipment[4];
        e.hp=number(j,"hp");e.maxHp=number(j,"max_hp");e.tp=number(j,"tp");e.maxTp=number(j,"max_tp");auto a=ints(j.at("attributes"),4);e.baseAttack=a[0];e.baseDefense=a[1];e.baseAgility=a[2];e.baseSpirit=a[3];auto condition=ints(j.at("conditions"),4);e.conditions={condition[0],condition[1],condition[2],condition[3]};e.quickshotTicks=number(j,"quickshot_ticks");
        e.commonEffectsKnown=effects;e.conditions.bleedingActive=effects?bleeding:e.conditions.bleedingTicks>0;e.quickshotActive=effects?quick:e.quickshotTicks>0;
        for(int id:ints(j.at("skills"))){need(e.skills.insert(id).second,"Duplicate save skill");}
        if(full){
            const auto& timer=j.at("event_timer");keys(timer,{"source","remaining_ticks","running","visible","battle"});
            const auto source=string(timer.at("source"));const auto found=std::find(timerSources.begin(),timerSources.end(),source);
            need(found!=timerSources.end(),"Unknown saved event timer source");
            e.eventTimer={static_cast<EventTimerSource>(found-timerSources.begin()),number(timer,"remaining_ticks"),boolean(timer.at("running")),boolean(timer.at("visible")),boolean(timer.at("battle"))};
            e.pursuerHealth=number(j,"pursuer_health");
            e.conditions.poisoned=boolean(j.at("poisoned"));e.conditions.poisonWaitTicks=number(j,"poison_wait_ticks");
            e.conditions.neuralParalysis=boolean(j.at("neural_paralysis"));
        }
        return e;
    }
    const Json& savePoint(const CombatContent& c,const std::string& id,const GameSession& session){
        const auto* active=c.activeVariant(c.objects.at(id),session);
        for(const auto& p:c.catalog.at("save_points")){
            if(p.at("object")!=id||!active||p.at("page")!=active->page){continue;}
            if(p.contains("required_flags")){for(const auto& flag:p.at("required_flags")){need(session.flag(flag),"Save point has not reached its stable story window");}}
            if(p.contains("absent_flags")){for(const auto& flag:p.at("absent_flags")){need(!session.flag(flag),"Save point is outside its delivered phase");}}
            if(p.contains("allowed_party")){need(std::any_of(p.at("allowed_party").begin(),p.at("allowed_party").end(),[&](const auto& actor){return session.inParty(actor);}),"Save party is not allowed at this point");}
            return p;
        }
        throw std::runtime_error("Unsupported save point");
    }
    std::string difficultyLabel(const Json& point,const SessionEntry& session,const std::string& field){
        if(!point.contains(field+"_by_difficulty")){return point.at(field);}
        auto difficulty=session.difficulty;std::transform(difficulty.begin(),difficulty.end(),difficulty.begin(),[](unsigned char c){return std::toupper(c);});
        return point.at(field+"_by_difficulty").at(difficulty);
    }
    void validTile(const SceneDefinition& map,TilePosition tile){need(tile.x>=0&&tile.y>=0&&tile.x<map.width&&tile.y<map.height,"Save position outside map");}
    CombatEntry restoredEntry(const SaveSnapshot& s){CombatEntry e;e.map=s.scene.map;e.playerTile=s.scene.playerTile;e.direction=s.scene.direction;e.sprite=s.scene.sprite;e.positions=s.scene.positions;e.directions=s.scene.directions;return e;}
    SaveSnapshot captureStableState(const CombatContent& c,const CombatScene& scene,const SavedPresentation& p,bool moving=false){
        SaveSnapshot s;s.contentCompatibility=c.compatibility;s.session=scene.session().exportState();
        std::erase_if(s.session.flags,[](const auto& kv){return !persistentFlags.contains(kv.first);});
        std::erase_if(s.session.variables,[](const auto& kv){return !persistentVariables.contains(kv.first);});
        s.scene.map=scene.mapId();s.scene.playerTile=scene.world().player().motion.tile();s.scene.direction=scene.world().player().motion.facing();s.scene.sprite=scene.world().player().spriteId.value_or("");
        for(const auto& actor:scene.world().objects()){
            need(moving||!actor.motion.moving(),"Cannot capture a moving event");
            s.scene.positions.emplace(actor.id,actor.motion.tile());s.scene.directions.emplace(actor.id,actor.motion.facing());
        }
        s.presentation=p;s.presentation.warehouseVisual=nullptr;return s;
    }
}
SaveSnapshot captureSave(const CombatContent& c,const CombatScene& scene,const SaveRequest& request,const SavedPresentation& p){
    const bool clear=request.kind=="clear_after_file",chapter=request.kind=="chapter_handoff_after_file",direct=request.kind=="direct_unscored";
    need(request.kind=="ordinary"||clear||chapter||direct,"Unknown save request kind");
    need(!clear||scene.clearSavePending(request),"Stale Clear request");
    need(!direct||scene.directSavePending(request),"Stale direct request");
    need(!chapter||scene.chapterSavePending(request),"Stale chapter request");
    need(scene.active()&&request.scene==scene.generation()&&((chapter||clear)?scene.foreground()==ForegroundKind::Content:scene.foreground()==ForegroundKind::None)&&!scene.raised()&&!scene.world().player().motion.moving(),"Save request is stale or scene is not stable");
    need(scene.session().hp()>0&&(chapter||clear||(scene.session().flag(1)&&scene.session().flag(18)))&&!scene.session().flag(10)&&!scene.session().flag(15),"Cannot save without live player control");
    if(!c.expansion.tool.is_null()){need(!scene.session().flag(c.expansion.tool.at("area_flag")),"Cannot save inside an active tool context");}
    auto s=captureStableState(c,scene,p,direct);if(clear){s.formatVersion=6;}
    if(direct){s.formatVersion=5;s.warehouse=scene.captureWarehouseState();s.presentation=p;if(!s.presentation.warehouseVisual.is_null()){s.presentation.warehouseVisual["pending"]=scene.pendingWarehouseVisual();}s.session.conditions.invulnerableTicks=s.session.conditions.recoveryTicks=0;s.session.conditions.corroded=false;}
    s.continuation={saveVersion(c.compatibility)==3?2:1,request.object,s.session.saveCount,scene.session().variable(399),request.kind};
    need(s.session.saveCount<INT_MAX&&s.continuation.statisticBefore<INT_MAX,"Save counter exhausted");
    ++s.session.saveCount;s.session.flags[1]=false;s.session.flags[18]=false;
    const auto& point=clear?c.catalog.at("clear"):chapter?c.catalog.at("chapter"):savePoint(c,request.object,scene.session());
    s.summary={std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(),point.at("location").get<std::string>(),chapter?s.session.actorFace:"face-actor1",chapter?s.session.actorFaceIndex:15,clear?s.session.actor8Name:chapter?s.session.actorName:difficultyLabel(point,s.session,"slot_label")};
    if(clear){s.summary.face=point.at("face");s.summary.faceIndex=point.at("face_index");s.summary.label=s.session.actor8Name;}
    validateSave(c,s);return s;
}
SaveSnapshot captureF5Recovery(const CombatContent& c,const CombatScene& scene,const SavedPresentation& p){
    need(c.catalog.contains("development_recovery"),"Development recovery node unavailable");
    need(scene.foreground()==ForegroundKind::None&&!scene.raised()&&!scene.world().player().motion.moving()&&!scene.exitRequest(),"Development recovery scene is not stable");
    need(scene.session().flag(1)&&scene.session().flag(18)&&!scene.session().flag(10)&&!scene.session().flag(15),"Development recovery requires live controls");
    auto s=captureStableState(c,scene,p);
    s.continuation={2,c.catalog.at("development_recovery").at("node"),s.session.saveCount,scene.session().variable(399),"development_f5_recovery"};
    const auto& point=c.catalog.at("development_recovery");
    s.summary={std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(),point.at("location"),"face-actor1",15,point.at("slot_label")};
    s.presentation=p;s.presentation.warehouseVisual=nullptr;validateSave(c,s);return s;
}
void validateSave(const CombatContent& c,const SaveSnapshot& s){
    need(s.formatVersion>=1&&s.formatVersion<=6,"Unsupported save format");
    if(s.contentCompatibility!=c.compatibility){throw IncompatibleSave("Incompatible save content");}need(s.build.size()<=128,"Save build label too long");
    const auto& e=s.session;const bool clear=s.continuation.kind=="clear_after_file",chapter=s.continuation.kind=="chapter_handoff_after_file",recovery=s.continuation.kind=="development_f5_recovery",direct=s.continuation.kind=="direct_unscored";
    need(s.continuation.kind=="ordinary"||clear||chapter||recovery||direct,"Unknown save continuation kind");
    need(direct?(s.formatVersion==5&&c.catalog.contains("warehouse_direct")&&!s.warehouse.is_null()):((clear?s.formatVersion==6:s.formatVersion<=4)&&s.warehouse.is_null()),"Warehouse profile differs from save kind");
    need(direct||s.presentation.warehouseVisual.is_null(),"Ordinary save has warehouse visuals");
    need(!recovery||((c.compatibility=="post-f5-g8-late-4"||c.compatibility=="post-f5-g8-region-3"||c.compatibility=="post-f5-g8-first-part-2"||c.compatibility=="post-f5-g8-initialization-1"||c.compatibility=="post-f5-g0-normal-2"||c.compatibility=="post-f5-g1-normal-1"||c.compatibility=="post-f5-g1-normal-2"||c.compatibility=="post-f5-g1-normal-3"||c.compatibility=="post-f5-g1-normal-4"||c.compatibility=="post-f5-g1-normal-5"||c.compatibility=="post-f5-g1-normal-6"||c.compatibility=="post-f5-g1-normal-7"||c.compatibility=="post-f5-g1-normal-8"||c.compatibility=="post-f5-g1-normal-9"||c.compatibility=="post-f5-g2-normal-1"||c.compatibility=="post-f5-g2-normal-2"||c.compatibility=="post-f5-g2-normal-3"||c.compatibility=="post-f5-g2-normal-4"||c.compatibility=="post-f5-g2-normal-5"||c.compatibility=="post-f5-g2-normal-6"||c.compatibility=="post-f5-g3-normal-1"||c.compatibility=="post-f5-g3-normal-2"||c.compatibility=="post-f5-g3-normal-3"||c.compatibility=="post-f5-g3-normal-4"||c.compatibility=="post-f5-g3-normal-5"||c.compatibility=="post-f5-g3-normal-6"||c.compatibility=="post-f5-g3-normal-7"||c.compatibility=="post-f5-g4-normal-1"||c.compatibility=="post-f5-g4-normal-2"||c.compatibility=="post-f5-g7-normal-7"||c.compatibility=="post-f5-g7-normal-6"||c.compatibility=="post-f5-g7-normal-4"||c.compatibility=="post-f5-g7-normal-1"||c.compatibility=="post-f5-g6-normal-5"||c.compatibility=="post-f5-g6-normal-4"||c.compatibility=="post-f5-g6-normal-3"||c.compatibility=="post-f5-g6-normal-2"||c.compatibility=="post-f5-g6-normal-1"||c.compatibility=="post-f5-g5-normal-3"||c.compatibility=="post-f5-g5-normal-2"||c.compatibility=="post-f5-g5-normal-1"||c.compatibility=="post-f5-g4-normal-3")&&c.catalog.contains("development_recovery")),"Development recovery node unavailable");
    need(!clear||(c.catalog.contains("clear")&&saveVersion(c.compatibility)==3),"Clear not delivered");
    need(!chapter||(saveVersion(c.compatibility)==3&&c.catalog.contains("chapter")),"Chapter not delivered");
    if(saveVersion(c.compatibility)==1){need(e.eventTimer==EventTimer{}&&e.pursuerHealth==500&&!e.conditions.poisoned&&e.conditions.poisonWaitTicks==0&&!e.conditions.neuralParalysis,"Legacy save cannot represent full F4 persistent state");}
    need(e.actor==1&&e.actorName==(chapter?difficultyLabel(c.catalog.at("chapter"),e,"name"):"大雄")&&e.party==(clear?std::vector<int>{8}:std::vector<int>{1})&&e.level==1,"Unsupported save actor, party or difficulty");
    if(c.catalog.contains("difficulty")){
        const auto& support=c.catalog.at("difficulty").at("support");
        if(!chapter&&!recovery&&!direct&&!clear){
            const auto& ordinary=support.at("ordinary");need(ordinary.contains(e.difficulty),"Unsupported ordinary difficulty");
            const auto& points=ordinary.at(e.difficulty);need(std::find(points.begin(),points.end(),"*")!=points.end()||std::find(points.begin(),points.end(),s.continuation.point)!=points.end(),"Difficulty save point is not delivered");
        }else{
            const auto& allowed=support.at(recovery?"development_recovery":clear?"clear":chapter?"chapter":"direct");
            need(std::find(allowed.begin(),allowed.end(),e.difficulty)!=allowed.end(),"Difficulty save kind is not delivered");
        }
        if(e.difficulty=="easy"){GameSession state(c.items,e,c.facts);for(const auto& id:c.catalog.at("difficulty").at("easy_preseed")){need(state.flag(id),"EASY initialization fact missing");}}
    }else{need(e.difficulty=="normal","Unsupported legacy difficulty");}
    need(e.actorFace==(chapter?c.catalog.at("chapter").at("face").get<std::string>():"face-actor1")&&e.actorFaceIndex==(chapter?15:1),"Unsupported actor face state");
    need(e.actor8Name==(clear?c.catalog.at("clear").at("name").get<std::string>():"dammy"),"Actor8 name differs from save boundary");
    need(e.hp>0&&e.maxHp>0&&e.maxHp<=9999&&e.maxTp>=0&&e.maxTp<=9999,"Invalid save vital limits");
    for(int n:{e.baseAttack,e.baseDefense,e.baseAgility,e.baseSpirit}){need(n>=1&&n<=999,"Invalid save attribute");}
    for(const auto& [id,n]:e.inventory){need(n>=0&&n<=99,"Save inventory outside 0..99");}
    for(const auto& [item,initial]:c.requiredMagazines){need(e.magazines.contains(item),"Missing required weapon magazine state");}
    for(const auto& [id,n]:e.magazines){need(c.items.contains(id)&&c.items.at(id).capacity>0,"Unknown save magazine");}
    for(const auto& [id,b]:e.flags){need(persistentFlags.contains(id),"Unsupported save switch");}
    for(const auto& [id,n]:e.variables){need(persistentVariables.contains(id)&&n>=0&&n<INT_MAX,"Invalid save statistic");}
    const auto flagIds=c.facts.flagIds(),valueIds=c.facts.valueIds();
    for(const auto& [id,b]:e.worldFlags){need(flagIds.contains(id),"Unknown world fact");}
    for(const auto& [id,n]:e.worldValues){need(valueIds.contains(id),"Unknown world value");}
    if(c.facts.variables.contains(49)){const auto it=e.worldValues.find("combat.last-defense");need(it!=e.worldValues.end()&&it->second>=0&&it->second<=999,"Invalid retained combat defense");}
    for(int id:e.skills){need(c.skills.contains(id),"Unknown save skill");}
    const int quickLimit=c.skills.at(9).durationTicks;need(e.quickshotTicks>=0&&e.quickshotTicks<=quickLimit,"Invalid quickshot timer");
    need(e.eventTimer.source!=EventTimerSource::AccessKey||(c.catalog.contains("g3")&&c.catalog.at("g3").at("delivered").get<int>()>=4),"Access-key timer outside its source contract");
    need(!e.conditions.corroded,"Active corrosion outside ordinary save boundary");
    need(e.conditions.bleedingTicks>=0&&e.conditions.bleedingTicks<=360000&&e.conditions.invulnerableTicks==0&&e.conditions.recoveryTicks==0&&e.conditions.walkedSteps>=0&&e.conditions.walkedSteps<INT_MAX,"Invalid save conditions");
    if(e.commonEffectsKnown){need(!e.conditions.bleedingActive||e.conditions.bleedingTicks>0,"Active bleeding without CE34 wait");need(!e.quickshotActive||e.quickshotTicks>0,"Active quickshot without CE60 wait");}
    validateFullSession(c,e,clear);
    GameSession check(c.items,e,c.facts);
    if(c.catalog.contains("g4")&&c.catalog.at("g4").contains("values")){
        // V627=1..4 belongs to the compulsory truth presentation. This save
        // contract has no foreground cursor and cannot reconstruct those stages.
        need(check.variable(627)==0||check.variable(627)==5,"Truth presentation stage cannot be saved");
        need(check.variable(627)!=5||check.flag(506),"Final truth stage without completion");
        need(!check.flag(504)||check.flag(506),"Dora victory cannot be saved before compulsory truth completion");
    }
    if(!chapter&&!recovery&&!clear&&c.catalog.contains("g5")&&c.catalog.at("g5").contains("control_contract")){
        const auto& contract=c.catalog.at("g5").at("control_contract");
        if(check.flag(contract.at("victory_flag"))&&!(c.catalog.contains("g6")&&check.flag(c.catalog.at("g6").at("return_contract").at("phase")))){const auto& points=contract.at("post_victory_save_points");need(std::find(points.begin(),points.end(),s.continuation.point)!=points.end(),"Cash victory outside its reachable ordinary point");}
    }
    // The delivered Continue capture is confined to the boss arena. Its only return
    // to an ordinary save point follows M165 E3P7 C21–23, which clears S586.
    // Retained CE141 backup values are legal; an active checkpoint on disk is not.
    if(!chapter&&!recovery&&!clear&&c.catalog.contains("g3")){need(!check.flag(586),"Active Continue checkpoint at an ordinary save point");}
    if(!chapter&&!recovery&&!clear&&c.catalog.contains("g1")&&c.catalog.at("g1").at("delivered").get<int>()>=6){need(check.count(96)==0,"Usable clock part outside its legal region at ordinary save");}
    for(const auto& [id,enemy]:c.expansion.enemies){if(enemy.activationFlag){
        const bool sourceEntry=enemy.rules.contains("entry_requires_fact")&&check.flag(enemy.rules.at("entry_requires_fact"));
        need(!check.flag(enemy.deadFlag)||check.flag(*enemy.activationFlag)||sourceEntry||isDifficultyPreset(c,check,enemy.deadFlag),"Enemy death without activation");
    }}
    for(int slot=1;slot<=5;++slot){need(check.canEquip(check.equipped(slot),slot),"Illegal save equipment");}
    need(check.flag(1)==recovery&&check.flag(18)==recovery,"Save controls disagree with continuation kind");
    need(s.continuation.version==(saveVersion(c.compatibility)==3?2:1)&&s.continuation.countBefore>=0&&s.continuation.countBefore<INT_MAX&&e.saveCount==s.continuation.countBefore+(recovery?0:1)&&s.continuation.statisticBefore>=0&&s.continuation.statisticBefore<INT_MAX&&check.variable(399)==s.continuation.statisticBefore,"Contradictory after-save marker");
    const auto& map=c.maps.at(s.scene.map);validTile(map,s.scene.playerTile);
    if(clear){
        const auto& point=c.catalog.at("clear");need(s.continuation.point==point.at("node").get<std::string>()&&s.scene.map==point.at("map")&&s.scene.playerTile==TilePosition{point.at("tile").at(0),point.at("tile").at(1)},"Clear source position/node differs");
        need(check.flag(574)&&check.flag(575)&&check.flag(576)&&check.flag(582)&&check.flag(581)&&check.flag(580),"Clear lacks completed final battle");
        const auto rank=endingScore(check.endingScoreInput());for(int id:rank.unlocks){need(check.flag(id),"Clear lacks completed ending reward");}
        need(!e.eventTimer.running&&!e.eventTimer.visible&&!e.eventTimer.battle&&!e.conditions.poisoned&&!e.conditions.neuralParalysis&&e.hp==e.maxHp&&e.tp==e.maxTp,"Clear Actor1 recovery differs");
        need(s.summary.savedAtUnix>0&&s.summary.location==point.at("location").get<std::string>()&&s.summary.face==point.at("face").get<std::string>()&&s.summary.faceIndex==point.at("face_index")&&s.summary.label==e.actor8Name,"Invalid Clear slot summary");
        need(s.presentation.music==readSound(point.at("music")),"Clear stored BGM differs");
    }else if(recovery){
        const auto& point=c.catalog.at("development_recovery");
        need(s.continuation.point==point.at("node").get<std::string>()&&s.scene.map==point.at("map")&&s.scene.playerTile==TilePosition{point.at("tile").at(0),point.at("tile").at(1)}&&s.scene.direction==directionFromName(point.at("facing").get<std::string>()),"Development recovery position/node differs");
        for(const auto& flag:point.at("required_flags")){need(check.flag(flag),"Development recovery missing F5 prerequisite");}
        for(const auto& flag:point.at("absent_flags")){need(!check.flag(flag),"Development recovery already entered hangar");}
        need(!e.eventTimer.running&&!e.eventTimer.visible&&!e.eventTimer.battle&&!check.flag(10)&&!check.flag(15),"Development recovery retains transient battle state");
        need(s.summary.savedAtUnix>0&&s.summary.location==point.at("location").get<std::string>()&&s.summary.face=="face-actor1"&&s.summary.faceIndex==15&&s.summary.label==point.at("slot_label").get<std::string>(),"Invalid development recovery summary");
    }else{
        const auto& point=chapter?c.catalog.at("chapter"):savePoint(c,s.continuation.point,check);
        const auto object=chapter?point.at("object").get<std::string>():s.continuation.point;
        need(c.objects.contains(object),"Missing save point content");const auto& obj=c.objects.at(object);
        if(chapter){need(s.continuation.point==point.at("node").get<std::string>()&&check.flag(346)&&check.flag(347)&&check.flag(348)&&check.flag(349)&&check.flag(350)&&check.flag(357)&&!check.flag(360)&&check.variable(13)==1,"Chapter node disagrees with source facts");}
        need(s.summary.savedAtUnix>0&&s.summary.location==point.at("location").get<std::string>()&&s.summary.face==(chapter?e.actorFace:"face-actor1")&&s.summary.faceIndex==15&&s.summary.label==(chapter?e.actorName:difficultyLabel(point,e,"slot_label")),"Invalid save slot summary");
        need(s.scene.map==obj.map&&c.maps.contains(s.scene.map),"Save map and point disagree");
        if(chapter){need(int(s.scene.direction)>=0&&int(s.scene.direction)<=3&&s.scene.playerTile==TilePosition{point.at("tile").at(0),point.at("tile").at(1)},"Chapter position differs from source entry");}
        else if(direct){need(point.at("kind")=="direct_unscored"&&point.at("geometry")=="same_tile"&&s.scene.playerTile==obj.tile&&int(s.scene.direction)>=0&&int(s.scene.direction)<=3,"Direct save is not on its source tile");}
        else{need(!point.contains("kind")&&s.scene.direction==directionFromName(point.at("facing").get<std::string>())&&adjacentTile(s.scene.playerTile,s.scene.direction)==obj.tile,"Save position is not the reviewed approach");}
        const auto* page=c.activeVariant(obj,check);need(page&&page->page==point.at("page").get<int>(),"Save point page unavailable");
    }
    need(map.sprites->contains(s.scene.sprite),"Unknown saved player graphic");
    std::set<std::string> expected;
    for(const auto& [id,o]:c.objects){if(o.map==s.scene.map){expected.insert(id);}}
    need(s.scene.positions.size()==expected.size()&&s.scene.directions.size()==expected.size(),"Incomplete saved event state");
    for(const auto& [id,tile]:s.scene.positions){need(expected.contains(id)&&s.scene.directions.contains(id),"Unknown saved event");validTile(map,tile);need(int(s.scene.directions.at(id))>=0&&int(s.scene.directions.at(id))<=3,"Invalid event direction");}
    if(recovery||clear){for(const auto& id:expected){need(s.scene.positions.at(id)==c.objects.at(id).tile,"Development recovery event moved");}}
    else{const auto& point=chapter?c.catalog.at("chapter"):savePoint(c,s.continuation.point,check);const auto object=chapter?point.at("object").get<std::string>():s.continuation.point;need(s.scene.positions.at(object)==c.objects.at(object).tile,"Moved save point is unsupported");}
    need(!check.flag(54)||check.flag(101),"Reward without corpse");need(!check.flag(101)||check.flag(52),"Corpse before pickup");
    if(check.flag(52)){need(check.variable(388)>=1000&&check.variable(388)<=9999,"Invalid generated password");}
    if(!chapter&&!recovery&&!clear&&c.catalog.contains("g1")&&c.catalog.at("g1").at("delivered").get<int>()>=7){need(check.count(98)==0,"Usable axe outside its legal region at ordinary save");}
    if(!chapter&&!recovery&&!clear&&c.catalog.contains("g1")&&c.catalog.at("g1").at("delivered").get<int>()>=8){need(check.count(100)==0,"Usable piano device outside its legal region at ordinary save");need(!check.flag(429)||(check.flag(378)&&check.flag(379)&&check.flag(438)),"Piano battle or monitor in progress at ordinary save");}
    if(check.flag(101)){validTile(c.maps.at(c.objects.at(c.rules.at("enemy").at("id").get<std::string>()).map),{check.variable(396),check.variable(397)});need(check.variable(398)>=1&&check.variable(398)<=3,"Invalid corpse variant");}
    need(!check.flag(60)||check.flag(59),"Unlocked archive before acquiring key");
    if(!check.flag(59)){need(check.count(60)==0,"Archive key without acquisition fact");}
    else{need(check.count(60)<=1&&(check.flag(60)||check.count(60)==1),"Acquired archive key missing before unlock");}
    if(!c.branch.is_null()){
        const auto& power=c.branch.at("power_tool");const int inactive=power.at("inactive_item"),usable=power.at("usable_item");
        need(!check.flag(93)||check.flag(87),"Installed cable before acquisition");
        need(check.count(usable)==0&&check.count(inactive)==(check.flag(87)&&!check.flag(93)?1:0),"Power cable disagrees with legal save context");
        need(!check.flag(88)||check.flag(87),"Storage dog before cable removal");
        need(!check.flag(89)||(check.flag(88)&&check.flag(90)),"Dining arrival before storage or glass");
        need(!check.flag(91)||check.flag(88),"Shared dog before storage trigger");
        need(!(check.flag(96)&&check.flag(97)),"Wardrobe cannot end on both sides");
        need(check.count(69)==0&&check.count(68)==(check.flag(145)?1:0),"Lighter disagrees with legal save context");
        need(!check.flag(141)||check.flag(145),"Flame before lighter acquisition");
        need(!check.flag(142)||check.flag(141),"Matched puzzle before lighting candle");
        need(!check.flag(143)||check.flag(142),"Revealed puzzle before matching statues");
        need(!check.flag(144)||check.flag(143),"Spade key before puzzle reveal");
        need(!check.flag(147)||check.flag(144),"Spade door before key acquisition");
        need(check.count(70)<=1&&(check.flag(144)?(check.flag(147)||check.count(70)==1):check.count(70)==0),"Spade key disagrees with acquisition and discard facts");
        need(!check.flag(318)||check.flag(316),"Gemstone before tooth acquisition");
        need(!check.flag(314)||check.flag(318),"Blue eye before gemstone acquisition");
        need(check.count(88)==0&&check.count(87)==(check.flag(316)&&!check.flag(318)?1:0),"Tooth disagrees with legal save context");
        need(check.count(92)==0&&check.count(91)==(check.flag(318)&&!check.flag(314)?1:0),"Gemstone disagrees with legal save context");
        if(c.catalog.contains("rescue_crises")){
            const int outcomes=static_cast<int>(check.flag(272))+check.flag(273)+check.flag(337)+check.flag(338);
            need(outcomes<=1,"Conflicting rescue outcomes");
            need((outcomes==1)==check.flag(274)&&check.flag(213)==check.flag(274),"Incomplete rescue cannot reach a legal save point");
            need(!(check.flag(272)||check.flag(273))||check.flag(144),"Gian outcome before request");
            need(!(check.flag(337)||check.flag(338))||check.flag(318),"Suneo outcome before request");
            need(!check.flag(318)||check.flag(274),"Sealed Suneo crisis has no legal save path");
            need(!check.flag(336)||check.flag(273),"SG reward without timely Gian rescue");
            need(!check.flag(339)||check.flag(338),"LC9 reward without timely Suneo rescue");
        }
        std::set<int> sourceClearedEnemies;
        // S202 also records an ordinary M68 enemy death; it alone cannot prove fuse pickup.
        const bool fuseTaken=check.count(78)>0||check.count(79)>0||check.worldFlag("underground.fuse-installed");
        if(!c.full->is_null()&&c.full->at("runtime").contains("underground")){
            // CN1.00 M76E5P2 C11 clears enemies before C12 publishes fuse pickup.
            // Read the already verified finite source profile; no map or enemy IDs
            // are inferred from the current save location.
            const auto& steps=c.full->at("runtime").at("underground").at("flows").at("m76-e5-p2").at("steps");
            for(std::size_t n=0;n+1<steps.size();++n){
                const auto& clear=steps[n];const auto& completed=steps[n+1];
                if(clear.at("kind")!="flags"||!clear.at("value").get<bool>()||completed.at("kind")!="flag"||!completed.at("value").get<bool>()||!fuseTaken){continue;}
                for(const auto& fact:clear.at("ids")){need(check.flag(fact),"Incomplete source enemy clearance");sourceClearedEnemies.insert(fact.get<int>());}
                need(check.flag(completed.at("id")),"Incomplete source fuse pickup");
                for(const auto& [id,row]:c.full->at("runtime").at("underground").at("encounters").items()){
                    // Pickup itself activated this segment; its live pursuit/stop flags may reset later.
                    if(row.at("type")=="pursuer"&&c.enemies.contains(id)){sourceClearedEnemies.insert(c.enemies.at(id).deadFlag);}
                }
            }
        }
        for(const auto& [id,d]:c.enemies){
            if(d.rules.is_object()&&!d.rules.contains("rescue")&&d.rules.value("behavior",std::string{})!="cerberus"&&d.rules.contains("activation_fact")&&!d.rules.at("activation_fact").is_null()){const int activation=d.rules.value("behavior",std::string{})=="chimera"?d.rules.at("death_activation_fact").get<int>():d.rules.at("activation_fact").get<int>();
                // Chimera source death-page guards can differ from its pursuit activation.
                // S18 is live control, deliberately off until the legal save continuation resumes.
                if(d.rules.value("behavior",std::string{})=="chimera"||c.facts.switches.contains(activation)||(persistentFlags.contains(activation)&&activation!=1&&activation!=18)){need(!check.flag(d.deadFlag)||check.flag(activation)||sourceClearedEnemies.contains(d.deadFlag)||isDifficultyPreset(c,check,d.deadFlag),"Enemy death before activation");}}
            if(d.rules.is_object()&&d.rules.value("behavior",std::string{})=="cerberus"){
            // S119 shares the storage trigger, while ordinary dogs use only the live control gate.
            const int activation=d.deadFlag==119?88:d.rules.at("activation_fact").get<int>();if(activation!=1&&activation!=18){need(!check.flag(d.deadFlag)||check.flag(activation)||sourceClearedEnemies.contains(d.deadFlag)||isDifficultyPreset(c,check,d.deadFlag),"Canine death before activation");}
        }}
    }
    if(c.facts.switches.contains(66)){
        // CN1.00 M22 E4P2 grants I61/S66; M11 E5P3 may discard it before S67.
        need(!check.flag(67)||check.flag(66),"Unlocked wardrobe before acquiring key");
        if(!check.flag(66)){need(check.count(61)==0,"Wardrobe key without acquisition fact");}
        else{need(check.count(61)<=1&&(check.flag(67)||check.count(61)==1),"Acquired wardrobe key missing before unlock");}
    }
    if(c.facts.switches.contains(68)){
        // CN1.00 M11 E5P4 grants one inactive wrench. Reviewed saves are
        // outside M12's active tool area; its temporary I63 form cannot persist.
        need(!check.flag(68)||check.flag(67),"Wrench collected before wardrobe unlock");
        need(check.count(62)==(check.flag(68)?1:0)&&check.count(63)==0,"Wrench inventory disagrees with collection or save context");
    }
    if(c.facts.switches.contains(72)){
        need(!check.flag(72)||check.flag(68),"Vent removed before obtaining wrench");
        // CE55 does not clear S71; retain its completed history, not a live request.
        need(check.flag(71)==check.flag(72),"Incomplete wrench request at save point");
    }
    if(!c.expansion.mixing.is_null()){
        const auto& m=c.expansion.mixing;
        // Ordinary points declare their reachable mixing form; older safe rooms default to enabled.
        auto variant=chapter||recovery||clear?std::string{"enabled"}:savePoint(c,s.continuation.point,check).value("mixing_variant",std::string{"enabled"});
        if(!chapter&&!recovery&&c.catalog.contains("g6")&&c.catalog.at("g6").contains("save_mixing")){
            const auto& boundary=c.catalog.at("g6").at("save_mixing");
            if(check.flag(boundary.at("after_flag"))){variant=boundary.at("variant");}
        }
        need(variant=="enabled"||variant=="disabled"||variant=="preserved","Unknown mixing save variant");
        const bool disabled=recovery||(variant=="preserved"?check.knowsSkill(m.at("disabled_skill")):variant=="disabled");
        const int present=m.at(disabled?"disabled_skill":"enabled_skill"),absent=m.at(disabled?"enabled_skill":"disabled_skill");
        const bool learned=check.knowsSkill(present);
        need(!check.knowsSkill(absent),"Mixing variant differs from reviewed save node");
        // Natural F4-A paths can return from M24 only after the teaching dialogue.
        // These are save reachability invariants, not replacement runtime page conditions.
        need(learned==check.flag(77)&&check.flag(77)==check.flag(73),"Mixing skill disagrees with completed teaching/meeting entry");
    }
    if(!c.expansion.meeting.is_null()){
        const auto& meeting=c.expansion.meeting;const bool entered=check.flag(meeting.at("entered_flag"));
        // CN1.00 M12 E14 -> M23 -> M24 is the only F4-A meeting approach.
        need(!entered||check.flag(c.expansion.tool.at("removed_flag")),"Meeting entry before ventilation access");
        for(const auto& flag:meeting.at("talk_flags")){need(check.flag(flag)==entered,"Incomplete meeting conversations at a legal save point");}
        need(!check.flag(meeting.at("optional_weapon_flag"))||entered,"Optional meeting weapon claimed before entry");
    }
    if(!c.expansion.meetingStory.is_null()){
        const int completed=c.expansion.meetingStory.at("return").at("completed_flag");
        need(check.flag(completed)==check.flag(c.expansion.meeting.at("entered_flag")),"Meeting cannot be saved between entry and completed return");
    }
    need(s.presentation.music.volume>=0&&s.presentation.music.volume<=100&&s.presentation.music.pitch>=50&&s.presentation.music.pitch<=200&&s.presentation.music.balance>=0&&s.presentation.music.balance<=100,"Invalid saved music parameters");
    if(!s.presentation.music.resource.empty()){need(c.media.at("audio").contains(s.presentation.music.resource),"Unknown saved music");}
    need(s.presentation.saturation>=0&&s.presentation.saturation<=200,"Invalid saved saturation");
    for(int n:s.presentation.tone){need(n>=0&&n<=200,"Invalid saved screen tone");}
    if(direct){GameSession state(c.items,e,c.facts);state.finishSave(e.saveCount,s.continuation.statisticBefore);CombatScene candidate(c,state,restoredEntry(s),[](int lo,int){return lo;});candidate.restoreWarehouseState(s.warehouse);validateWarehouseVisual(c,s.presentation.warehouseVisual,s.presentation.music);}

}
namespace{
    SaveSnapshot adaptKnownSave(const CombatContent& target,const SaveSnapshot& source){
        if(source.contentCompatibility==target.compatibility){validateSave(target,source);return source;}
        if(!target.legacySource){throw IncompatibleSave("Unsupported save source");}
        // Only the fixed F3 -> F4-A -> B/C chain is registered by the content loader.
        auto result=adaptKnownSave(*target.legacySource,source);
        result.contentCompatibility=target.compatibility;
        result.continuation.version=saveVersion(target.compatibility)==3?2:1;
        // Older remake contracts never represented this work value. Zero is an
        // explicit compatibility initialization, not an original CE55 reset.
        if(target.facts.variables.contains(49)&&!target.legacySource->facts.variables.contains(49)){result.session.worldValues.emplace("combat.last-defense",0);}
        const auto sourceIds=target.legacySource->facts.flagIds();
        for(const auto& id:target.facts.flagIds()){if(!sourceIds.contains(id)){result.session.worldFlags.emplace(id,false);}}
        for(const auto& [id,rounds]:target.requiredMagazines){
            if(target.legacySource->requiredMagazines.contains(id)||target.legacySource->items.contains(id)){continue;}
            need(!result.session.magazines.contains(id),"Legacy source unexpectedly contains target magazine");
            result.session.magazines.emplace(id,rounds);
        }
        initializeFullSession(target,result.session);
        // Only explicitly reviewed additions may extend a verified older scene.
        // Existing saved actors keep their original motion and direction.
        if(target.catalog.contains("legacy_scene_objects")){
            for(const auto& value:target.catalog.at("legacy_scene_objects")){
                const auto id=value.get<std::string>();
                if(target.legacySource->objects.contains(id)){continue;}
                const auto& object=target.objects.at(id);
                if(object.map!=result.scene.map){continue;}
                need(!result.scene.positions.contains(id)&&!result.scene.directions.contains(id),"Legacy scene unexpectedly contains added actor");
                result.scene.positions.emplace(id,object.tile);result.scene.directions.emplace(id,directionFromName(object.variants.front().definition.direction));
            }
        }
        validateSave(target,result);return result;
    }
}
std::string encodeSave(const CombatContent& c,const SaveSnapshot& s){
    validateSave(c,s);Json objects=Json::array();for(const auto& [id,t]:s.scene.positions){objects.push_back({{"id",id},{"tile",{t.x,t.y}},{"direction",directionName(s.scene.directions.at(id))}});}
    const int version=s.continuation.kind=="clear_after_file"?6:s.continuation.kind=="direct_unscored"?5:4;const int contentVersion=saveVersion(c.compatibility);const bool full=contentVersion>=2;
    Json j={{"schema","nobihaza.save.v"+std::to_string(version)},{"version",version},{"content",s.contentCompatibility},{"build",s.build},{"session",sessionJson(s.session,full,s.continuation.kind=="chapter_handoff_after_file")},{"summary",{{"saved_at_unix",s.summary.savedAtUnix},{"location",s.summary.location},{"face",s.summary.face},{"face_index",s.summary.faceIndex},{"label",s.summary.label}}},
        {"scene",{{"map",s.scene.map},{"tile",{s.scene.playerTile.x,s.scene.playerTile.y}},{"direction",directionName(s.scene.direction)},{"sprite",s.scene.sprite},{"objects",objects}}},
        {"presentation",{{"music",{{"resource",s.presentation.music.resource},{"volume",s.presentation.music.volume},{"pitch",s.presentation.music.pitch},{"balance",s.presentation.music.balance}}},{"tone",s.presentation.tone},{"saturation",s.presentation.saturation}}},
        {"after_save",{{"version",s.continuation.version},{"point",s.continuation.point},{"count_before",s.continuation.countBefore},{"statistic_before",s.continuation.statisticBefore}}}};
    if(contentVersion==3){j["after_save"]["kind"]=s.continuation.kind;}
    if(version==6){j["clear_actor"]={{"actor",8},{"name",s.session.actor8Name}};}
    if(version==5){j["warehouse"]=s.warehouse;j["presentation"]["warehouse_visual"]=s.presentation.warehouseVisual;}
    auto bytes=j.dump(2);need(bytes.size()<=maxBytes,"Save exceeds 8 MiB");return bytes+"\n";
}
SaveSnapshot decodeSave(const CombatContent& c,std::string_view bytes){
    need(bytes.size()<=maxBytes,"Save exceeds 8 MiB");std::vector<std::set<std::string>> objectKeys;
    auto callback=[&](int depth,Json::parse_event_t event,Json& parsed){
        need(depth<=32,"Save nesting exceeds 32");
        if(event==Json::parse_event_t::object_start){objectKeys.emplace_back();}
        if(event==Json::parse_event_t::key){need(objectKeys.back().insert(parsed.get<std::string>()).second,"Duplicate JSON key");}
        if(event==Json::parse_event_t::object_end){objectKeys.pop_back();}return true;
    };
    const Json j=Json::parse(bytes.begin(),bytes.end(),callback);
    keys(j,{"schema","version","content","build","session","scene","presentation","after_save","summary"},j.value("version",0)==6?std::initializer_list<const char*>{"clear_actor"}:j.value("version",0)==5?std::initializer_list<const char*>{"warehouse"}:std::initializer_list<const char*>{});
    const int contentVersion=saveVersion(string(j.at("content")));const int version=number(j,"version");const bool full=contentVersion>=2;
    if((version!=6&&version!=5&&version!=4&&version!=contentVersion)||string(j.at("schema"))!=("nobihaza.save.v"+std::to_string(version))){throw IncompatibleSave("Incompatible save schema");}
    SaveSnapshot s;s.formatVersion=version;const auto& summary=j.at("summary");keys(summary,{"saved_at_unix","location","face","face_index","label"});
    need(summary.at("saved_at_unix").is_number_integer()&&!summary.at("saved_at_unix").is_number_float(),"Invalid save time");s.summary={summary.at("saved_at_unix").get<std::int64_t>(),string(summary.at("location")),string(summary.at("face")),number(summary,"face_index"),string(summary.at("label"))};
    s.contentCompatibility=string(j.at("content"));s.build=string(j.at("build"));s.session=readSession(j.at("session"),full,contentVersion==3&&j.at("after_save").value("kind",std::string{})=="chapter_handoff_after_file",version>=4);
    if(version==6){const auto& actor=j.at("clear_actor");keys(actor,{"actor","name"});need(number(actor,"actor")==8,"Unknown Clear actor");s.session.actor8Name=string(actor.at("name"));}
    const auto& scene=j.at("scene");keys(scene,{"map","tile","direction","sprite","objects"});s.scene.map=number(scene,"map");auto tile=ints(scene.at("tile"),2);s.scene.playerTile={tile[0],tile[1]};s.scene.direction=directionFromName(string(scene.at("direction")));s.scene.sprite=string(scene.at("sprite"));need(scene.at("objects").is_array(),"Save objects array expected");
    for(const auto& o:scene.at("objects")){keys(o,{"id","tile","direction"});auto id=string(o.at("id"));auto t=ints(o.at("tile"),2);need(s.scene.positions.emplace(id,TilePosition{t[0],t[1]}).second,"Duplicate scene object");s.scene.directions.emplace(id,directionFromName(string(o.at("direction"))));}
    const auto& p=j.at("presentation");keys(p,{"music","tone","saturation"},version==5?std::initializer_list<const char*>{"warehouse_visual"}:std::initializer_list<const char*>{});if(version==5){s.warehouse=j.at("warehouse");s.presentation.warehouseVisual=p.at("warehouse_visual");}s.presentation.saturation=number(p,"saturation");const auto& m=p.at("music");keys(m,{"resource","volume","pitch","balance"});s.presentation.music={string(m.at("resource")),number(m,"volume"),number(m,"pitch"),number(m,"balance")};auto tone=ints(p.at("tone"),3);std::copy(tone.begin(),tone.end(),s.presentation.tone.begin());
    const auto& a=j.at("after_save");keys(a,{"version","point","count_before","statistic_before"},contentVersion==3?std::initializer_list<const char*>{"kind"}:std::initializer_list<const char*>{});s.continuation={number(a,"version"),string(a.at("point")),number(a,"count_before"),number(a,"statistic_before"),contentVersion==3?string(a.at("kind")):"ordinary"};return adaptKnownSave(c,s);
}
void finishSaveContinuation(GameSession& session,const SaveSnapshot& s){
    const bool clear=s.continuation.kind=="clear_after_file",chapter=s.continuation.kind=="chapter_handoff_after_file",recovery=s.continuation.kind=="development_f5_recovery",direct=s.continuation.kind=="direct_unscored";session.finishSave(s.session.saveCount,s.continuation.statisticBefore+(clear||chapter||recovery||direct?0:1),!chapter&&!clear);
}
PreparedRestore prepareRestore(const CombatContent& c,const SaveSnapshot& input,CombatScene::PreparePresentation prepare,CombatScene::Random random){
    const auto s=adaptKnownSave(c,input);auto state=s.session;if(s.continuation.kind!="direct_unscored"&&state.quickshotTicks>0){state.quickshotTicks=(!state.commonEffectsKnown||state.quickshotActive)?c.skills.at(9).durationTicks:0;}
    PreparedRestore candidate;candidate.session=std::make_unique<GameSession>(c.items,state,c.facts);finishSaveContinuation(*candidate.session,s);
    candidate.scene=std::make_unique<CombatScene>(c,*candidate.session,restoredEntry(s),std::move(random),std::move(prepare));candidate.presentation=s.presentation;
    if(s.continuation.kind=="direct_unscored"){candidate.scene->restoreWarehouseState(s.warehouse);}
    if(s.continuation.kind=="clear_after_file"){candidate.scene->installClearContinuation(s.continuation.point);}
    if(s.continuation.kind=="chapter_handoff_after_file"){candidate.scene->installChapterContinuation(s.continuation.point);}
    return candidate;
}
