#include "combat_content.h"
#include "full_content.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <stdexcept>
void require(bool condition,const char* reason){if(!condition){throw std::runtime_error(reason);}}
int main(int argc,char** argv){
    if((argc!=2&&argc!=3&&argc!=4)||(argc==4&&std::string(argv[2])!="--status")){return 2;}
    try{
        const auto path=std::filesystem::absolute(argv[1]);
        const auto content=loadCombatContent(path);
        if(argc==3&&std::string(argv[2])=="--parking"){
            require(content.fullStage=="parking"&&content.maps.size()==87&&content.objects.size()==1257,"Parking reader preserves cumulative training and underground content");
            const auto& runtime=content.full->at("runtime");const auto& parking=runtime.at("parking");
            require(runtime.contains("status_definitions")&&content.routes.contains(runtime.at("player_recovery").at("ce113").at("restore_route_selection").at("then")),"Later package keeps State8 and CE113 instead of reverting the training contract");
            require(parking.at("flows").size()==90&&parking.at("encounters").size()==15,"Reviewed parking interactions and encounters are available to their consumers");
            for(const auto& [map,ticket]:parking.at("map_implementation_tickets").items()){
                const int id=std::stoi(map);
                if(content.legacySource->maps.contains(id)&&!content.legacySource->pendingMapTickets.contains(id)){
                    require(!content.pendingMapTickets.contains(id),"Cumulative content must not lock delivered B/C maps");
                }
                else{require(content.pendingMapTickets.at(id)==ticket,"New parking maps preserve their gameplay gate");}
            }
            for(const auto& [id,flow]:parking.at("flows").items()){
                const auto& inherited=content.legacySource->catalog.at("flows");
                if(inherited.contains(id)){require(content.catalog.at("flows").at(id)==inherited.at(id),"Cumulative projection preserves delivered B/C interaction behavior");}
                else{require(!content.catalog.at("flows").at(id).at("runtime_enabled").get<bool>(),"Reading parking data does not pretend its new behavior is implemented");}
            }
            auto state=content.entries.at("natural").session;state.conditions.neuralParalysis=true;
            state.eventTimer={EventTimerSource::GuardFirst,1800,true,false,false};validateFullSession(content,state);
            state.eventTimer.source=EventTimerSource::GuardFinal;bool rejected=false;
            try{validateFullSession(content,state);}catch(const std::exception&){rejected=true;}
            require(rejected,"Parking contract cannot restore the undelivered final guard timer");
            const auto fixture=std::filesystem::temp_directory_path()/("nobihaza-parking 中文-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}} cleanup{fixture};
            std::filesystem::copy(path.parent_path(),fixture,std::filesystem::copy_options::recursive);
            std::ifstream input(path);const auto original=nlohmann::json::parse(input);
            for(int variation=0;variation<3;++variation){auto bad=original;
                if(variation==0){bad["stage"]="underground";bad["capabilities"]["delivered_stage"]="underground";}
                else if(variation==1){bad["f4full"]["runtime"]["parking"]["flows"].begin().value()["runtime_enabled"]=true;}
                else{bad["f4full"]["runtime"]["status_definitions"].erase("state8");}
                std::ofstream(fixture/"malformed.json")<<bad.dump();rejected=false;
                try{loadCombatContent(fixture/"malformed.json");}catch(const std::exception&){rejected=true;}
                require(rejected,"Disguised stage, unaudited runtime and lost cumulative State8 are rejected");
            }
            std::cout<<"Cumulative parking content, State8 and later-stage rejection passed\n";return 0;
        }
        if(argc>=3&&std::string(argv[2])=="--status"){
            require(argc==4,"Status admission needs its unchanged reviewed training package for both mismatch directions");
            require(content.fullStage=="training"&&content.maps.size()==70&&content.objects.size()==974,"State8 supplement retains complete cumulative training identity");
            const auto& runtime=content.full->at("runtime");const auto& status=runtime.at("status_definitions").at("state8");
            const auto& field=content.full->at("state_contract").at("player_conditions").at("new_fields").at("neural_paralysis");
            require(status.at("source_state")==8&&status.at("session_field")=="PlayerConditions.neuralParalysis"&&status.at("snapshot_field")=="neural_paralysis"&&status.at("source_aliases").empty(),"Original State8 has an independent session boolean without a switch alias");
            require(field.at("type")=="boolean"&&!field.at("initial").get<bool>()&&field.at("source_state")==8,"Reviewed source declaration retains the fresh false State8 value");
            const auto& recovery=runtime.at("player_recovery").at("ce113");const auto& selection=recovery.at("restore_route_selection");
            require(recovery.at("activation_fact")==148&&recovery.at("presentation_fact")==149&&recovery.at("wait_ticks")==240&&recovery.at("remove_status")=="state8","CE113 retains separate trigger/presentation facts and the original Wait40 recovery");
            require(selection.at("condition").at("kind")=="flag"&&selection.at("condition").at("id")==3&&selection.at("condition").at("value").get<bool>(),"CE113 C5 selects the original S3 direction-lock condition");
            for(const auto& [branch,downs]:std::map<std::string,int>{{"then",3},{"else",2}}){
                const auto& route=content.routes.at(selection.at(branch).get<std::string>());
                require(route.target==10001&&route.frequency==8&&route.steps.size()==static_cast<std::size_t>(11+downs),"Both original player restore routes reach the shared finite route directory");
                for(std::size_t n=0;n<route.steps.size();++n){require(route.steps[n].kind==(n<11?"speed_up":"speed_down"),"CE113 C6/C9 preserve eleven up and their distinct three/two down steps");}
            }
            GameSession session(content.items,content.entries.at("natural").session,content.facts);
            require(!session.conditions().neuralParalysis,"Fresh complete training does not seed neural paralysis");
            session.setFlag(148,true);session.setFlag(149,true);require(!session.conditions().neuralParalysis,"Source trigger and presentation switches cannot alias State8");
            session.setNeuralParalysis(true);session.setFlag(148,false);session.setFlag(149,false);validateFullSession(content,session.exportState());
            require(session.conditions().neuralParalysis,"Independent current State8 survives changes to its source switches");
            for(int map=51;map<=58;++map){
                require(!content.pendingMapTickets.contains(map),"Complete training contracts reach their implemented map consumers");
            }
            for(int map:{59,60,61,182}){require(content.pendingMapTickets.contains(map),"Training admission does not unlock later rescue or tank consumers");}
            for(const auto& [id,flow]:runtime.at("flows").items()){
                const bool implemented=flow.value("implementation_ticket",93)==93||id=="m54-e5-p1"||id=="m56-e5-p1";
                require(content.catalog.at("flows").at(id).at("runtime_enabled").get<bool>()==implemented,"Training and the two source-invoked pursuer door flows are enabled without opening later story flows");
            }
            std::ifstream input(path),oldInput(std::filesystem::absolute(argv[3]));const auto original=nlohmann::json::parse(input),old=nlohmann::json::parse(oldInput);
            const auto oldContent=loadCombatContent(std::filesystem::absolute(argv[3]));require(oldContent.fullStage=="training"&&!oldContent.full->at("runtime").contains("status_definitions"),"The previous approved complete training package remains readable unchanged");
            const auto fixture=std::filesystem::temp_directory_path()/("nobihaza-status 中文-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}} cleanup{fixture};
            std::filesystem::copy(path.parent_path(),fixture,std::filesystem::copy_options::recursive);
            for(int variation=0;variation<9;++variation){auto bad=original;
                if(variation==0){bad["f4full"]["runtime"]["player_recovery"].erase("ce113");}
                else if(variation==1){bad["cues"].erase(recovery.at("wait").get<std::string>());}
                else if(variation==2){bad["cues"].erase(selection.at("else").get<std::string>());}
                else if(variation==3){bad["f4full"]["runtime"]["status_definitions"].erase("state8");}
                else if(variation==4){bad["f4full"]["state_contract"]["player_conditions"]["new_fields"].erase("neural_paralysis");}
                else if(variation==5){bad["f4full"]["runtime"]["player_recovery"]["ce113"]["restore_route_selection"]["else"]=selection.at("then");}
                else if(variation==6){bad["f4full"]["state_contract"]=old.at("f4full").at("state_contract");}
                else if(variation==7){bad=old;bad["f4full"]["state_contract"]=original.at("f4full").at("state_contract");}
                else{bad["f4full"]["runtime"]["status_definitions"]["state8"]["source_aliases"]={148};}
                std::ofstream(fixture/"malformed.json")<<bad.dump();bool rejected=false;std::string rejection;
                try{loadCombatContent(fixture/"malformed.json");}catch(const std::exception& error){rejected=true;rejection=error.what();}
                require(rejected,"Missing status/cue, merged routes, switch aliases and both state/projection mismatch directions are rejected");
                if(variation==6||variation==7){require(rejection==(fixture/"malformed.json").string()+": Unreviewed full F4 state declaration","An otherwise approved projection rejects only its mismatched state binding");}
            }
            std::cout<<"Reviewed State8 training admission, separate routes and exact state/projection pairing passed\n";return 0;
        }
        if(argc==3&&std::string(argv[2])=="--underground"){
            require(content.fullStage=="underground"&&content.maps.size()==83&&content.objects.size()==1207,"Audited underground data is cumulative without replacing complete training");
            require(content.maps.at(66).width==49&&content.maps.at(66).height==40&&content.maps.at(77).width==20&&content.maps.at(77).height==15,"Different original underground sizes use the shared map reader");
            const auto& underground=content.full->at("runtime").at("underground");
            require(underground.at("encounters").size()==16&&underground.at("flows").size()==114,"Complete finite underground definitions remain available for later consumers");
            for(const auto& [enemy,manager]:std::map<std::string,std::string>{{"m66-e17","m66-e15"},{"m67-e13","m67-e11"},{"m68-e19","m68-e17"},{"m69-e9","m69-e7"},{"m75-e6","m75-e4"},{"m76-e4","m76-e2"},{"m77-e4","m77-e2"}}){
                const auto& lifecycle=underground.at("encounters").at(enemy).at("runtime_profile").at("presentation_lifecycle");
                require(lifecycle.at("manager_object")==manager&&content.objects.at(manager).map==content.objects.at(enemy).map,"Every reviewed underground body manager belongs to its actual source actor's map");
                require(lifecycle.at("terminal_pages")==nlohmann::json::array({4,5}),"Both original S160/S585 empty manager pages are retained without inventing an erase");
            }
            for(const auto& [map,ticket]:underground.at("map_implementation_tickets").items()){
                const int id=std::stoi(map);
                require(content.pendingMapTickets.at(id)==ticket,"Underground maps keep their independent implementation gates");
                GameSession session(content.items,content.entries.at("natural").session,content.facts);
                bool rejected=false;try{content.makeScene(id,{1,1},Direction::Down,session);}catch(const std::exception&){rejected=true;}
                require(rejected,"Reading new media cannot open unfinished gameplay");
            }
            require(content.expansion.mapMusic.at("66")==underground.at("map_music").at("66"),"Underground music reaches the existing scene directory");
            for(const auto& [id,flow]:underground.at("flows").items()){
                require(!content.catalog.at("flows").at(id).at("runtime_enabled").get<bool>(),"New finite flows are explicitly gated");
            }
            auto state=content.entries.at("natural").session;state.eventTimer={EventTimerSource::Power,5400,true,false,false};
            state.conditions.poisoned=true;state.conditions.poisonWaitTicks=180;validateFullSession(content,state);
            const auto fixture=std::filesystem::temp_directory_path()/("nobihaza-underground 中文-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}} cleanup{fixture};
            std::filesystem::copy(path.parent_path(),fixture,std::filesystem::copy_options::recursive);
            std::ifstream input(path);const auto original=nlohmann::json::parse(input);
            auto bad=original;bad["stage"]="training";bad["capabilities"]["delivered_stage"]="training";
            std::ofstream(fixture/"malformed.json")<<bad.dump();
            bool rejected=false;try{loadCombatContent(fixture/"malformed.json");}catch(const std::exception&){rejected=true;}
            require(rejected,"A cumulative underground package cannot disguise its delivered stage as training");
            for(int variation=0;variation<5;++variation){auto bad=original;
                if(variation==0){bad["f4full"]["runtime"]["underground"]["encounters"]["m66-e17"]["type"]="unsupported";}
                else if(variation==1){bad["maps"]["4"]["grid"]["width"]=999;}
                else if(variation==2){bad["stage"]="parking";bad["capabilities"]["delivered_stage"]="parking";}
                else if(variation==3){bad["f4full"]["runtime"]["underground"]["encounters"]["m77-e4"]["runtime_profile"]["presentation_lifecycle"]["manager_object"]="m52-e2";}
                else{bad["f4full"]["runtime"]["underground"]["encounters"]["m66-e17"]["runtime_profile"]["presentation_lifecycle"]["terminal_pages"]=nlohmann::json::array({4});}
                std::ofstream(fixture/"malformed.json")<<bad.dump();rejected=false;
                try{loadCombatContent(fixture/"malformed.json");}catch(const std::exception&){rejected=true;}
                require(rejected,"Unknown behavior, inherited changes, later stages and incorrect manager bindings or terminal pages are rejected");
            }
            std::filesystem::remove(fixture/original.at("images").at("f4ug-animation-sheet-22").at("file").get<std::string>());
            rejected=false;try{loadCombatContent(fixture/"f4full.json");}catch(const std::exception&){rejected=true;}
            require(rejected,"Missing underground animation media cannot fall back to the inherited sheet");
            state.eventTimer.source=EventTimerSource::GuardFirst;rejected=false;
            try{validateFullSession(content,state);}catch(const std::exception&){rejected=true;}
            require(rejected,"Underground content cannot restore an undelivered parking guard timer");
            std::cout<<"Cumulative underground reader and explicit pending capabilities passed\n";return 0;
        }
        const auto& map=content.maps.at(51);
        require(map.width==52&&map.height==34&&map.tileSize==16,"original training exterior uses its own grid");
        require(map.passageMasks.size()==52*34&&content.terrainIds.at(51).size()==52*34,"passage and terrain cover the complete map");
        require(content.objects.at("m51-e2").tile==TilePosition{25,17}&&content.objects.at("m51-e7").tile==TilePosition{43,13},"new stable object identities retain original locations");
        GameSession session(content.items,content.entries.at("natural").session,content.facts);
        const auto& door=content.objects.at("m51-e7");
        require(content.activeVariant(door,session)->page==2,"locked door uses highest matching default page");
        session.setFlag(163,true);require(content.activeVariant(door,session)->page==3,"unlock fact selects the original later page");
        require(content.messages.at("m51-e7-p2-c7").message.lines.size()==3,"locked door message keeps two blank instant lines");
        const auto& music=content.full->at("runtime").at("map_music").at("51");
        const auto resource=music.at("sound").at("resource").get<std::string>();
        require(content.media.at("audio").at(resource).at("initial_sample_compatibility")==17829,"MIDI playback preserves reviewed first note offset");
        require(content.expansion.mapMusic.contains("51")&&content.expansion.mapMusic.at("51")==music,"Reviewed map-entry music reaches the shared scene consumer");
        require(content.pendingMapTickets.at(51)==93&&!content.transitions.at("m20-e3-p4").at("runtime_enabled").get<bool>(),"media preparation does not unlock the unfinished training flow");
        bool rejected=false;try{content.makeScene(51,{10,28},Direction::Down,session);}catch(const std::exception&){rejected=true;}
        require(rejected,"shared scene entry rejects pending map capability");
        if(argc==3){
            require(content.maps.at(52).width==29&&content.maps.at(52).height==21,"shooting range retains its original different dimensions");
            const auto& pursuer=content.enemies.at("m52-e4");
            require(pursuer.map==52&&pursuer.sourceOrder==4&&pursuer.deadFlag==158&&pursuer.rules.at("defense")==30,"real pursuer binds stable source identity and original attributes");
            require(pursuer.rules.at("hitbox_offset_pixels")==nlohmann::json::array({8,-5})&&pursuer.rules.at("hitbox_half_pixels")==nlohmann::json::array({8,15}),"pursuer receiver preserves original feet offset and independent half-height");
            require(pursuer.rules.at("completion_flow")=="m52-e4-p16"&&!content.catalog.at("flows").at("m52-e4-p16").at("runtime_enabled").get<bool>(),"bound complete original reward flow remains gated until93");
            GameSession selecting(content.items,content.entries.at("natural").session,content.facts);selecting.setFlag(156,true);
            require(content.activeVariant(content.objects.at("m52-e25"),selecting)->page==1,"original expired timer page is retained");
            selecting.setFlag(164,true);require(content.activeVariant(content.objects.at("m52-e25"),selecting)->page==2,"failure suppresses the timer trigger");
            selecting.setFlag(162,true);require(content.activeVariant(content.objects.at("m52-e25"),selecting)->page==3,"training completion has higher priority than failure and timer");
            require(content.pendingMapTickets.at(52)==93,"adding enemy metadata alone cannot unlock natural training");
            const auto& profile=pursuer.rules.at("runtime_profile");
            if(profile.contains("autonomy")){
                require(profile.at("autonomy").at("modes").at("wander").at("selector").at("output_by_random")==nlohmann::json::array({0,1,1}),"Original selector retains zero and folds both positive rolls to slash");
                require(profile.at("side_contacts").at("restore_mode")==3&&!profile.at("side_contacts").at("wait_for_route_completion").get<bool>(),"Source side contact submits a bounded player route without pretending a lethal bite");
                require(profile.at("blood").at("body_offset_from_worldfoot_pixels")==nlohmann::json::array({0,-21})&&profile.at("burn").at("interval_ticks")==6,"Reviewed common media preserve body anchor and six separate wait steps");
                require(content.routes.contains(profile.at("side_contacts").at("restore_route_cue").get<std::string>()),"Reviewed player response route reaches the existing finite route directory");
            }
            if(std::string(argv[2])=="--training"){
                require(content.maps.size()==70&&content.objects.size()==974,"Real cumulative training metadata retains inherited content");
                for(int id:{42,45,51,52,53,54,55,56,57,58,59,60,61,182}){
                    const int ticket=content.full->at("runtime").contains("map_implementation_tickets")?(id==182?95:(id>=51&&id<=58?93:94)):93;
                    require(content.pendingMapTickets.at(id)==ticket,"Each new map retains its independently confirmed later implementation consumer");
                }
                if(content.full->at("runtime").contains("map_implementation_tickets")){
                    require(content.transitions.at("m14-e21-p4").at("implementation_ticket")==94,"The old-room hot-water door belongs to its source consumer94");
                    require(content.transitions.at("m61-e13-p2").at("implementation_ticket")==94&&content.pendingMapTickets.at(182)==95,"Source rescue and destination story retain separate consumer gates");
                    require(content.transitions.at("m182-e3-p2").at("implementation_ticket")==95,"Returning from the story belongs to95");
                }
                const auto& extension=content.objects.at("m14-e21");
                require(std::any_of(extension.variants.begin(),extension.variants.end(),[](const auto& page){return page.page==4;}),"Reviewed old-room source page extensions reach the shared page selector");
                require(content.messages.at("f4full-ce154-c24").message.lines.front().raw=="\\>No.『\\V[388]』","Real file page preserves the session-bound password control");
                if(content.full->at("runtime").contains("player_recovery")){
                    const auto& runtime=content.full->at("runtime");
                    require(runtime.at("player_recovery").at("ce36").at("hp_thresholds")==nlohmann::json::array({{{"at_most",20},{"input_count",50}},{{"at_most",50},{"input_count",20}},{{"otherwise",true},{"input_count",10}}}),"Complete source recovery preserves all HP/input boundaries");
                    require(content.catalog.at("flows").at("m51-e9-p2").at("implementation_ticket")==95,"Tank source flow retains its actual later consumer instead of shared93");
                    for(int id=52;id<=58;++id){
                        const auto key="m"+std::to_string(id)+"-e4";
                        require(content.enemies.at(key).map==id&&content.enemies.at(key).rules.at("runtime_profile").contains("reappear"),"Every training room binds its own complete reappearance profile");
                    }
                    auto fresh=content.entries.at("natural").session;validateFullSession(content,fresh);
                    GameSession burning(content.items,fresh,content.facts);require(!burning.flag(574),"Fresh normal session does not inherit postgame sound suppression");
                    for(int id=301;id<=308;++id){
                        require(burning.variable(id)==0,"All source burn counters begin at zero");
                        burning.setVariable(id,24);validateFullSession(content,burning.exportState());
                        burning.setVariable(id,25);bool invalid=false;
                        try{validateFullSession(content,burning.exportState());}catch(const std::exception&){invalid=true;}
                        require(invalid,"A save cannot retain a counter after its source terminal reset threshold");burning.setVariable(id,0);
                    }
                    fresh.worldValues.erase(content.facts.variables.at(301));bool missing=false;
                    try{validateFullSession(content,fresh);}catch(const std::exception&){missing=true;}
                    require(missing,"A missing required burn counter is rejected rather than silently seeded during validation");
                }
            }
        }
        const auto fixture=std::filesystem::temp_directory_path()/("nobihaza-training 中文 path-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}} cleanup{fixture};
        std::filesystem::copy(path.parent_path(),fixture,std::filesystem::copy_options::recursive);
        std::ifstream input(path);const auto original=nlohmann::json::parse(input);
        const int variations=argc==3&&std::string(argv[2])=="--training"?(content.full->at("runtime").contains("map_implementation_tickets")?5:4):3;
        for(int variation=0;variation<variations;++variation){auto bad=original;
            if(variation==0){bad["objects"]["m51-e2"]["initial_tile"]={52,34};}
            else if(variation==1){bad["f4full"]["runtime"]["flows"]["m51-e4-p1"]["trigger"]="unreviewed";}
            else if(variation==2){bad["maps"]["4"]["grid"]["width"]=999;}
            else if(variation==4){bad["f4full"]["runtime"]["map_implementation_tickets"]["182"]=94;}
            else{auto& speed=bad["objects"]["m14-e21"]["variants"][0]["move_speed"];speed=speed==1?2:1;}
            std::ofstream(fixture/"malformed.json")<<bad.dump();
            rejected=false;try{loadCombatContent(fixture/"malformed.json");}catch(const std::exception&){rejected=true;}
            require(rejected,"unreviewed new or inherited training data is rejected");
        }
        const auto& profile=content.enemies.contains("m52-e4")?content.enemies.at("m52-e4").rules:nlohmann::json::object();
        if(profile.contains("runtime_profile")&&profile.at("runtime_profile").contains("autonomy")){
            for(int variation=0;variation<3;++variation){auto bad=original;auto& rule=bad["f4full"]["runtime"]["encounters"]["m52-e4"]["rules"]["runtime_profile"];
                if(variation==0){rule["autonomy"]["modes"]["wander"]["selector"]["output_by_random"]={1,1,2};}
                else if(variation==1){rule["blood"]["body_offset_from_worldfoot_pixels"]={0,0};}
                else{rule["original_commands"]={0};}
                std::ofstream(fixture/"malformed.json")<<bad.dump();
                rejected=false;try{loadCombatContent(fixture/"malformed.json");}catch(const std::exception&){rejected=true;}
                require(rejected,"Unreviewed AI selection, coordinate semantics and runtime scripts are rejected");
            }
        }
        std::filesystem::remove(fixture/original.at("maps").at("51").at("background").get<std::string>());
        rejected=false;try{loadCombatContent(fixture/"f4full.json");}catch(const std::exception&){rejected=true;}
        require(rejected,"missing new map media is rejected without falling back to an old image");
        std::cout<<"F4 training projection and capability boundary passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
