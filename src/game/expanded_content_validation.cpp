#include "expanded_content_validation.h"
#include <sstream>
#include <set>
#include <stdexcept>
#include <algorithm>
namespace{
using J=nlohmann::json;
void need(bool b,const char* m){if(!b){throw std::runtime_error(std::string("Expanded content: ")+m);}}
void shape(const J& v,const std::string& names){
    std::istringstream in(names);std::set<std::string> keys;std::string name;while(in>>name){keys.insert(name);}
    need(v.is_object()&&v.size()==keys.size(),"unknown or missing field");for(const auto& [k,x]:v.items()){need(keys.contains(k),"unknown field");}
}
int integer(const J& v,int low,int high){need(v.is_number_integer()&&v>=low&&v<=high,"integer out of range");return v.get<int>();}
void pair(const J& v,int low,int high){need(v.is_array()&&v.size()==2,"expected pair");for(const auto& n:v){integer(n,low,high);}}
void flag(const J& v){need(v.is_boolean(),"boolean expected");}
void array(const J& v){need(v.is_array()&&!v.empty()&&v.size()<=1000,"invalid sequence length");}
}
void validateExpandedContent(const CombatContent& c,const J& d){
 need(d.at("batch")=="A"||d.at("batch")=="B"||d.at("batch")=="C","unknown cumulative batch");
 shape(d.at("capabilities"),"weapons delivered_batch");
 shape(d.at("capabilities").at("weapons"),"14 21");
 need(d.at("capabilities").at("delivered_batch")==d.at("batch"),"capability batch mismatch");
 need(d.at("capabilities").at("weapons").at("14")== (d.at("batch")=="A"?"state-only":"executable")&&d.at("capabilities").at("weapons").at("21")=="executable","invalid weapon capability declaration");
 shape(d.at("legacy_source"),"file sha256 compatibility");
 for(const auto& animation:c.media.at("animations")){
  need(animation.contains("playback")&&animation.at("playback").at("logic_updates_per_frame")==2,"Missing or unsupported animation playback");
 }
 const auto& x=d.at("f4");
 const auto cue=[&](const J& id){need(id.is_string()&&c.cues.contains(id.get<std::string>()),"missing cue");};
 const auto page=[&](const J& v){
    const auto& object=c.objects.at(v.at("object").get<std::string>());int p=integer(v.at("page"),1,10000);
    need(std::any_of(object.variants.begin(),object.variants.end(),[&](const auto& candidate){return candidate.page==p;}),"missing event page");return object.map;
 };
 const auto placement=[&](const J& v,int map){
    const auto& object=c.objects.at(v.at("object").get<std::string>());need(object.map==map,"placement on wrong map");
    pair(v.at("tile"),0,10000);const auto& grid=c.maps.at(map);need(v.at("tile")[0]<grid.width&&v.at("tile")[1]<grid.height,"placement outside map");
 };
 const std::map<std::string,std::string> steps{{"grant","kind item count source"},{"remove","kind item count source"},{"flag","kind id value source"},{"message","kind message source"},{"cue","kind cue"},{"place_object","kind object tile source"},{"enemy_state","kind enemy state source"},{"cancel_player_actions","kind cleanup source"},{"place_attack_actor_at_player","kind source"},{"return_title_via_death","kind source"}};
 const std::map<std::string,std::set<std::string>> allowed{
    {"flows",{"grant","remove","flag","message","cue"}},
    {"encounters",{"flag","message","cue","place_object","enemy_state","cancel_player_actions"}},
    {"failure_flows",{"cue","place_attack_actor_at_player","return_title_via_death"}}};
 for(const std::string group:{"flows","encounters","failure_flows"}){
  need(x.at(group).is_object(),"flow group must be object");
  for(const auto& [id,f]:x.at(group).items()){
   shape(f,group=="flows"?"object page trigger steps source":group=="encounters"?"object page enemy trigger requires_fact_off steps source":"actor preserves_positive_hp steps source");
   int map=0;
   if(group=="failure_flows"){need(f.at("actor")==1&&f.at("preserves_positive_hp")==true,"unsupported fatal semantics");}
   else{map=page(f);need(f.at("trigger")==(group=="flows"?"action":"contact"),"unsupported trigger");}
   if(group=="encounters"){need(c.expansion.enemies.contains(f.at("enemy").get<std::string>()),"missing encounter enemy");integer(f.at("requires_fact_off"),1,10000);}
   array(f.at("steps"));
   for(const auto& s:f.at("steps")){
    const auto k=s.at("kind").get<std::string>();need(allowed.at(group).contains(k),"unsupported finite operation");shape(s,steps.at(k)+(k=="cue"&&s.contains("source")?" source":""));
    if(k=="cue"){cue(s.at("cue"));}
    else if(k=="message"){need(c.messages.contains(s.at("message").get<std::string>()),"missing flow message");}
    else if(k=="grant"||k=="remove"){need(c.items.contains(integer(s.at("item"),1,10000)),"missing inventory item");integer(s.at("count"),1,99);}
    else if(k=="flag"){integer(s.at("id"),1,10000);flag(s.at("value"));if(group=="encounters"){need(s.at("id")==1||s.at("id")==18||c.facts.switches.contains(s.at("id").get<int>()),"unknown encounter fact");}}
    else if(k=="place_object"){placement(s,map);}
    else if(k=="enemy_state"){
      need(s.at("enemy")==f.at("enemy"),"enemy state target mismatch");const auto& states=x.at("enemy_behaviors").at("licker").at("states");
      need(s.at("state")==states.at("pursuit")&&s.at("state")==2&&c.expansion.enemies.at(s.at("enemy").get<std::string>()).behavior==EnemyBehaviorKind::Licker&&c.expansion.enemies.at(s.at("enemy").get<std::string>()).map==map,"unsupported encounter activation");
    }else if(k=="cancel_player_actions"){need(s.at("cleanup")=="ce5","unknown action cleanup");}
   }
  }
 }
 for(const auto& p:x.at("save_points")){shape(p,"object page facing location slot_label source");page(p);directionFromName(p.at("facing").get<std::string>());need(!p.at("location").get<std::string>().empty()&&!p.at("slot_label").get<std::string>().empty(),"empty save label");}
 for(const auto& [mid,rows]:x.at("entry_placements").items()){need(rows.is_array(),"placements array expected");for(const auto& v:rows){shape(v,"object tile if_flag source");placement(v,std::stoi(mid));if(!v.at("if_flag").is_null()){integer(v.at("if_flag"),1,10000);}}}
 for(const auto& loop:x.at("environment")){
  shape(loop,"object page cue_order repeat source");page(loop);array(loop.at("cue_order"));need(loop.at("repeat")==true,"environment loop must repeat");
  for(const auto& id:loop.at("cue_order")){cue(id);const std::set<std::string> kinds{"wait","panorama","flash_screen","sound"};need(kinds.contains(c.cue(id.get<std::string>()).at("kind")),"unsupported environment cue");}
 }
 const auto& blood=x.at("blood_loop");shape(blood,"object page requires_control cue_order check_control_after source");page(blood);
 need(blood.at("requires_control")==true&&blood.at("check_control_after")==J::array({0,1,2,3,4})&&blood.at("cue_order").size()==6,"unsupported blood loop cancellation");
 for(const auto& id:blood.at("cue_order")){cue(id);const auto kind=c.cue(id.get<std::string>()).at("kind");need(kind=="animation"||kind=="wait","unsupported blood cue");}
 for(const auto& [mid,m]:x.at("map_music").items()){
  need(c.maps.contains(std::stoi(mid)),"unknown music map");const auto mode=m.at("mode").get<std::string>();need(mode=="play"||mode=="stop"||mode=="preserve-current","unknown music mode");
  shape(m,"mode lookup_chain raw_type resolved_type source compatibility"+std::string(mode=="play"?" sound fade_in_milliseconds":""));array(m.at("lookup_chain"));std::set<int> chain;
  for(const auto& id:m.at("lookup_chain")){need(chain.insert(integer(id,0,10000)).second,"cyclic music lookup");}integer(m.at("raw_type"),0,2);integer(m.at("resolved_type"),1,2);
  if(mode=="play"){shape(m.at("sound"),"resource volume_percent pitch_percent balance source");const auto s=readSound(m.at("sound"));need(c.media.at("audio").contains(s.resource),"unknown map music");integer(m.at("fade_in_milliseconds"),0,100000);}
 }
 for(const auto& e:x.at("enemies")){
  shape(e,"id map event source_order initial_tile behavior actor_stat_source normal_attack normal_defense dead_switch termination_damage_threshold hitbox_half_size_pixels source_slots activation_fact drop return_behavior source");
  need(c.media.at("actors").contains(std::to_string(integer(e.at("actor_stat_source"),1,10000))),"unknown enemy actor");pair(e.at("initial_tile"),0,10000);const auto& map=c.maps.at(e.at("map").get<int>());need(e.at("initial_tile")[0]<map.width&&e.at("initial_tile")[1]<map.height,"enemy outside map");
  shape(e.at("source_slots"),"base damage event_id state");for(const auto& v:e.at("source_slots")){integer(v,1,10000);}need(e.at("drop").is_null()&&e.at("return_behavior")=="reset-live-temporary-state; dead-event-at-origin","unknown enemy return/drop policy");
 }
 const auto& b=x.at("enemy_behaviors");shape(b,"licker running-zombie falling-zombie soldier-zombie");const auto& l=b.at("licker");
 shape(l,"states receive_state_less_than rage_damage_threshold damage_sources tongue_range_tiles ceiling_drop_radius_tiles grapple receivers runtime_profile source");
 const auto& profile=l.at("runtime_profile");shape(profile,"cues pages attack_switch grab_pose blood_effects enemy_blood_effects helper_sounds source");
 shape(profile.at("helper_sounds"),"ceiling grab_voice");for(const auto& id:profile.at("helper_sounds")){need(c.cue(id.get<std::string>()).at("kind")=="sound","Invalid helper sound");}
 shape(profile.at("enemy_blood_effects"),"11 12");for(const auto& effect:profile.at("enemy_blood_effects")){need(c.media.at("effects").contains(effect.get<std::string>()),"Missing enemy blood effect");}
 shape(profile.at("blood_effects"),"1 2 11");for(const auto& effect:profile.at("blood_effects")){need(c.media.at("effects").contains(effect.get<std::string>()),"Missing blood effect");}
 integer(profile.at("attack_switch"),1,10000);need(profile.at("grab_pose").is_array()&&profile.at("grab_pose").size()==2,"Invalid grab pose");
 for(const auto& cue:profile.at("grab_pose")){need(c.cues.contains(cue.get<std::string>()),"Missing grab pose");}
 shape(profile.at("pages"),"wander stalk pursuit tongue claw pounce find_wall to_wall ceiling climb grab drop down ending corpse");
 for(const auto& p:profile.at("pages")){integer(p,1,99);}
 for(const auto& [role,id]:profile.at("cues").items()){need(c.cues.contains(id.get<std::string>()),"Missing licker media");}

 shape(l.at("states"),"wander stalk pursuit claw pounce find_wall to_wall ceiling tongue_grab drop knocked_down");std::set<int> states;
 for(const auto& v:l.at("states")){need(states.insert(integer(v,0,10000)).second,"duplicate enemy state");}integer(l.at("receive_state_less_than"),1,10000);integer(l.at("rage_damage_threshold"),1,999999);
 shape(l.at("damage_sources"),"tongue claw pounce");for(const std::string name:{"tongue","claw","pounce"}){const std::string field=name=="tongue"?"attack_add":"attack_multiplier";shape(l.at("damage_sources").at(name),field);integer(l.at("damage_sources").at(name).at(field),0,9999);}
 array(l.at("tongue_range_tiles"));need(l.at("tongue_range_tiles").size()<=10,"tongue range too long");int previous=0;for(const auto& v:l.at("tongue_range_tiles")){previous=integer(v,previous+1,100);}pair(l.at("ceiling_drop_radius_tiles"),0,100);
 const auto& g=l.at("grapple");shape(g,"period_tenths failure_cycles damages_each_cycle escape_thresholds failure_flow");integer(g.at("period_tenths"),1,10000);integer(g.at("failure_cycles"),1,10000);
 need(g.at("damages_each_cycle")==false&&x.at("failure_flows").contains(g.at("failure_flow").get<std::string>()),"invalid grapple behavior");need(g.at("escape_thresholds").size()==3,"invalid escape thresholds");previous=0;int index=0;
 for(const auto& t:g.at("escape_thresholds")){shape(t,"hp_at_most input_sum");integer(t.at("input_sum"),1,10000);if(++index==3){need(t.at("hp_at_most").is_null(),"missing default threshold");}else{previous=integer(t.at("hp_at_most"),previous+1,99999);}}
 shape(l.at("receivers"),"1 2 3 4 5");for(const auto& [id,r]:l.at("receivers").items()){
  shape(r,"damage retreat_tiles"+std::string(id!="1"?" critical":"")+((id=="4"||id=="5")?" next_state":""));flag(r.at("damage"));integer(r.at("retreat_tiles"),0,100);
  if(r.contains("critical")){need(r.at("critical")=="double","unknown critical rule");}if(r.contains("next_state")){need(states.contains(integer(r.at("next_state"),0,10000)),"unknown response state");}
 }
 const auto& falling=b.at("falling-zombie");shape(falling,"base_behavior fall_damage_threshold fall_once_per_visit fall_state fall_wait_tenths next_state grounded_receiver_2_3_requires_auxiliary source runtime_profile");
 for(const auto* k:{"fall_damage_threshold","fall_state","fall_wait_tenths","next_state"}){integer(falling.at(k),1,10000);}need(falling.at("fall_once_per_visit")==true&&falling.at("grounded_receiver_2_3_requires_auxiliary")==true,"unsupported falling policy");
 const auto& running=b.at("running-zombie");shape(running,"base_behavior approach_page approach_speed source runtime_profile");integer(running.at("approach_page"),1,10000);integer(running.at("approach_speed"),1,6);
 const auto& soldier=b.at("soldier-zombie");shape(soldier,"base_behavior receiver_2_interrupts_attack source runtime_profile");need(soldier.at("receiver_2_interrupts_attack")==true,"unsupported soldier response");
 for(const auto* name:{"running-zombie","falling-zombie","soldier-zombie"}){need(b.at(name).at("base_behavior")=="zombie","unknown behavior base");}

 for(const std::string name:{"running-zombie","falling-zombie","soldier-zombie"}){
  const auto& p=b.at(name).at("runtime_profile");const bool rush=name=="running-zombie",fall=name=="falling-zombie";
  shape(p,"blood_effects pursuit_page retreat_page normal_death_page head_page body_page sprites cues grab_release_wait retreat_routes retreat_flash collision_yield_routes feeding normal_death_wait_tenths head_animation head_delay head_final_route body_start_route body_wait body_final_route evade_routes source "+std::string(rush?"rush":fall?"fall":"receiver_2_interrupts_attack"));
  shape(p.at("blood_effects"),"1 2 3 11 12");for(const auto& effect:p.at("blood_effects")){need(c.media.at("effects").contains(effect.get<std::string>()),"missing zombie blood effect");}
  shape(p.at("sprites"),"idle windup1 windup2 windup3 grab player_grab recover normal_end head_transition");
  for(const auto& sprite:p.at("sprites")){need(c.maps.begin()->second.sprites->contains(sprite.get<std::string>()),"missing zombie sprite");}
  const auto typedCue=[&](const J& id,const std::string& kind){cue(id);need(c.cue(id.get<std::string>()).at("kind")==kind,"wrong zombie cue kind");};
  shape(p.at("cues"),"approach_bite windup_sound bite_sound grab_sound normal_hit_route knockback_route large_knockback_route hit_sound normal_death_sound"+std::string(rush?"":" approach_grab"));
  for(const auto& [role,id]:p.at("cues").items()){typedCue(id,role.ends_with("sound")?"sound":"forced_route");}
  for(const auto* group:{"retreat_routes","collision_yield_routes","evade_routes"}){shape(p.at(group),"up right down left");for(const auto& id:p.at(group)){typedCue(id,"forced_route");}}
  for(const auto* key:{"pursuit_page","retreat_page","normal_death_page","head_page","body_page","normal_death_wait_tenths"}){integer(p.at(key),1,10000);}
  for(const auto* key:{"grab_release_wait","head_delay","body_wait"}){typedCue(p.at(key),"wait");}
  for(const auto* key:{"head_final_route","body_start_route","body_final_route"}){typedCue(p.at(key),"forced_route");}
  typedCue(p.at("retreat_flash"),"flash_character");typedCue(p.at("head_animation"),"animation");
  need(p.at("feeding").size()==4,"invalid zombie feeding stages");for(const auto& row:p.at("feeding")){shape(row,"route wait_tenths"+std::string(row.contains("sound")?" sound":""));typedCue(row.at("route"),"forced_route");integer(row.at("wait_tenths"),0,10000);if(row.contains("sound")){typedCue(row.at("sound"),"sound");}}
  if(rush){const auto& r=p.at("rush");shape(r,"page trigger_front_pixels trigger_side_pixels trigger_sounds acquire_wait front_probe_offsets geometry_half_extent grab_player_wait");integer(r.at("page"),1,10000);integer(r.at("geometry_half_extent"),1,100);pair(r.at("trigger_front_pixels"),0,1000);pair(r.at("trigger_side_pixels"),-1000,1000);shape(r.at("trigger_sounds"),"up right down left");shape(r.at("front_probe_offsets"),"up right down left");for(const auto& id:r.at("trigger_sounds")){typedCue(id,"sound");}for(const auto& offset:r.at("front_probe_offsets")){pair(offset,-100,100);}typedCue(r.at("acquire_wait"),"wait");typedCue(r.at("grab_player_wait"),"forced_route");}
  if(fall){const auto& f=p.at("fall");shape(f,"page state once_variable damage_threshold start_sound wait rise_sound next_state grounded_hit_route grounded_knockback_route grounded_blood_types receiver23_requires_auxiliary");for(const auto* key:{"page","state","once_variable","damage_threshold","next_state"}){integer(f.at(key),1,10000);}typedCue(f.at("start_sound"),"sound");typedCue(f.at("rise_sound"),"sound");typedCue(f.at("wait"),"wait");typedCue(f.at("grounded_hit_route"),"forced_route");typedCue(f.at("grounded_knockback_route"),"forced_route");need(f.at("grounded_blood_types")==J::array({11,12})&&f.at("receiver23_requires_auxiliary")==true,"unsupported fallen response");}
  if(!rush&&!fall){need(p.at("receiver_2_interrupts_attack")==true,"unsupported soldier interruption");}
  const auto behavior=rush?EnemyBehaviorKind::RunningZombie:fall?EnemyBehaviorKind::FallingZombie:EnemyBehaviorKind::SoldierZombie;
  for(const auto& [id,enemy]:c.expansion.enemies){if(enemy.behavior!=behavior){continue;}
   const auto exists=[&](int n){const auto& pages=c.objects.at(id).variants;need(std::any_of(pages.begin(),pages.end(),[&](const auto& page){return page.page==n;}),"missing zombie role page");};
   for(const auto* key:{"pursuit_page","retreat_page","normal_death_page","head_page","body_page"}){exists(p.at(key));}
   if(rush){exists(p.at("rush").at("page"));}if(fall){exists(p.at("fall").at("page"));}
  }
 }
 std::set<std::string> expectedAttacks;
 for(const auto& [id,b]:c.catalog.at("weapon_bindings").items()){if(b.at("kind")=="de"||b.at("kind")=="shotgun"){expectedAttacks.insert(id);}}
 need(x.at("attacks").is_object()&&x.at("attacks").size()==expectedAttacks.size(),"missing typed weapon attack");
 for(const auto& id:expectedAttacks){need(x.at("attacks").contains(id),"missing typed weapon attack");}
 for(const auto& [id,a]:x.at("attacks").items()){
  const auto& bindings=c.catalog.at("weapon_bindings");
  need(bindings.contains(id),"attack has no weapon binding");
  const std::string kind=bindings.at(id).at("kind");
  need((kind=="de"&&a.at("strategy")=="magnum-flight")||(kind=="shotgun"&&a.at("strategy")=="shotgun-distance"),"weapon binding and attack strategy differ");
  if(a.at("strategy")=="shotgun-distance"){
   shape(a,"strategy muzzle_test muzzle_forward_px muzzle_half_px flight_motion forward_sample_offset_px flight_half_vertical_px flight_half_horizontal_px response_by_advance end_on_hit_from_advance wall_terrain source");
   need(a.at("muzzle_test")==true&&a.at("muzzle_forward_px")==J({{"up",8},{"right",8},{"down",16},{"left",8}}),"unsupported shotgun muzzle");
   const auto& motion=a.at("flight_motion");shape(motion,"original_forward_steps original_speed original_frequency compatibility_sample_lead_px compatibility_counter_step source");
   need(motion.at("original_forward_steps")==8&&motion.at("original_speed")==6&&motion.at("original_frequency")==8&&motion.at("compatibility_sample_lead_px")==8&&motion.at("compatibility_counter_step")==1,"unsupported shotgun motion interpretation");
   need(a.at("forward_sample_offset_px")==16,"unsupported shotgun flight offset");
   need(a.at("muzzle_half_px")==J::array({1,1})&&a.at("flight_half_vertical_px")==J::array({15,5})&&a.at("flight_half_horizontal_px")==J::array({5,15}),"unsupported shotgun geometry");
   need(a.at("response_by_advance")==J::array({5,4,4,3})&&a.at("end_on_hit_from_advance")==2&&a.at("wall_terrain")==3,"unsupported shotgun response");continue;
  }
  shape(a,"strategy muzzle_test projectile_half_px forward_sample_offset_px response_page end_on_target wall_terrain source");
  need(a.at("muzzle_test")==false&&a.at("end_on_target")==false&&a.at("response_page")==5,"unsupported DE response");integer(a.at("wall_terrain"),1,10000);
 }
 for(const auto& [id,r]:x.at("reload_equipment").items()){
  shape(r,"equipment_item profile source available_from_batch");need(c.items.contains(std::stoi(id))&&c.actionProfiles.contains(r.at("profile").get<std::string>()),"missing reload reference");need(r.at("equipment_item")==51&&r.at("available_from_batch")=="B","unknown reload equipment policy");
 }
 if(d.at("batch")!="A"){
  const auto steps=[&](const J& values){
   array(values);
   const std::map<std::string,std::string> fields{{"flag","id value"},{"grant","item count"},{"remove","item count"},{"learn_skill","skill"},{"forget_skill","skill"},{"message","message"},{"cue","cue"},{"system_graphic","image"}};
   for(const auto& step:values){
    const auto kind=step.at("kind").get<std::string>();need(fields.contains(kind),"unknown staged operation");shape(step,"kind "+fields.at(kind)+" source");
    if(kind=="flag"){integer(step.at("id"),1,10000);flag(step.at("value"));}
    else if(kind=="grant"||kind=="remove"){need(c.items.contains(integer(step.at("item"),1,10000)),"missing staged item");integer(step.at("count"),1,99);}
    else if(kind=="learn_skill"||kind=="forget_skill"){need(c.skills.contains(integer(step.at("skill"),1,10000)),"missing staged skill");}
    else if(kind=="message"){need(c.messages.contains(step.at("message").get<std::string>()),"missing staged message");}
    else if(kind=="cue"){cue(step.at("cue"));}
    else{need(c.media.at("images").contains(step.at("image").get<std::string>()),"missing staged system image");}
   }
  };
  for(const auto& [id,f]:x.at("staged_flows").items()){
   shape(f,"object page implementation_ticket steps source");page(f);int t=integer(f.at("implementation_ticket"),1,1000);need(t==57||t==58||t==61,"unknown staged consumer");steps(f.at("steps"));
  }
  const auto& tool=x.at("tool");shape(tool,"inactive_item active_item request_flag area_flag removed_flag ladder_object vent_object active_area return_areas discard_choice discard_message_steps source");
  for(const auto* k:{"inactive_item","active_item"}){need(c.items.contains(integer(tool.at(k),1,10000)),"missing tool item");}
  for(const auto* k:{"request_flag","area_flag","removed_flag"}){integer(tool.at(k),1,10000);}
  for(const auto* k:{"ladder_object","vent_object","active_area"}){need(c.objects.contains(tool.at(k).get<std::string>()),"missing tool area");}
  array(tool.at("return_areas"));for(const auto& id:tool.at("return_areas")){need(c.objects.contains(id.get<std::string>()),"missing tool return area");}
  const int toolMap=c.objects.at(tool.at("vent_object").get<std::string>()).map;
  for(const auto* k:{"ladder_object","active_area"}){need(c.objects.at(tool.at(k).get<std::string>()).map==toolMap,"tool object on wrong map");}
  std::set<std::string> returns;
  for(const auto& id:tool.at("return_areas")){need(c.objects.at(id.get<std::string>()).map==toolMap&&returns.insert(id.get<std::string>()).second,"invalid tool return area relationship");}
  const auto& ladders=c.catalog.at("ladders");
  need(std::count_if(ladders.begin(),ladders.end(),[&](const J& l){return l.at("object")==tool.at("ladder_object");})==1,"tool requires a registered ladder");
  const auto& removal=x.at("staged_flows").at("vent-remove");
  need(removal.at("object")==tool.at("vent_object")&&removal.at("implementation_ticket")==58,"tool removal target differs");
  const auto& vent=c.objects.at(removal.at("object").get<std::string>());
  const auto removalPage=std::find_if(vent.variants.begin(),vent.variants.end(),[&](const auto& v){return v.page==removal.at("page");});
  need(removalPage!=vent.variants.end()&&removalPage->flags.contains(tool.at("area_flag"))&&removalPage->flags.contains(tool.at("request_flag")),"tool removal page lacks request conditions");
  cue(tool.at("discard_choice"));need(c.cue(tool.at("discard_choice").get<std::string>()).at("kind")=="choices","invalid discard choice");steps(tool.at("discard_message_steps"));
  const auto& mix=x.at("mixing");shape(mix,"enabled_skill disabled_skill request_flag ingredients result begin question choice success missing before_menu finish source");
  for(const auto* k:{"enabled_skill","disabled_skill"}){need(c.skills.contains(integer(mix.at(k),1,10000)),"missing mixing skill");}
  integer(mix.at("request_flag"),1,10000);need(mix.at("ingredients").size()==2,"unsupported ingredient count");std::set<int> ingredients;
  const auto item=[&](const J& v){shape(v,"item count");int id=integer(v.at("item"),1,10000);need(c.items.contains(id),"missing recipe item");integer(v.at("count"),1,99);return id;};
  for(const auto& v:mix.at("ingredients")){need(ingredients.insert(item(v)).second,"duplicate ingredient");}item(mix.at("result"));
  cue(mix.at("choice"));need(c.cue(mix.at("choice").get<std::string>()).at("kind")=="choices","invalid recipe choice");
  const auto& enabled=c.skills.at(mix.at("enabled_skill").get<int>());const auto& disabled=c.skills.at(mix.at("disabled_skill").get<int>());
  need(enabled.id!=disabled.id&&enabled.type==3&&enabled.fieldUse&&enabled.tpCost==0&&!disabled.fieldUse,"unsupported mixing skill capabilities");
  for(const auto* k:{"begin","question","success","missing","before_menu","finish"}){
   steps(mix.at(k));
   const std::string segment=k;
   for(const auto& step:mix.at(k)){
    const auto kind=step.at("kind").get<std::string>();
    if(segment=="before_menu"||segment=="finish"){need(kind=="flag","mixing menu cleanup must be synchronous flags");}
    else{need(kind=="flag"||kind=="cue"||kind=="message","unsupported mixing operation");}
    if(kind=="cue"){const std::string type=c.cue(step.at("cue").get<std::string>()).at("kind");need(type=="sound"||type=="face"||type=="message_options","unsupported mixing presentation cue");}
   }
  }
  const auto& m=x.at("meeting");shape(m,"entry_object entry_page entered_flag talk_flags meeting_trigger meeting_page optional_weapon_object optional_weapon_flag source");
  for(const auto* k:{"entry_object","meeting_trigger","optional_weapon_object"}){need(c.objects.contains(m.at(k).get<std::string>()),"missing meeting actor");}
  for(const auto* k:{"entry_page","entered_flag","meeting_page","optional_weapon_flag"}){integer(m.at(k),1,10000);}
  page(J{{"object",m.at("entry_object")},{"page",m.at("entry_page")}});
  page(J{{"object",m.at("meeting_trigger")},{"page",m.at("meeting_page")}});
  need(m.at("talk_flags").is_object()&&m.at("talk_flags").size()==8,"missing independent talk flags");std::set<int> talkFlags;
  for(const auto& [id,v]:m.at("talk_flags").items()){need(c.objects.contains("m24-e"+id)&&talkFlags.insert(integer(v,1,10000)).second,"invalid talk flag");}
  if(d.at("batch")=="C"){
   const auto& story=x.at("meeting_story");shape(story,"version conference escape return refusals safe_room_skill source");need(story.at("version")==1,"unknown meeting story version");
   const auto storyPage=[&](const J& v){page(v);need(c.objects.at(v.at("object").get<std::string>()).map==integer(v.at("map"),1,10000),"story actor map mismatch");};
   const auto storySteps=[&](const J& values){steps(values);for(const auto& op:values){const auto k=op.at("kind");need(k=="flag"||k=="grant"||k=="message"||k=="cue","unsupported story sequence operation");}};
   const auto transfer=[&](const J& v){shape(v,"map tile facing room_reset source");const int map=integer(v.at("map"),1,10000);need(c.maps.contains(map),"missing story destination");const auto& dest=c.maps.at(map);const auto& xy=v.at("tile");need(xy.is_array()&&xy.size()==2,"invalid story tile");integer(xy[0],0,dest.width-1);integer(xy[1],0,dest.height-1);need(v.at("facing")=="preserve"&&v.at("room_reset")==false,"unsupported story transfer");};
   const auto& safe=story.at("safe_room_skill");shape(safe,"map object page learn forget source");storyPage(safe);
   need(c.skills.contains(integer(safe.at("learn"),1,10000))&&c.skills.contains(integer(safe.at("forget"),1,10000))&&safe.at("learn")!=safe.at("forget"),"invalid safe room skill conversion");
   const auto& conf=story.at("conference");shape(conf,"map object page before_facing facing_branches after_facing transfer after_transfer source");storyPage(conf);
   for(const auto* k:{"before_facing","after_facing","after_transfer"}){storySteps(conf.at(k));}transfer(conf.at("transfer"));
   need(conf.at("facing_branches").is_array()&&conf.at("facing_branches").size()==4,"invalid story direction branches");
   for(const auto& branch:conf.at("facing_branches")){shape(branch,"direction steps source");directionFromName(branch.at("direction").get<std::string>());storySteps(branch.at("steps"));}
   const auto& escape=story.at("escape");shape(escape,"map object page steps skill_conversion transfer source");storyPage(escape);storySteps(escape.at("steps"));transfer(escape.at("transfer"));
   const auto& conversion=escape.at("skill_conversion");shape(conversion,"if_known learn forget source");for(const auto* k:{"if_known","learn","forget"}){need(c.skills.contains(integer(conversion.at(k),1,10000)),"missing story skill");}need(conversion.at("if_known")==conversion.at("forget")&&conversion.at("learn")!=conversion.at("forget"),"invalid conditional skill conversion");
   const auto& ret=story.at("return");shape(ret,"map object page steps completed_flag source");storyPage(ret);storySteps(ret.at("steps"));integer(ret.at("completed_flag"),1,10000);
   need(conf.at("transfer").at("map")==escape.at("map")&&escape.at("transfer").at("map")==ret.at("map"),"disconnected story stages");
   array(story.at("refusals"));for(const auto& refusal:story.at("refusals")){shape(refusal,"object page steps source");page(refusal);need(c.objects.at(refusal.at("object").get<std::string>()).map==ret.at("map"),"refusal map mismatch");storySteps(refusal.at("steps"));}
  }

 }

}
