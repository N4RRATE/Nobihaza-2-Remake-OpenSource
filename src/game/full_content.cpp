#include "full_content.h"
#include "content_hash.h"
#include "branch_content.h"
#include "guard_controller.h"
#include "escort_controller.h"
#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>
using Json=nlohmann::json;
namespace{
    const std::array<std::string,5> stages{"training","underground","parking","truth","evacuation"};
    void need(bool condition,const char* message){if(!condition){throw std::runtime_error(message);}}
    int stageIndex(const std::string& stage){
        const auto found=std::find(stages.begin(),stages.end(),stage);
        need(found!=stages.end(),"Unknown full F4 stage");return static_cast<int>(found-stages.begin());
    }
    Json reviewedTrainingProjection(const Json& data,const Json& original){
        Json delta=Json::object();
        for(const auto* group:{"maps","objects","sprites","images","audio","asset_names","animations","messages","cues","transitions"}){
            delta[group]=Json::object();
            for(const auto& [key,value]:data.at(group).items()){
                if(!original.at(group).contains(key)){delta[group][key]=value;}
            }
            for(const auto& [key,value]:original.at(group).items()){
                auto expected=value;
                if(std::string(group)=="transitions"&&key=="m20-e3-p4"){expected["handoff_only"]=false;expected["runtime_enabled"]=false;}
                if(std::string(group)=="objects"&&data.at(group).at(key)!=expected){
                    const auto& extended=data.at(group).at(key);auto preserved=extended;preserved["variants"]=value.at("variants");
                    need(preserved==value,"Inherited object metadata differs");
                    for(const auto& page:value.at("variants")){
                        const auto& pages=extended.at("variants");const auto found=std::find_if(pages.begin(),pages.end(),[&](const auto& candidate){return candidate.at("original_page")==page.at("original_page");});
                        need(found!=pages.end()&&*found==page,"Inherited object page differs");
                    }
                    delta["object_extensions"][key]=extended;continue;
                }
                need(data.at(group).at(key)==expected,"Inherited training content differs");
            }
        }
        delta["page_presentations"]=Json::object();
        for(const auto& [key,value]:data.at("f3").at("page_presentations").items()){
            if(!original.at("f3").at("page_presentations").contains(key)){delta["page_presentations"][key]=value;}
        }
        auto catalog=original.at("f3");
        for(const auto& [key,value]:delta.at("page_presentations").items()){catalog["page_presentations"][key]=value;}
        need(data.at("f3")==catalog,"Inherited training catalog differs");
        delta["runtime"]=data.at("f4full").at("runtime");delta["catalog"]=data.at("f4full").at("catalog");
        if(delta.at("runtime").contains("door_closure")){
            const auto hash=contentSha256(delta.at("runtime").at("door_closure").dump());
            need(hash=="e467b797ba0b1b9c8e4614987b58563b6e2a11f5bd3fc3f1fc08fd4be06b09ba"||hash=="df4d2ad9e0c45278241af1ea182dd71d5b61b3caca456580dc9d4a0536f6af17","Unreviewed original door closure");
            delta["runtime"].erase("door_closure");
        }
        if(delta.at("runtime").contains("dormant_licker")){
            need(contentSha256(delta.at("runtime").at("dormant_licker").dump())=="30f941ccef527231ac357c2e6368a3f3a61109b76191b7fce29879e377bd7947","Unreviewed dormant licker binding");
            delta["runtime"].erase("dormant_licker");
        }
        if(delta.at("runtime").contains("hot_water")){
            need(contentSha256(delta.at("runtime").at("hot_water").dump())=="a8baa11bf03bdb5b196f7b8872a19776a1502e66b2245d78628c8a59aaa88094","Unreviewed hot-water enemy roles");
            delta["runtime"].erase("hot_water");
        }
        const auto hash=contentSha256(delta.dump());
        const bool statusSupplement=hash=="4019890c20cd167bb00316e2fdbe2f8613b0f20373f43920dd48a2030ea2a145"||hash=="db5da4045349c353667ee7fe9ac1354d338d5bca6c22786aa5d24bacebfa2373"||hash=="9af851b61f121af35350da48e410c626b340d887497f90b37cc6f2e01325d0e5"||hash=="7aec13f71a5eda952057ed0eba9dc0ce1fb0a761b3f881b6645072a9cd905357"||hash=="f11de25a6b7827da746b6a5c9fea8b4ae4c9bbf77d087319cba24d9719be3721"||hash=="26318eef00a8ec2f3fe7dccc5d25b64ca057a15a345b8f6f8b1deb938c05c1c2"||hash=="9d8f901d868928cfb8f469ac049f667eaefec4ffbd61f91ccddbdbc6c6b2031a"||hash=="0cdb9420904b1ffd65f5fc092ec790548ab8e18b08af6efebf69bc9ec8aeb4b4"||hash=="ebbe6d861eacb2e3e1237b1fe69012b0f9f46d307cb24fa0e15f0c2e72ebf53c"||hash=="e45a2fb4cbb94f579c257a9d80bb7c8bca820fc1db903dddfb6a33364ec5b702"||hash=="712154b7d6759c5166f0ccbe9f1e8d4b731466957ca62f83565c7b80a140a614"||hash=="b9e46d34255a2b0da23784be148c76d04edd2c1760bcedab3cf6157c58f2d564"||hash=="a6fe422e3cd4197b2071f3c220be8240928caf654da2671a0b0d7dce8ac40cf4"||hash=="8644de38e6420aa1dfb5d9ffbcddaf15836100714bd0abe43dda35c4fe6e066a";
        need(hash=="20a390b1f73d0ff13dbdd6eac0e108e480c4c262ba6ecbc19a121481a1b45f18"||hash=="96ff20abca302e2575d5f8b733f3064b1f99c0c756d4d9d3601b195cdce03fa3"||hash=="29048df722cadc662d784e4691123dfadb87c9ea8b81087198b7167670268e4b"||hash=="7acbaf62f55b07c7b2315874b4d7a97a6eaddcd19ec1b7dd2557ff54cfedea07"||hash=="0cd37698bc6ec7bbd0c1e9b0e075a10d9df8f2c1e6f186aad0ced38012c92801"||hash=="02f5a2a0071a8069567eb4ccbc0c34d81e91452fda4219ba008ae45a924f2718"||hash=="e6eb140f45cef6c5974f7cdfe04ebb7922e7bb0e06ed3870f90f25582aa7a290"||hash=="0301ae32c78a107847ec3a6f1ce8b43d170f03b496f52cc1c2735d8f2e8b9a3b"||statusSupplement,"Unreviewed training projection");
        const bool completeTraining=delta.at("runtime").contains("player_recovery");
        const auto expectedState=statusSupplement?"7b61a692af846adb32f3766041dd415f4bedb8f1fe5e7ff2dd1342fbb20d2788":completeTraining?"73629ccffc177967628986a850cf86fddb8e922c95833161f7a054cdf600b49b":"d4f3a6f31d266e3f08ae1b4e18fab4eea120c224707ea50e74a37234005d3c99";
        need(contentSha256(data.at("f4full").at("state_contract").dump())==expectedState,"Unreviewed full F4 state declaration");
        return delta;
    }
    void appendTrainingProjection(CombatContent& content,const Json& data,const Json& delta){
        for(const auto* group:{"images","audio","animations","cues"}){content.replaceMediaDefinitions(group,data.at(group));}
        auto objects=delta.at("objects");
        if(delta.contains("object_extensions")){for(const auto& [id,value]:delta.at("object_extensions").items()){content.objects.erase(id);objects[id]=value;}}
        const auto sprites=readSceneDefinitions(content,Json{{"font",data.at("font")},{"sprites",data.at("sprites")},{"maps",delta.at("maps")},{"objects",objects}});
        const auto sharedSprites=std::make_shared<const SpriteDefinitions>(sprites);for(auto& [id,map]:content.maps){map.sprites=sharedSprites;}
        for(const auto& [id,value]:delta.at("messages").items()){content.messages.emplace(id,readCombatMessage(value,true));}
        for(const auto& [id,value]:delta.at("cues").items()){
            if(value.at("kind")=="forced_route"&&!value.contains("runtime_support")){content.routes.emplace(id,readRouteDefinition(value));}
        }
        for(const auto& [id,value]:delta.at("transitions").items()){content.transitions[id]=value;}
        content.transitions["m20-e3-p4"]=data.at("transitions").at("m20-e3-p4");
        content.catalog["page_presentations"]=data.at("f3").at("page_presentations");
        for(const auto& [map,music]:delta.at("runtime").at("map_music").items()){content.expansion.mapMusic[map]=music;}
        if(delta.at("runtime").contains("readings")){for(const auto& [id,reading]:delta.at("runtime").at("readings").items()){need(content.catalog["readings"].emplace(id,reading).second,"Duplicate full F4 reading");}}
        for(const auto& [id,map]:delta.at("maps").items()){
            const auto& runtime=delta.at("runtime");
            int ticket=93;
            if(runtime.contains("map_implementation_tickets")){
                const Json* tickets=&runtime.at("map_implementation_tickets");
                for(const auto* stage:{"underground","parking","truth","evacuation"}){
                    if(!tickets->contains(id)&&runtime.contains(stage)&&runtime.at(stage).at("map_implementation_tickets").contains(id)){tickets=&runtime.at(stage).at("map_implementation_tickets");}
                }
                ticket=tickets->at(id);
            }
            content.pendingMapTickets.emplace(std::stoi(id),ticket);
        }
        for(const auto& [id,flow]:delta.at("runtime").at("flows").items()){
            auto gated=flow;gated["runtime_enabled"]=false;gated["implementation_ticket"]=flow.value("implementation_ticket",93);content.catalog["flows"][id]=std::move(gated);
        }
        for(const auto& [id,row]:delta.at("runtime").at("encounters").items()){
            need(row.at("type")=="pursuer"&&id==row.at("object").get<std::string>(),"Unsupported full F4 encounter binding");
            auto rules=row.at("rules");rules["completion_flow"]=row.at("completion_flow");
            const int map=row.at("map"),order=row.at("source_order"),flag=row.at("completion_flag");
            need(content.objects.at(id).map==map&&content.facts.switches.contains(flag)&&content.catalog.at("flows").at(rules.at("completion_flow").get<std::string>()).at("object")==id,"Full F4 encounter has missing source binding");
            need(content.enemies.emplace(id,EnemyDefinition{id,map,order,flag,false,std::move(rules)}).second,"Duplicate full F4 encounter");
            content.pendingMapTickets.emplace(map,93);
        }
        for(const auto* stage:{"underground","parking","truth","evacuation"}){
            if(!delta.at("runtime").contains(stage)){continue;}
            const auto& extension=delta.at("runtime").at(stage);
            for(const auto& [map,music]:extension.at("map_music").items()){content.expansion.mapMusic[map]=music;}
            for(const auto& [id,flow]:extension.at("flows").items()){
                need(!flow.at("runtime_enabled").get<bool>()&&content.objects.contains(flow.at("object").get<std::string>()),"Later-stage flow is unsupported or unbound");
                if(content.catalog["flows"].contains(id)){
                    // The reviewed cumulative projection repeats B/C pages with new cue aliases; keep their delivered behavior.
                    const auto& inherited=content.legacySource->catalog.at("flows");
                    need(inherited.contains(id)&&content.catalog["flows"].at(id)==inherited.at(id)&&inherited.at(id).at("object")==flow.at("object")&&inherited.at(id).at("page")==flow.at("page"),"Conflicting later-stage flow");
                    continue;
                }
                need(content.catalog["flows"].emplace(id,flow).second,"Duplicate later-stage flow");
            }
            if(extension.contains("encounters")){for(const auto& [id,row]:extension.at("encounters").items()){
                need(!row.at("runtime_enabled").get<bool>()&&id==row.at("object").get<std::string>()&&content.objects.at(id).map==row.at("map"),"Later-stage encounter is unsupported or unbound");
                // Their source profiles remain read-only until their implementation ticket installs the matching behavior.
            }}
            if(extension.contains("entry_links")){for(const auto& [id,link]:extension.at("entry_links").items()){
                need(!link.at("runtime_enabled").get<bool>(),"Later-stage entry is not implemented");
            }
            }
            for(const auto* id:{"m51-e9-p4","m51-e9-p5"}){content.transitions[id]=data.at("transitions").at(id);}
        }
        // Only the complete State8/contact contract has the training runtime consumers.
        if(delta.at("runtime").contains("status_definitions")){
            const auto& runtime=delta.at("runtime");
            need(runtime.contains("training_target_contacts")&&runtime.contains("gas_monitors"),"Incomplete training runtime capability");
            std::erase_if(content.pendingMapTickets,[](const auto& entry){return entry.second==93;});
            for(const auto& [id,flow]:runtime.at("flows").items()){
                if(flow.value("implementation_ticket",93)==93){content.catalog["flows"][id]["runtime_enabled"]=true;}
            }
            for(const auto& [id,encounter]:runtime.at("encounters").items()){
                const auto& reappear=encounter.at("rules").at("runtime_profile").at("reappear");
                if(!reappear.contains("special")){continue;}
                const auto enableDoor=[&](const std::string& flowId){
                    auto& flow=content.catalog["flows"].at(flowId);
                    need(flow.at("trigger")=="invoked"&&flow.at("implementation_ticket")==92&&content.objects.at(flow.at("object").get<std::string>()).map==content.objects.at(id).map,"Invalid pursuer door flow binding");
                    flow["runtime_enabled"]=true;
                };
                const auto& special=reappear.at("special");
                if(special.contains("flow")){enableDoor(special.at("flow"));}
                if(special.contains("sequences")){for(const auto& step:special.at("sequences")){if(step.contains("invoke")){enableDoor(step.at("invoke"));}}}
            }
            for(auto& [id,link]:content.transitions.items()){
                const auto destination=link.at("destination").at("map").get<int>();
                const auto target=runtime.at("map_implementation_tickets").find(std::to_string(destination));
                const bool training=link.value("implementation_ticket",0)==93||(target!=runtime.at("map_implementation_tickets").end()&&*target==93);
                if(training&&!content.pendingMapTickets.contains(destination)&&!content.pendingMapTickets.contains(content.objects.at(link.at("object").get<std::string>()).map)){
                    link["runtime_enabled"]=true;link["handoff_only"]=false;
                }
            }
        }
        if(delta.at("runtime").contains("weapon_actions")){
            const auto& actions=delta.at("runtime").at("weapon_actions");
            for(const auto& [id,profile]:actions.at("profiles").items()){
                for(const auto& cue:profile.at("cue_order")){
                    if(cue=="transact-gl-reload"){need(id=="gl-fire","Invalid grenade ammunition commit profile");continue;}
                    need(!content.cue(cue.get<std::string>()).contains("runtime_support"),"Unsupported full F4 weapon cue");
                }
                need(content.actionProfiles.emplace(id,profile).second,"Duplicate full F4 weapon action");
            }
            for(const auto& [id,binding]:actions.at("bindings").items()){
                for(const auto* role:{"raise","lower","fire"}){need(content.actionProfiles.contains(binding.at(role).get<std::string>()),"Missing full F4 weapon action");}
                auto& item=content.items.at(std::stoi(id));
                need(item.stateOnly&&item.pendingActionTicket==91,"Invalid full F4 weapon capability upgrade");
                need(content.catalog["weapon_bindings"].emplace(id,binding).second,"Duplicate full F4 weapon binding");
                item.stateOnly=false;item.pendingActionTicket=0;
            }
            for(const auto& [id,effect]:actions.at("effects").items()){
                need(content.media["effects"].emplace(id,effect).second,"Duplicate full F4 weapon effect");
            }
        }
        if(delta.at("runtime").contains("rescue")){
            const auto& rescue=delta.at("runtime").at("rescue");
            need(delta.at("runtime").contains("combat_effects")||delta.at("runtime").contains("weapon_actions"),"Rescue requires its shared burn presentation");
            for(const auto& [id,cue]:rescue.at("presentation_cues").items()){
                need(content.cues.emplace(id,std::make_shared<const Json>(cue)).second,"Conflicting rescue presentation cue");
            }
            for(const auto& [id,effect]:rescue.at("effects").items()){
                need(content.media["effects"].emplace(id,effect).second,"Conflicting rescue effect");
            }
            auto encounters=rescue.at("encounters");
            if(content.full->at("runtime").contains("hot_water")){for(const auto& [id,row]:content.full->at("runtime").at("hot_water").at("encounters").items()){need(!encounters.contains(id),"Duplicate hot-water enemy");encounters[id]=row;}}
            for(const auto& [id,row]:encounters.items()){
                need(row.at("type")=="zombie"&&row.at("object")==id&&content.objects.at(id).map==row.at("map"),"Unbound rescue enemy");
                validateBranchEnemyProfile(content,id,row.at("runtime_profile"));
                auto rules=content.rules.at("enemy");rules["runtime_profile"]=row.at("runtime_profile");
                rules["bite_attack"]=row.at("normal_attributes").at("attack");rules["defense"]=row.at("normal_attributes").at("defense");
                rules["death_damage_threshold"]=row.at("termination_damage_threshold");rules["activation_fact"]=row.at("activation_fact");
                rules["grab_max_bites"]=row.at("grab").at("maximum_damage_cycles");rules["grab_escape_direction_inputs"]=row.at("grab").at("escape_input_count");rules["grab_bite_wait_tenths"]=row.at("grab").at("wait_tenths");
                for(const auto* key:{"arrival","dormant_receive_activation","dormant_presentation","source_slots"}){if(row.contains(key)){rules[key]=row.at(key);}}
                need(content.enemies.emplace(id,EnemyDefinition{id,row.at("map"),row.at("source_order"),row.at("dead_fact"),false,std::move(rules)}).second,"Duplicate rescue enemy");
            }
            for(const auto& [id,flow]:rescue.at("flows").items()){
                need(content.objects.contains(flow.at("object").get<std::string>())&&flow.at("implementation_ticket")==94,"Unbound rescue flow");
                auto ready=flow;ready["runtime_enabled"]=true;content.catalog["flows"][id]=std::move(ready);
            }
            content.catalog["environment_targets"]=rescue.at("environment_targets");
            content.catalog["rescue_environment"]={{"clearance",rescue.at("clearance")},{"reinforcement",rescue.at("reinforcement")},{"player_reaction",rescue.at("player_reaction")}};
            std::erase_if(content.pendingMapTickets,[](const auto& entry){return entry.second==94;});
            for(auto& [id,flow]:content.catalog["flows"].items()){
                if(flow.value("implementation_ticket",0)==94){flow["runtime_enabled"]=true;}
            }
            for(const auto* id:{"m51-e9-p2","m51-e9-p3"}){
                auto& flow=content.catalog["flows"].at(id);
                need(flow.at("implementation_ticket")==95,"Unreviewed tank preparation flow");flow["runtime_enabled"]=true;
            }
            for(auto& [id,link]:content.transitions.items()){
                if(link.value("implementation_ticket",0)==94&&!content.pendingMapTickets.contains(link.at("destination").at("map").get<int>())&&!content.pendingMapTickets.contains(content.objects.at(link.at("object").get<std::string>()).map)){
                    link["runtime_enabled"]=true;link["handoff_only"]=false;
                }
            }
        }
        if(delta.at("runtime").contains("underground")&&delta.at("runtime").at("underground").contains("ordinary")){
            installUndergroundContent(content,delta.at("runtime").at("underground"));
            std::erase_if(content.pendingMapTickets,[](const auto& row){return row.second==97;});
            for(auto& [id,flow]:content.catalog["flows"].items()){
                const int ticket=flow.value("implementation_ticket",0);
                if((ticket!=96&&ticket!=97)||content.pendingMapTickets.contains(content.objects.at(flow.at("object").get<std::string>()).map)){continue;}
                const auto& savePoints=content.catalog.at("save_points");
                if(std::any_of(savePoints.begin(),savePoints.end(),[&](const auto& point){return point.at("object")==flow.at("object")&&point.at("page")==flow.at("page");})){flow["runtime_enabled"]=false;continue;}
                const auto link=content.transitions.find(id);
                flow["runtime_enabled"]=link==content.transitions.end()||!content.pendingMapTickets.contains(link->at("destination").at("map").get<int>());
            }
            for(auto& [id,link]:content.transitions.items()){
                const int ticket=link.value("implementation_ticket",0);
                if((ticket==96||ticket==97)&&!content.pendingMapTickets.contains(link.at("destination").at("map").get<int>())&&!content.pendingMapTickets.contains(content.objects.at(link.at("object").get<std::string>()).map)){
                    link["runtime_enabled"]=true;link["handoff_only"]=false;
                }
            }
        }
        if(delta.at("runtime").contains("tank_return")){
            const auto& returned=delta.at("runtime").at("tank_return");
            need(returned.at("implementation_ticket")==95,"Unreviewed rescue return capability");
            for(const auto& row:returned.at("object_pages")){
                const auto& pages=content.objects.at(row.at("object").get<std::string>()).variants;
                const auto& original=row.at("variant");const int page=original.at("original_page");
                const auto found=std::find_if(pages.begin(),pages.end(),[&](const auto& value){return value.page==page;});
                need(found!=pages.end()&&!found->definition.spriteId&&row.at("command_count")==0,"Missing retired conference actor page");
            }
            std::erase_if(content.pendingMapTickets,[](const auto& row){return row.second==95;});
            for(auto& [id,flow]:content.catalog["flows"].items()){
                const auto link=content.transitions.find(id);
                if(flow.value("implementation_ticket",0)==95&&(link==content.transitions.end()||!content.pendingMapTickets.contains(link->at("destination").at("map").get<int>()))){flow["runtime_enabled"]=true;}
            }
            for(auto& [id,link]:content.transitions.items()){
                if(link.value("implementation_ticket",0)==95&&!content.pendingMapTickets.contains(link.at("destination").at("map").get<int>())&&!content.pendingMapTickets.contains(content.objects.at(link.at("object").get<std::string>()).map)){
                    link["runtime_enabled"]=true;link["handoff_only"]=false;
                }
            }
        }
        if(delta.at("runtime").contains("parking")&&delta.at("runtime").at("parking").at("first_guard").contains("runtime_profile")){installFirstGuardContent(content,delta.at("runtime").at("parking").at("first_guard"));installParkingContent(content,delta.at("runtime").at("parking"));}
        if(delta.at("runtime").contains("truth")){installTruthContent(content,delta.at("runtime").at("truth"));}
        if(delta.at("runtime").contains("evacuation")){const auto& evacuation=delta.at("runtime").at("evacuation");installDrainageContent(content,evacuation);installCrocodileContent(content,evacuation);installPostCrocodileContent(content,evacuation);installSecondGuardContent(content,evacuation);if(evacuation.at("runtime_profile").at("escort").contains("details")){installEscortContent(content,evacuation);}}
        if(content.full->at("runtime").contains("door_closure")){
            const auto& closure=content.full->at("runtime").at("door_closure");
            for(const auto& [id,definition]:closure.at("flows").items()){
                const auto& object=content.objects.at(definition.at("object").get<std::string>());
                need(std::any_of(object.variants.begin(),object.variants.end(),[&](const auto& page){return page.page==definition.at("page");}),"Missing original door page");
                auto ready=definition;ready["runtime_enabled"]=true;content.catalog["flows"][id]=std::move(ready);
                if(content.transitions.contains(id)){
                    auto& link=content.transitions[id];need(content.maps.contains(link.at("destination").at("map").get<int>()),"Missing original door destination");
                    link["flow"]=definition.at("steps");link["runtime_enabled"]=true;link["handoff_only"]=false;
                }
            }
        }
        if(content.full->at("runtime").contains("dormant_licker")){
            const auto& binding=content.full->at("runtime").at("dormant_licker");const auto& row=binding.at("instance");const auto& rules=binding.at("rules");
            const std::string id=row.at("id");const auto& object=content.objects.at(id);const auto& profile=rules.at("runtime_profile");
            for(const auto& number:profile.at("pages")){need(std::any_of(object.variants.begin(),object.variants.end(),[&](const auto& page){return page.page==number;}),"Missing dormant licker page");}
            for(const auto& cue:profile.at("cues")){need(content.cues.contains(cue.get<std::string>()),"Missing dormant licker cue");}
            for(const auto& cue:profile.at("helper_sounds")){need(content.cues.contains(cue.get<std::string>()),"Missing dormant licker helper");}
            for(const auto& effect:profile.at("enemy_blood_effects")){need(content.media.at("effects").contains(effect.get<std::string>()),"Missing dormant licker blood");}
            EnemyBlueprint enemy;enemy.id=id;enemy.map=row.at("map");enemy.event=object.variants.front().definition.sourceEventId;enemy.sourceOrder=row.at("source_order");enemy.sourceSlot=(row.at("source_slots").at("base").get<int>()-81)/14+1;
            enemy.deadFlag=row.at("dead_switch");enemy.activationFlag=row.at("activation_fact");enemy.attack=row.at("normal_attributes").at("attack");enemy.defense=row.at("normal_attributes").at("defense");enemy.terminationDamage=row.at("termination_damage_threshold");enemy.behavior=EnemyBehaviorKind::Licker;enemy.hitboxHalfPixels={8,8};enemy.rules=rules;
            need(content.expansion.enemies.emplace(id,std::move(enemy)).second,"Duplicate dormant licker");
        }
        validateSceneContent(content,sprites);
    }
    Json readSource(const std::filesystem::path& directory,const Json& declaration){
        need(declaration.size()==2&&declaration.at("file")=="f4bc-source-contract.json"&&declaration.at("sha256")=="6fbe7afb3b2d198ef0948c5b4716e2171bc47dccc44ceb19114fd3c66b4bca70","Unreviewed full F4 source");
        const auto path=directory/"f4bc-source-contract.json";
        need(!std::filesystem::is_symlink(path)&&std::filesystem::is_regular_file(path)&&std::filesystem::file_size(path)<=32*1024*1024,"Invalid full F4 source file");
        std::ifstream input(path,std::ios::binary);const std::string bytes=readContentBytes(input);
        need(contentSha256(bytes)==declaration.at("sha256").get<std::string>(),"Full F4 source hash differs");
        std::istringstream verified(bytes);return parseCombatContentDefinition(verified);
    }
}
void initializeFullSession(const CombatContent& content,SessionEntry& entry){
    if(content.catalog.contains("difficulty")){for(const auto& id:content.catalog.at("difficulty").at("easy_preseed")){entry.worldFlags.try_emplace(content.facts.switches.at(id.get<int>()),false);}}
    if(content.catalog.contains("score")){
        for(const auto& [id,name]:content.catalog.at("score").at("facts").items()){entry.worldFlags.try_emplace(name.get<std::string>(),false);}
        for(const auto& [id,name]:content.catalog.at("score").at("values").items()){entry.worldValues.try_emplace(name.get<std::string>(),0);}
    }
    if(content.catalog.contains("g7")){for(const auto& [id,name]:content.catalog.at("g7").at("facts").items()){entry.worldFlags.try_emplace(name.get<std::string>(),false);}for(const auto& [id,name]:content.catalog.at("g7").at("values").items()){entry.worldValues.try_emplace(name.get<std::string>(),0);}}
    if(content.catalog.contains("g6")&&content.catalog.at("g6").contains("values")){for(const auto& [id,name]:content.catalog.at("g6").at("values").items()){entry.worldValues.try_emplace(name.get<std::string>(),0);}}
    if(content.catalog.contains("g6")){for(const auto& [id,name]:content.catalog.at("g6").at("facts").items()){entry.worldFlags.try_emplace(name.get<std::string>(),false);}}
    if(content.catalog.contains("g5")){for(const auto& [id,name]:content.catalog.at("g5").at("facts").items()){entry.worldFlags.try_emplace(name.get<std::string>(),false);}}
    if(content.catalog.contains("g4")){for(const auto& [id,name]:content.catalog.at("g4").at("facts").items()){entry.worldFlags.try_emplace(name.get<std::string>(),false);}}
    if(content.catalog.contains("g4")&&content.catalog.at("g4").contains("values")){for(const auto& [id,name]:content.catalog.at("g4").at("values").items()){entry.worldValues.try_emplace(name.get<std::string>(),0);}}
    if(content.catalog.contains("g3")){
        for(const auto& [id,name]:content.catalog.at("g3").at("facts").items()){entry.worldFlags.try_emplace(name.get<std::string>(),false);}
        for(const auto& [id,name]:content.catalog.at("g3").at("values").items()){entry.worldValues.try_emplace(name.get<std::string>(),0);}
    }
    if(content.catalog.contains("g2")){for(const auto& [id,name]:content.catalog.at("g2").at("facts").items()){entry.worldFlags.try_emplace(name.get<std::string>(),false);}}
    if(content.catalog.contains("g1")){for(const auto& [id,name]:content.catalog.at("g1").at("facts").items()){entry.worldFlags.try_emplace(name.get<std::string>(),false);}}
    if(content.catalog.contains("g1")&&content.catalog.at("g1").contains("values")){for(const auto& [id,name]:content.catalog.at("g1").at("values").items()){entry.worldValues.try_emplace(name.get<std::string>(),0);}}
    if(content.catalog.contains("g0")){for(const auto& [id,name]:content.catalog.at("g0").at("facts").items()){entry.worldFlags.try_emplace(name.get<std::string>(),false);}}
    if(content.full->is_null()){return;}
    const auto& state=content.full->at("state_contract");
    for(const auto& fact:state.at("facts")){entry.worldFlags.try_emplace(fact.at("stable_id").get<std::string>(),fact.at("initial").get<bool>());}
    for(const auto& value:state.at("persistent_values")){entry.worldValues.try_emplace(value.at("stable_id").get<std::string>(),value.at("initial").get<int>());}
    for(const auto& [item,rounds]:content.requiredMagazines){entry.magazines.try_emplace(item,rounds);}
}
CombatContent loadFullContent(const std::filesystem::path& path,const Json& data){
    need(data.at("schema")=="nobihaza.f4full.v1"&&data.at("content_compatibility")=="f4-normal-1","Unknown full F4 identity");
    const auto stage=data.at("stage").get<std::string>();stageIndex(stage);
    const auto original=readSource(path.parent_path(),data.at("source_contracts").at("f4bc-normal-1"));
    auto expectedSources=original.at("source_contracts");expectedSources["f4bc-normal-1"]=data.at("source_contracts").at("f4bc-normal-1");
    need(data.at("source_contracts")==expectedSources,"Unknown full F4 source declaration");
    need(data.size()==original.size()+2,"Unknown/missing full F4 field");
    const bool trainingProjection=data.at("maps").contains("51");
    std::set<std::string> changed{"schema","content_compatibility","source_contracts","items","entries","normal_initial_state","capabilities"};
    Json trainingDelta;
    if(trainingProjection){
        changed.insert({"maps","objects","sprites","images","audio","asset_names","animations","messages","cues","transitions","f3"});
        trainingDelta=reviewedTrainingProjection(data,original);
    }
    for(const auto& [key,value]:original.items()){if(!changed.contains(key)){need(data.at(key)==value,"Inherited full F4 content differs");}}
    const auto& full=data.at("f4full");need(full.is_object()&&full.size()==3,"Unknown full F4 extension");
    need(full.at("runtime").contains("underground")== (stageIndex(stage)>=1)&&full.at("runtime").contains("parking")== (stageIndex(stage)>=2),"Full F4 stage differs from its reviewed projection");
    need(full.at("runtime").contains("evacuation")== (stageIndex(stage)>=4),"Evacuation stage differs from its reviewed projection");
    const auto& state=full.at("state_contract");
    if(!trainingProjection){need(contentSha256(state.dump())=="d4f3a6f31d266e3f08ae1b4e18fab4eea120c224707ea50e74a37234005d3c99","Unreviewed full F4 state declaration");}
    if(!trainingProjection){need(full.at("runtime")==Json{{"status","pending"},{"flows",Json::object()},{"encounters",Json::object()},{"map_music",Json::object()}}&&full.at("catalog")==Json{{"status","pending"},{"selected_pages",Json::array()},{"assets",Json::array()},{"animations",Json::array()}},"Full F4 runtime is not delivered");}
    auto source=std::make_shared<const CombatContent>(loadCombatContentDefinition(path.parent_path()/"f4bc-source-contract.json",original));auto content=*source;
    content.legacySource=source;content.compatibility="f4-normal-1";content.setFullDefinition(full);content.fullStage=stage;
    Json newDefinitions=Json::object();
    for(const auto& [key,item]:data.at("items").items()){
        if(original.at("items").contains(key)){need(item==original.at("items").at(key),"Inherited full F4 item differs");continue;}
        newDefinitions[key]=item;
        auto definition=readItemDefinition(item);need(key==std::to_string(definition.id),"Full F4 item ID differs");
        if(definition.equipmentType==1||definition.equipmentType==2){definition.stateOnly=true;definition.pendingActionTicket=91;}
        need(content.items.emplace(definition.id,std::move(definition)).second,"Duplicate full F4 item ID");
    }
    need(data.at("items").size()==original.at("items").size()+newDefinitions.size()&&contentSha256(newDefinitions.dump())=="0c570dd090828781a8bb84f747ac121c3c4d81738b4dfedcef6b5b0ff4259e8c","Unreviewed/missing full F4 item definitions");
    auto capabilities=original.at("capabilities");capabilities["delivered_stage"]=stage;
    for(const auto* item:{"16","17","18","19","25"}){capabilities["weapons"][item]="state-only";}
    need(data.at("capabilities")==capabilities,"Unknown full F4 capability");
    for(const auto& fact:state.at("facts")){
        const auto key=fact.at("stable_id").get<std::string>();const int id=fact.at("source_switch");
        const auto found=content.facts.switches.find(id);
        if(found!=content.facts.switches.end()){need(found->second==key,"Inherited full F4 fact differs");}
        else{need(!content.facts.flagIds().contains(key),"Duplicate full F4 fact name");content.facts.switches.emplace(id,key);}
    }
    for(const auto& value:state.at("persistent_values")){
        const auto key=value.at("stable_id").get<std::string>();const int id=value.at("source_variable");
        need(!content.facts.variables.contains(id)&&!content.facts.valueIds().contains(key),"Duplicate full F4 value");content.facts.variables.emplace(id,key);
    }
    for(const auto& slot:state.at("magazine_slots")){
        const int item=slot.at("item"),initial=slot.at("initial"),variable=slot.at("source_variable");
        need(content.items.at(item).capacity==slot.at("capacity")&&initial>=0&&initial<=content.items.at(item).capacity,"Invalid full F4 magazine slot");
        if(content.requiredMagazines.contains(item)){need(content.requiredMagazines.at(item)==initial&&content.magazineSeedBindings.at(variable)==item,"Inherited magazine binding differs");}
        else{need(!content.magazineSeedBindings.contains(variable),"Duplicate magazine source variable");content.requiredMagazines.emplace(item,initial);content.magazineSeedBindings.emplace(variable,item);}
    }
    auto expectedEntry=[&](Json entry){for(const auto& [key,value]:state.at("magazine_seeds").items()){if(!entry.at("magazines").contains(key)){entry["magazines"][key]=value;}}return entry;};
    need(data.at("normal_initial_state")==expectedEntry(original.at("normal_initial_state"))&&data.at("entries").size()==original.at("entries").size(),"Full F4 initial state differs");
    for(const auto& [key,value]:original.at("entries").items()){
        need(data.at("entries").at(key)==expectedEntry(value),"Inherited full F4 development entry differs");initializeFullSession(content,content.entries.at(key).session);
        GameSession check(content.items,content.entries.at(key).session,content.facts);
    }
    if(trainingProjection){appendTrainingProjection(content,data,trainingDelta);}
    return content;
}
void validateFullSession(const CombatContent& content,const SessionEntry& entry,bool clearBoundary){
    if(content.catalog.contains("difficulty")){for(const auto& id:content.catalog.at("difficulty").at("easy_preseed")){need(entry.worldFlags.contains(content.facts.switches.at(id.get<int>())),"Missing difficulty initialization fact");}}
    if(content.catalog.contains("score")){
        const auto& score=content.catalog.at("score");
        for(const auto& [id,name]:score.at("facts").items()){need(entry.worldFlags.contains(name.get<std::string>()),"Missing score fact");}
        for(const auto& [id,name]:score.at("values").items()){need(entry.worldValues.contains(name.get<std::string>()),"Missing score value");}
        GameSession state(content.items,entry,content.facts);const auto in=state.endingScoreInput();(void)endingScore(in);
        if(content.catalog.contains("cycle")){const auto& cycle=content.catalog.at("cycle");need(state.variable(cycle.at("counter"))>=0&&state.variable(cycle.at("counter"))<=cycle.at("counter_max").get<int>(),"Cycle count outside fixed source bounds");}
        need(in.hours<INT_MAX,"Playtime exceeds supported statistic range");need(clearBoundary?state.variable(381)==endingScore(in).rank:state.variable(381)==0,"Rank disagrees with save boundary");
        for(int id=601;id<=605;++id){if(!clearBoundary&&!(content.catalog.contains("cycle")&&state.variable(635)>0)){need(!state.flag(id),"Reward has no delivered Clear or new-cycle save boundary");}}
    }
    if(content.catalog.contains("g7")){for(const auto& [id,name]:content.catalog.at("g7").at("facts").items()){need(entry.worldFlags.contains(name.get<std::string>()),"Missing final fact");if(!clearBoundary){need(!entry.worldFlags.at(name.get<std::string>()),"Final facts have no delivered legal save boundary");}}need(entry.worldValues.contains("final.assistance-hits")&&entry.worldValues.at("final.assistance-hits")>=0,"Missing or invalid final hit statistic");}
    if(content.catalog.contains("g4")&&content.catalog.at("g4").contains("values")){
        for(const auto& [id,name]:content.catalog.at("g4").at("values").items()){need(entry.worldValues.contains(name.get<std::string>()),"Missing G4 value");}
        GameSession check(content.items,entry,content.facts);need(check.variable(627)>=0&&check.variable(627)<=5,"Invalid truth stage");need(!check.flag(506)||check.flag(504),"Truth completion before Dora victory");
    }
    if(content.catalog.contains("g6")){
        for(const auto& [id,name]:content.catalog.at("g6").at("facts").items()){need(entry.worldFlags.contains(name.get<std::string>()),"Missing return fact");}
        if(content.catalog.at("g6").contains("terminal_contract")){
            for(const auto& [id,name]:content.catalog.at("g6").at("values").items()){need(entry.worldValues.contains(name.get<std::string>()),"Missing evacuation value");}
            GameSession check(content.items,entry,content.facts);
            need(check.variable(628)>=0&&check.variable(628)<=2&&check.variable(629)>=0&&check.variable(629)<=2&&check.variable(635)>=0,"Invalid evacuation stage");
            // Ordinary snapshots have no foreground stack for the intermediate display stage.
            need(check.variable(628)!=1,"Terminal intermediate stage at save boundary");
            need(check.flag(528)==(check.variable(628)==2),"Terminal completion disagrees with display stage");
            need(!check.flag(528)||check.flag(content.catalog.at("g5").at("control_contract").at("victory_flag")),"Terminal before Cash victory");
            need(!check.flag(530)||(check.flag(528)&&check.variable(629)==2),"Rescue phase without complete terminal/rescue");
            need(check.flag(530)||(check.variable(629)==0&&!check.flag(529)&&!check.flag(556)),"Incomplete rescue at save boundary");
            need(!check.flag(556)||(check.flag(530)&&check.variable(635)>=1),"Core transfer without completion history");
        }
        GameSession check(content.items,entry,content.facts);const auto& contract=content.catalog.at("g6").at("return_contract");
        if(content.catalog.at("g6").contains("memorial_contract")){
            const auto& memorial=content.catalog.at("g6").at("memorial_contract");
            need(!check.flag(memorial.at("claim_flag"))||check.flag(memorial.at("phase_flag")),"Memorial claim before return phase");
        }
        if(content.catalog.contains("warehouse_g4")){const auto& warehouse=content.catalog.at("warehouse_g4");need(!check.flag(warehouse.at("victory_fact"))||check.flag(warehouse.at("started_fact")),"Warehouse victory before start");need(!check.flag(warehouse.at("fatal_squeeze_fact")),"Fatal squeeze cannot be an ordinary live save");}
        const int phase=contract.at("phase");
        need(!check.flag(phase)||check.flag(content.catalog.at("g5").at("control_contract").at("victory_flag")),"Return world before Cash victory");
        need(!check.flag(contract.at("reflection"))||check.flag(phase),"Reflection before return world");
        need(!check.flag(contract.at("ambush"))||check.flag(phase),"Ambush before return world");
        for(const auto& dead:contract.at("deaths")){need(!check.flag(dead)||check.flag(phase)||isDifficultyPreset(content,check,dead),"Return enemy death before world phase");}
    }
    if(content.catalog.contains("g5")){for(const auto& [id,name]:content.catalog.at("g5").at("facts").items()){need(entry.worldFlags.contains(name.get<std::string>()),"Missing G5 fact");}}
    if(content.catalog.contains("g4")){for(const auto& [id,name]:content.catalog.at("g4").at("facts").items()){need(entry.worldFlags.contains(name.get<std::string>()),"Missing G4 fact");}}
    if(content.catalog.contains("g5")&&content.catalog.at("g5").contains("control_contract")){
        GameSession check(content.items,entry,content.facts);const auto& contract=content.catalog.at("g5").at("control_contract");
        for(const auto& id:contract.at("required_flags")){need(!check.flag(contract.at("victory_flag"))||check.flag(id),"Cash victory before Dora/truth completion");}
        for(const auto& id:contract.at("claim_flags")){need(!check.flag(id)||check.flag(contract.at("victory_flag")),"Control supply before Cash victory");}
    }
    if(content.catalog.contains("g3")){
        for(const auto& [id,name]:content.catalog.at("g3").at("facts").items()){need(entry.worldFlags.contains(name.get<std::string>()),"Missing G3 fact");}
        for(const auto& [id,name]:content.catalog.at("g3").at("values").items()){need(entry.worldValues.contains(name.get<std::string>()),"Missing Continue source value");}
        GameSession check(content.items,entry,content.facts);const auto& checkpoint=continueCheckpoint(content,check.variable(283));const auto& tile=checkpoint.at("tile");
        check.validateContinueCheckpoint(content.magazineSeedBindings,checkpoint.at("map"),{tile.at(0),tile.at(1)},check.flag(586)||check.variable(283)!=0);
        need(!check.flag(449)||check.flag(447),"Boss defeated before registration");need(!check.flag(450)||check.flag(449),"Boss aftermath before victory");need(!check.flag(499)||check.flag(450),"Magnum reward before aftermath");
        if(content.catalog.at("g3").at("delivered").get<int>()>=2){
            need(!check.flag(468)||check.flag(450),"Vent crossing before boss aftermath");
            if(content.catalog.at("g3").at("delivered").get<int>()>=3){
                need(check.variable(626)>=0&&check.variable(626)<=2,"Invalid lift floor");
                bool basement=check.flag(565);for(int id=388;id<=395;++id){basement|=check.flag(id);}
                need(!basement||check.flag(450),"Basement progress before boss aftermath");
            }
            need(!check.flag(463)||check.flag(461),"X acquisition before north route opens");
            need(check.count(191)+check.count(206)==(check.flag(512)?1:0),"Manual inventory disagrees with acquisition");
            int keys=check.count(105)+check.count(106);const int delivered=content.catalog.at("g3").at("delivered");
            if(delivered>=4){
                keys+=check.count(107)+check.count(108)+check.count(109)+check.count(110);const int phase=check.variable(387);
                need(phase>=0&&phase<=(delivered>=5?3:2),"Invalid access authentication phase");
                need(check.flag(469)==(phase>=1),"X authentication disagrees with phase");need(!check.flag(469)||check.flag(463),"Authentication without X acquisition");
                need(!check.flag(470)||(check.count(107)+check.count(108)==1&&check.eventTimer().source==EventTimerSource::AccessKey),"Cold timer without Y");
                if(delivered==4){
                    need(check.count(109)==0&&check.count(110)==0,"Hot Z inventory is not delivered");need(!check.flag(471),"Hot Z is not delivered");
                    need(keys==(check.flag(463)?1:0),"Access-key inventory disagrees with acquisition");
                }else{
                    const bool ground=check.flag(473);
                    need(keys+(ground?1:0)==(check.flag(463)&&phase<3?1:0),"Ground/access-key ownership disagrees with phase/acquisition");
                    const bool hot=check.count(109)+check.count(110)==1;const auto& clock=check.eventTimer();
                    need(hot==check.flag(471),"Hot Z disagrees with active source fact");
                    need(!hot||(clock.source==EventTimerSource::AccessKey&&clock.running==(clock.remainingTicks>=60)),"Hot Z timer is stopped with positive source seconds");
                    need(!check.flag(462)||phase==3,"Isolation released before Z authentication");
                    need(!check.flag(526)||check.flag(463),"Hot pickup activation before key acquisition");
                    need(!check.flag(465)||check.count(106)==1,"Delayed X request without usable X");
                    const auto& room=content.catalog.at("heat_room");
                    const int x=check.variable(385),y=check.variable(386);
                    if(ground||x||y){need(std::find(room.at("ground_tiles").begin(),room.at("ground_tiles").end(),Json::array({x,y}))!=room.at("ground_tiles").end(),"Invalid original heat ground coordinate");}
                }
            }
            if(delivered>=6){
                need(check.count(111)==(check.flag(509)?1:0),"B1 card disagrees with acquisition");
                need(!check.flag(509)||check.flag(462),"B1 card before isolation release");
                for(int fact:{403,404,405,406,566}){need(!check.flag(fact)||check.flag(462)||isDifficultyPreset(content,check,fact),"B1 progress before isolation release");}
            }
            if(delivered>=7){
                need(!check.flag(510)||check.flag(509),"Radio before original B1 card");
                need(!check.flag(511)||check.flag(510),"Reunion before radio");
                need(!check.flag(513)||check.flag(511),"Suneo reunion talk before reunion");
            }
            need(keys<=1&&(check.flag(463)||keys==0),"X inventory disagrees with acquisition");
            if(delivered<=3){need(keys==(check.flag(463)?1:0),"Acquired X missing before authentication delivery");}
        }
    }
    if(content.catalog.contains("g2")){
        for(const auto& [id,name]:content.catalog.at("g2").at("facts").items()){need(entry.worldFlags.contains(name.get<std::string>()),"Missing G2 persistent fact");}
        GameSession check(content.items,entry,content.facts);
        need(!(check.flag(493)||check.flag(561)||(content.catalog.at("g2").at("delivered").get<int>()>=2&&(check.flag(381)||check.flag(384)||check.flag(402)||check.flag(559)))||(content.catalog.at("g2").at("delivered").get<int>()>=3&&((check.flag(382)&&!isDifficultyPreset(content,check,382))||check.flag(383)||check.flag(446))))||check.flag(443),"G2 pickup before laboratory departure");
        if(content.catalog.at("g2").at("delivered").get<int>()>=4){need(!check.flag(445)||check.flag(444),"G2 power without completed panel");need(!(check.flag(444)||check.flag(445))||check.flag(443),"G2 panel before laboratory departure");}
        if(content.catalog.at("g2").at("delivered").get<int>()>=5){
            need(!check.flag(447)||check.flag(445),"G2 server without power");
            need(!check.flag(503)||check.flag(445),"G2 ambush trigger without power");
            need(!check.flag(502)||check.flag(503),"G2 ambush complete without trigger");
        }
        if(content.catalog.at("g2").at("delivered").get<int>()>=6){need(check.count(101)<=1&&(!check.count(101)||check.flag(447)),"G2 card contradicts registration");}
        need(check.count(190)+check.count(205)==(check.flag(493)?1:0),"G2 mail contradicts acquisition");
    }
    if(content.catalog.contains("g1")){
        for(const auto& [id,name]:content.catalog.at("g1").at("facts").items()){need(entry.worldFlags.contains(name.get<std::string>()),"Missing G1 persistent fact");}
        GameSession check(content.items,entry,content.facts);
        need(!check.flag(564)||check.flag(360),"G1 ammunition taken before awakening");
        if(content.catalog.at("g1").at("delivered").get<int>()>=2){
            need(!check.flag(362)||check.flag(360),"G1 stone fell before awakening");
            need(!check.flag(363)||check.flag(362),"G1 wax key taken before stone fell");
            need(!check.flag(364)||check.flag(363),"G1 wax door opened before key acquisition");
            need(check.count(93)<=1&&(!check.flag(363)?check.count(93)==0:check.flag(364)||check.count(93)==1),"G1 wax key inventory contradicts acquisition/unlock");
        }
    }
    if(content.catalog.contains("g1")&&content.catalog.at("g1").at("delivered").get<int>()>=3){
        GameSession check(content.items,entry,content.facts);
        need(!check.flag(361)||check.flag(364),"Wax intro before unlocked door");
        need(!check.flag(365)||check.flag(361),"Manor key before wax intro");
        need(!check.flag(366)||check.flag(365),"Chase before key acquisition");
        need(!check.flag(368)||check.flag(366),"Wax head before chase");
        need(!check.flag(367)||check.flag(368),"Hunter activation before wax head");
        need(!check.flag(270)||check.flag(367),"First Hunter death before activation");
        need(check.count(94)<=1&&(check.flag(365)||check.count(94)==0),"Manor key without acquisition or duplicated");
    }
    if(content.catalog.contains("g1")&&content.catalog.at("g1").at("delivered").get<int>()>=4){
        GameSession check(content.items,entry,content.facts);
        for(const auto& [id,enemy]:content.enemies){if(enemy.rules.value("behavior",std::string{})=="hunter"){need(!check.flag(enemy.deadFlag)||check.flag(enemy.rules.at("activation_fact"))||isDifficultyPreset(content,check,enemy.deadFlag),"Mansion Hunter death before activation");}}
        need(!check.flag(419)||check.flag(360),"Mansion inner door before awakening");
        need(!check.flag(420)||check.flag(365),"Corridor manor lock before I94 acquisition");
        need(!check.flag(425)||check.flag(365),"Study manor lock before I94 acquisition");
        need(!check.flag(433)||check.flag(367),"Clock part before mansion route activation");
    }
    if(content.catalog.contains("g1")&&content.catalog.at("g1").at("delivered").get<int>()>=5){
        GameSession check(content.items,entry,content.facts);
        need(!check.flag(495)||check.flag(494),"Small key before draining");
        need(!check.flag(496)||check.flag(495),"Kitchen unlock before key acquisition");
        need(!check.flag(497)||check.flag(496),"HP handgun before kitchen unlock");
        need(check.count(102)==(check.flag(495)&&!check.flag(496)?1:0),"Small key contradicts acquisition/unlock");
        need(check.count(13)+(check.equipped(1)==13)<=1&&(check.flag(497)||(check.count(13)==0&&check.equipped(1)!=13)),"HP handgun contradicts acquisition");
        for(int flag:{494,500,553}){need(!check.flag(flag)||check.flag(360),"Kitchen pickup before awakening");}
    }
    if(content.catalog.contains("g1")&&content.catalog.at("g1").at("delivered").get<int>()>=6){
        GameSession check(content.items,entry,content.facts);
        need(!check.flag(433)||check.flag(425),"Clock part before study lock");
        need(!check.flag(434)||check.flag(433),"Clock open before part acquisition");
        need(!check.flag(435)||check.flag(434),"Clock axe before mechanism open");
        need(!check.flag(492)||check.flag(420),"Clock note before corridor lock");
        const int parts=check.count(95)+check.count(96);
        need(parts<=1&&parts==(check.flag(433)&&!check.flag(434)?1:0),"Clock part inventory contradicts acquisition/use");
        need(check.count(97)<=1&&(check.flag(435)||check.count(97)==0),"Clock axe without acquisition or duplicated");
        need(check.count(189)==(check.flag(492)?1:0),"Clock note contradicts acquisition");
        need(!check.flag(423)&&!check.flag(430)&&!check.flag(431)&&!check.flag(432),"Transient clock/bookcase work at ordinary save");
    }
    if(content.catalog.contains("g1")&&content.catalog.at("g1").at("delivered").get<int>()>=7){
        GameSession check(content.items,entry,content.facts);
        need(!check.flag(422)||check.flag(360),"Mirror broken before awakening");
        need(!check.flag(424)||(check.flag(422)&&check.flag(435)),"Axe request before mirror or axe acquisition");
        need(!check.flag(427)||check.flag(424),"Secret door before axe request");
        need(!check.flag(436)||check.flag(427),"Piano device before secret door");
        const int axes=check.count(97)+check.count(98);
        need(axes<=1&&(check.flag(435)||axes==0)&&(!check.flag(427)||axes==0),"Carved axe contradicts acquisition/use");
        need(check.count(99)<=1&&(check.flag(436)||check.count(99)==0),"Piano device without acquisition or duplicated");
    }
    if(content.catalog.contains("g1")&&content.catalog.at("g1").at("delivered").get<int>()>=8){
        GameSession check(content.items,entry,content.facts);
        const int devices=check.count(99)+check.count(100);
        need(devices<=1&&(check.flag(436)||devices==0)&&(!check.flag(428)||devices==0),"Piano device contradicts acquisition/insertion");
        need(!check.flag(426)||check.flag(436),"Piano request before acquisition");
        need(!check.flag(428)||check.flag(426),"Piano insertion before request");
        need(!check.flag(429)||check.flag(428),"Piano battle before insertion");
        need(!(check.flag(437)||check.flag(438))||(check.flag(378)&&check.flag(379)),"Alarm or laboratory exit before both piano deaths");
    }
    if(content.catalog.contains("g1")&&content.catalog.at("g1").at("delivered").get<int>()>=9){
        GameSession check(content.items,entry,content.facts);need(entry.worldValues.contains("laboratory.treatment-remaining"),"Missing treatment allowance");
        need(check.variable(389)>=0&&check.variable(389)<=3,"Invalid treatment allowance");
        need(!check.flag(439)||check.flag(438),"Escape actor change before laboratory exit");
        need(!check.flag(440)||(check.flag(439)&&!check.flag(437)),"Monitor briefing facts contradict alarm");
        need(!check.flag(443)||check.flag(440),"Outside monitor before briefing");
        need(check.flag(440)||check.variable(389)==0,"Treatment allowance before briefing");
        need(!check.flag(440)||check.flag(443)||check.variable(389)==3,"Treatment spent before leaving monitor room");
        need((!check.flag(557)&&!check.flag(558)&&!check.flag(563)&&check.count(22)==0&&check.equipped(1)!=22&&check.equipped(2)!=22)||check.flag(440),"Laboratory supplies before briefing");
    }
    if(content.catalog.contains("g0")){
        for(const auto& [id,name]:content.catalog.at("g0").at("facts").items()){need(entry.worldFlags.contains(name.get<std::string>()),"Missing G0 persistent fact");}
        const auto flag=[&](int id){return entry.worldFlags.at(content.facts.switches.at(id));};
        need(!flag(347)||flag(346),"G0 return before first visit");
        need(!flag(348)||flag(347),"G0 battle intro before safe window");
        need(!flag(349)||flag(348),"G0 battle before intro");
        need(!flag(350)||flag(349),"G0 victory before battle");
        need(!flag(357)||flag(350),"G0 flight before victory");
        need(!flag(360)||flag(357),"G0 wake before flight");
        const int delivered=content.catalog.at("g0").at("delivered");
        need(delivered>=4||(!flag(348)&&!flag(349)),"G0 battle intro is not delivered");
        need(delivered>=6||!flag(350),"G0 victory is not delivered");
        need(delivered>=7||!flag(357),"G0 aviation is not delivered");
        need(delivered>=9||!flag(360),"G0 wake is not delivered");
        // Migration validates the old snapshot against its exact source first.
        // The target Session uses this delivery's G0 limits, plus inherited F4 rules.
        const CombatContent* inherited=content.legacySource.get();
        while(inherited&&inherited->catalog.contains("g0")){inherited=inherited->legacySource.get();}
        need(inherited!=nullptr,"Missing inherited G0 state contract");
        validateFullSession(*inherited,entry,clearBoundary);return;
    }
    if(content.full->is_null()){return;}
    const auto& state=content.full->at("state_contract");const int delivered=stageIndex(content.fullStage);
    const auto& old=*content.legacySource;
    const auto oldFlagIds=old.facts.flagIds();
    for(const auto& fact:state.at("facts")){
        const auto key=fact.at("stable_id").get<std::string>();const auto found=entry.worldFlags.find(key);
        need(found!=entry.worldFlags.end(),"Missing full F4 fact");
        if(!oldFlagIds.contains(key)&&found->second){need(stageIndex(fact.at("required_stage"))<=delivered,"Full F4 fact requires later content");}
    }
    for(const auto& value:state.at("persistent_values")){
        const auto key=value.at("stable_id").get<std::string>();const auto found=entry.worldValues.find(key);
        need(found!=entry.worldValues.end()&&found->second>=value.at("minimum").get<int>()&&found->second<=value.at("maximum").get<int>(),"Invalid full F4 persistent value");
        if(stageIndex(value.at("required_stage"))>delivered){need(found->second==value.at("initial").get<int>(),"Full F4 value requires later content");}
    }
    const std::array<int,5> equipment{entry.mainWeapon,entry.auxiliaryWeapon,entry.armor,entry.accessory,entry.otherEquipment};
    for(const auto& item:state.at("new_items")){
        const int id=item.at("id");if(old.items.contains(id)){continue;}
        const bool held=(entry.inventory.contains(id)&&entry.inventory.at(id)>0)||std::find(equipment.begin(),equipment.end(),id)!=equipment.end();
        if(held){need(stageIndex(item.at("first_natural_stage"))<=delivered,"Full F4 item requires later content");}
    }
    for(const auto& slot:state.at("magazine_slots")){
        const int item=slot.at("item");
        if(stageIndex(slot.at("execution_stage"))>delivered){need(entry.magazines.at(item)==slot.at("initial").get<int>(),"Dormant full F4 magazine was modified");}
    }
    const auto timer=entry.eventTimer;
    const int needed=timer.source==EventTimerSource::Power?1:timer.source==EventTimerSource::GuardFirst?2:timer.source==EventTimerSource::GuardFinal?4:0;
    need(needed<=delivered&&!timer.visible&&!timer.battle,"Full F4 timer requires unavailable or unnatural state");
    if(delivered<1){need(!entry.conditions.poisoned&&entry.conditions.poisonWaitTicks==0,"Poison requires underground content");}
}

