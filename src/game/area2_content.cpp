#include "full_content.h"
#include "content_hash.h"
#include <fstream>
#include <algorithm>
#include <sstream>
#include <stdexcept>
using Json=nlohmann::json;
namespace{
void need(bool ok,const char* why){if(!ok){throw std::runtime_error(why);}}
std::string verifiedFile(const std::filesystem::path& directory,const std::string& name,const std::string& sha){
    need(std::filesystem::path(name).filename()==name,"Invalid G2 resource path");const auto path=directory/name;
    need(!std::filesystem::is_symlink(path)&&std::filesystem::is_regular_file(path)&&std::filesystem::file_size(path)<=128*1024*1024,"Missing or oversized G2 resource");
    std::ifstream in(path,std::ios::binary);need(in.is_open(),"Cannot read G2 resource");std::string bytes=readContentBytes(in);need(contentSha256(bytes)==sha,"Changed G2 source or media");return bytes;
}
}
CombatContent loadArea2Content(const std::filesystem::path& path,const Json& data){
    const bool registration=data.at("content_compatibility")=="post-f5-g2-normal-6";
    const bool server=registration||data.at("content_compatibility")=="post-f5-g2-normal-5";
    const bool power=server||data.at("content_compatibility")=="post-f5-g2-normal-4";
    const bool hunters=power||data.at("content_compatibility")=="post-f5-g2-normal-3";
    const bool lickers=hunters||data.at("content_compatibility")=="post-f5-g2-normal-2";
    need(data.size()==4&&data.at("schema")=="nobihaza.g2.v1"&&(lickers||data.at("content_compatibility")=="post-f5-g2-normal-1"),"Unknown G2 identity");
    const auto& source=data.at("source");need(source.size()==2&&source.at("file")=="g2-g1-source.json"&&source.at("sha256")=="7eb832b77528cc3a470cd1de2b544725f3bb3389d5561af4d7fb912608da6d83","Unknown G2 source contract");
    const auto& p=data.at("projection");need(contentSha256(p.dump())==(registration?"77d2d972d4be7db22a142e1fbf86ba8cfbf17609704d799048a8d4de3603652b":server?"87262d4ae96baf564fd59c59d4f458daf29e6829f90a440baccf68b1ab515aef":power?"94e1cb0fdcff0e282e73f755fbfae833fe1a1574cc7b69b36e3730bc3d68c900":hunters?"b58ff4a5fd69910c156f0c9a21356ade9f3bd7eccc5b4691864a3ce2a50f2301":lickers?"85a5c763ac2bd7913d584c6b7b9170e544c7ac4ca5ab528c79f79f5b987585d6":"dc01c6676a992309c114c22b35d22ceb2ad993885ae47c6345b0030f0f01af9f"),"Unreviewed G2 projection");
    std::shared_ptr<const CombatContent> old,prior;
    if(lickers){
        std::istringstream input(verifiedFile(path.parent_path(),registration?"g2-05-source.json":server?"g2-04-source.json":power?"g2-03-source.json":hunters?"g2-02-source.json":"g2-01-source.json",registration?"ee8bad64af4b2b9f13074a46ab6fc6a21a57c6386296dc99039c345da6422627":server?"af42827ebb3acea94a365d7c9b696190dfcf2f140b514ab364c30eb797f1d844":power?"2160155e2a63be51460b75142c65ac57585fc70fa9c1d8bd1f5156f0dac7bb6a":hunters?"877deb9875803d176c1386b9077707d7632f36254552dc28cfcebf2f8e8a48bb":"f8f46ec6de4a2464fe6e83d6561332a7351304090bffcabb740d9098f69a1770"));
        prior=std::make_shared<const CombatContent>(loadCombatContentDefinition(path.parent_path()/"g2-01-source.json",parseCombatContentDefinition(input)));old=prior->legacySource;while(old&&old->catalog.contains("g2")){old=old->legacySource;}
        need(old&&old->compatibility=="post-f5-g1-normal-9","Wrong G2-02 inherited G1 content");
    }else{std::istringstream input(verifiedFile(path.parent_path(),source.at("file"),source.at("sha256")));old=std::make_shared<const CombatContent>(loadCombatContentDefinition(path.parent_path()/"g2-g1-source.json",parseCombatContentDefinition(input)));}
    auto c=*old;c.compatibility=data.at("content_compatibility");c.legacySource=prior?prior:old;

    for(const auto& [id,name]:p.at("facts").items()){need(c.facts.switches.emplace(std::stoi(id),name.get<std::string>()).second,"Duplicate G2 fact");}
    for(const auto* group:{"images","audio","cues","animations"}){for(const auto& [id,v]:p.at(group).items()){need(!c.hasMediaDefinition(group,id),"G2 shadows media");c.setMediaDefinition(group,id,v);}}
    auto sprites=*c.maps.begin()->second.sprites;
    const auto added=readSceneDefinitions(c,Json{{"font",p.at("font")},{"sprites",p.at("scene_sprites")},{"maps",p.at("maps")},{"objects",p.at("objects")}});
    for(const auto& [id,v]:added){sprites.insert_or_assign(id,v);}const auto sharedSprites=std::make_shared<const SpriteDefinitions>(sprites);for(auto& [id,map]:c.maps){map.sprites=sharedSprites;}
    for(const auto& [id,v]:p.at("messages").items()){need(c.messages.emplace(id,readCombatMessage(v,true)).second,"G2 shadows message");}
    for(const auto& [id,v]:p.at("cues").items()){need(!v.contains("runtime_support"),"Unsupported G2 cue");if(v.at("kind")=="forced_route"){need(c.routes.emplace(id,readRouteDefinition(v)).second,"Duplicate G2 route");}}
    for(const auto& [id,v]:p.at("flows").items()){need(c.catalog["flows"].emplace(id,v).second,"G2 shadows flow");}
    for(const auto& [id,v]:p.at("transitions").items()){
        if(c.transitions.contains(id)){const auto& prior=c.transitions.at(id);need(prior.at("handoff_only")&&prior.at("destination")==v.at("destination")&&prior.at("object")==v.at("object")&&prior.at("page")==v.at("page"),"G2 changes inherited door");c.transitions.at(id)=v;}
        else{c.transitions.emplace(id,v);}
    }
    for(const auto& [id,v]:p.at("map_music").items()){need(!c.expansion.mapMusic.contains(id),"G2 shadows map music");c.expansion.mapMusic[id]=v;}
    need(registration?p.at("items").size()==1&&p.at("items").contains("101"):p.at("items").empty(),"Unexpected G2 item override");
    for(const auto& [id,v]:p.at("items").items()){need(c.items.emplace(std::stoi(id),readItemDefinition(v)).second,"G2 shadows item");}
    need(c.catalog["readings"].emplace("item-190",p.at("mail_reading")).second,"G2 shadows reading");
    c.catalog["g2"]={{"facts",p.at("facts")},{"delivered",registration?6:server?5:power?4:hunters?3:lickers?2:1}};
    if(lickers){
        for(const auto& [map,rows]:p.at("tile_substitutions").items()){
            auto& scene=c.maps.at(std::stoi(map));
            for(const auto& row:rows){
                UpperTileReplacementDefinition replacement;replacement.fromIndex=row.at("from_index");replacement.toIndex=row.at("to_index");
                replacement.image=c.directory/c.media.at("images").at(row.at("image").get<std::string>()).at("file").get<std::string>();replacement.underlay=c.directory/c.media.at("images").at(row.at("underlay").get<std::string>()).at("file").get<std::string>();
                for(const auto& cell:row.at("cells")){const auto tile=cell.at("tile").get<std::array<int,2>>();need(tile[0]>=0&&tile[1]>=0&&tile[0]<scene.width&&tile[1]<scene.height,"Out-of-map lift replacement");replacement.tiles.push_back(tile);replacement.passageMasks.push_back(cell.at("mask"));}
                scene.upperTileReplacements.push_back(std::move(replacement));
            }
        }
        for(const auto& [id,row]:p.at("lickers").items()){
            const auto& object=c.objects.at(id);const auto& rules=row.at("rules");const auto& profile=rules.at("runtime_profile");
            for(const auto& number:profile.at("pages")){need(std::any_of(object.variants.begin(),object.variants.end(),[&](const auto& v){return v.page==number;}),"Missing G2 licker page");}
            for(const auto& cue:profile.at("cues")){need(c.cues.contains(cue.get<std::string>()),"Missing G2 licker cue");}
            EnemyBlueprint enemy;enemy.id=id;enemy.map=row.at("map");enemy.event=object.variants.front().definition.sourceEventId;enemy.sourceOrder=row.at("slot");enemy.sourceSlot=row.at("slot");enemy.deadFlag=row.at("dead_flag");if(!row.at("activation_flag").is_null()){enemy.activationFlag=row.at("activation_flag");}enemy.attack=row.at("attack");enemy.defense=row.at("defense");enemy.terminationDamage=row.at("termination");enemy.behavior=EnemyBehaviorKind::Licker;enemy.hitboxHalfPixels={8,8};enemy.rules=rules;
            need(c.expansion.enemies.emplace(id,std::move(enemy)).second,"Duplicate G2 licker");c.enemyBurnSlots[c.full->at("runtime").contains("combat_effects")?"combat_effects":"weapon_actions"][id]=row.at("slot");
        }
    }
    if(hunters){
        for(const auto& [id,row]:p.at("hunters").items()){
            const auto& rules=row.at("rules");need(rules.at("behavior")=="hunter"&&c.objects.contains(id)&&rules.at("barrel_receiver")==7,"Invalid area2 Hunter binding");
            need(c.enemies.emplace(id,EnemyDefinition{id,row.at("map"),row.at("event"),row.at("dead_fact"),false,rules}).second,"Duplicate area2 Hunter");
            c.enemyBurnSlots["weapon_actions"][id]=rules.at("burn_slot");
        }
        c.catalog["cached_environment_explosions"]=Json::array({p.at("barrel")});
    }
    if(power){c.catalog["nine_cell_panels"]=p.at("nine_cell_panels");}
    if(registration){c.catalog["registration"]=p.at("registration");}

    for(auto& [id,e]:c.entries){initializeFullSession(c,e.session);}
    for(const auto& [name,sha]:p.at("resources").items()){verifiedFile(c.directory,name,sha);}
    validateSceneContent(c,sprites);return c;
}