void installLowerTileContent(CombatContent& c,const Json& lower){
    LowerTileDefinition d;d.allowed=lower.at("allowed").get<std::vector<std::array<int,2>>>();d.passageFlags=lower.at("passage_flags").get<std::vector<int>>();d.terrainIds=lower.at("terrain_ids").get<std::vector<int>>();
    need(d.passageFlags.size()==162&&d.terrainIds.size()==162&&std::all_of(d.passageFlags.begin(),d.passageFlags.end(),[](int n){return n>=0&&n<=63;}),"Invalid lower passage table");
    auto& map=c.maps.at(lower.at("map"));need(!map.lowerTiles,"Duplicate lower binding");
    for(const auto& cell:lower.at("tiles")){
        const auto tile=cell.at("tile").get<std::array<int,2>>();const int index=cell.at("index"),upper=cell.at("upper_passage");
        need(tile[0]>=0&&tile[0]<map.width&&tile[1]>=0&&tile[1]<map.height&&index>=0&&index<144&&upper>=0&&upper<=63,"Invalid lower cell");d.cells.push_back({tile,index,upper});
    }
    for(const auto& [id,image]:lower.at("images").items()){
        const int index=std::stoi(id);const auto& resource=c.media.at("images").at(image.get<std::string>());need(index>=0&&index<144&&resource.at("size_px")==Json::array({16,16}),"Invalid lower pixels");d.images.emplace(index,c.directory/resource.at("file").get<std::string>());
    }
    for(const auto& pair:d.allowed){need(pair[0]>=0&&pair[0]<144&&pair[1]>=0&&pair[1]<144&&d.images.contains(pair[1]),"Invalid lower substitution");}
    map.lowerTiles=std::move(d);
}
