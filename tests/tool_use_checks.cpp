#include "game_application.h"
#include <iostream>
#include <fstream>
#include "expanded_content_validation.h"
#include <stdexcept>
namespace{
void require(bool b,const char* s){if(!b){throw std::runtime_error(s);}}
InputFrame key(InputAction a=InputAction::Confirm){InputFrame f;f.buttons[static_cast<int>(a)].pressed=true;return f;}
void settle(GameApplication& g){for(int n=0;n<5000;++n){g.update({});if(g.foreground()==ForegroundKind::None&&!g.world().player().motion.moving()){return;}}throw std::runtime_error("Tool setup did not settle");}
}
int main(int argc,char** argv){try{
 if(argc!=2){return 2;}auto c=loadCombatContent(argv[1]);auto e=c.entries.at("defeated-claimed");
 e.map=12;e.playerTile={28,8};e.direction=Direction::Up;e.pages.clear();e.positions.clear();e.directions.clear();e.through.clear();
 e.positions["m12-e9"]={28,7};e.session.flags[66]=e.session.flags[67]=e.session.flags[68]=true;e.session.inventory[62]=1;
 c.entries["tool-fixture"]=e;GameApplication game(c,"tool-fixture");
 game.update(key());settle(game);game.update({});
 require(game.world().player().motion.tile()==TilePosition{28,6}&&game.session().flag(69),"Real ladder route reaches reviewed upper position");
 require(game.session().count(62)==0&&game.session().count(63)==1&&game.session().flag(70),"Entering the top tool area converts the one wrench to its active ID");
 game.update(key(InputAction::Cancel));require(game.mode()==ApplicationMode::Equipment,"Top position permits original menu");
 game.update(key());require(game.menuView().page==MenuPage::Items,"Navigate actual items menu");
 for(int n=0;n<100;++n){const auto v=game.menuView();if(v.rows.at(v.selection).id==63){break;}game.update(key(InputAction::Right));}
 game.update(key());require(game.mode()==ApplicationMode::Playing&&game.foreground()==ForegroundKind::Content,"Use active wrench returns to actual map removal flow");
 require(game.session().flag(71)&&!game.session().flag(72),"Tool request commits before timed removal");
 for(int n=0;n<5000;++n){game.update(n%12==0?key():InputFrame{});if(game.foreground()==ForegroundKind::None){break;}}
 require(game.session().flag(72)&&!game.session().flag(70)&&game.session().count(62)==1&&game.session().count(63)==0,"Removal finishes once, retaining inactive wrench and control");
 // Without a co-located doorway, the same blocked helper retains source refusal.
 auto blocked=c;blocked.transitions.erase("m12-e14-p4");auto mounted=e;mounted.playerTile={28,6};mounted.positions["m12-e10"]={28,5};mounted.session=game.session().exportState();
 GameSession stillMounted(blocked.items,mounted.session,blocked.facts);CombatScene refused(blocked,stillMounted,mounted);
 refused.update(key());for(int n=0;n<120;++n){refused.update({});}
 require(refused.mapId()==12&&refused.world().player().motion.tile()==TilePosition{28,6}&&stillMounted.flag(69)&&stillMounted.flag(1)&&stillMounted.flag(18),"Blocked ladder helper without a doorway preserves mount and restores controls");
 game.update({});game.update(key());settle(game);require(game.mapId()==23&&!game.session().flag(69),"Next investigation enters actual ventilation map and leaves ladder state");
 // Returning through the real right-hand vent restores ordinary floor state.
 InputFrame exitSouth;exitSouth.movement=Direction::Down;game.update(exitSouth);game.update(key());settle(game);require(game.mapId()==12&&game.world().player().motion.tile()==TilePosition{28,7},"Map23 exit returns to Map12");
 require(game.session().count(62)==1&&game.session().flag(72),"Vent and tool persist across return");
 auto savedEntry=e;savedEntry.map=22;savedEntry.playerTile={10,5};savedEntry.positions.clear();
 GameSession saved(c.items,game.session().exportState(),c.facts);CombatScene saveScene(c,saved,savedEntry);
 auto snapshot=captureSave(c,saveScene,{saveScene.generation(),"m22-e2",""},{});
 auto restored=prepareRestore(c,decodeSave(c,encodeSave(c,snapshot)));
 require(restored.session->flag(72)&&restored.session->flag(71)&&restored.session->count(62)==1,"Removed bars and request history survive legitimate save/Load");
 auto bad=snapshot;bad.session.worldFlags[c.facts.switches.at(68)]=false;bad.session.inventory[62]=0;bool rejected=false;
 try{encodeSave(c,bad);}catch(const std::exception&){rejected=true;}require(rejected,"Removed bars without obtaining the tool must not be accepted");
 GameSession context(c.items,e.session,c.facts);CombatScene scene(c,context,e);
 scene.update(key());for(int n=0;n<5000;++n){scene.update({});if(scene.foreground()==ForegroundKind::None&&!scene.world().player().motion.moving()){break;}}scene.update({});
 auto request=scene.requestToolUse();require(bool(request),"Scene offers a contextual tool request");
 auto wrong=*request;wrong.page+=1;require(!scene.useTool(wrong)&&!context.flag(71),"Wrong page cannot submit");
 wrong=*request;wrong.target="m12-e9";require(!scene.useTool(wrong),"Wrong target cannot submit");
 scene.cancelToolUse();require(!scene.useTool(*request),"Cancelled menu request cannot submit later");
 auto fresh=scene.requestToolUse();require(fresh&&fresh->request!=request->request&&!scene.useTool(*request),"New menu gets a distinct request identity");
 scene.deactivate();require(!scene.useTool(*fresh)&&!context.flag(72),"Destroyed scene rejects old tool request");
 GameApplication leave(c,"tool-fixture");leave.update(key());settle(leave);leave.update({});
 leave.update(key(InputAction::Cancel));leave.update(key(InputAction::Cancel));require(!leave.session().flag(71),"Menu cancellation does not submit original switch");
 InputFrame south;south.movement=Direction::Down;leave.update(south);leave.update(key());settle(leave);leave.update({});
 require(!leave.session().flag(69)&&!leave.session().flag(70)&&leave.session().count(62)==1&&leave.session().count(63)==0,"Leaving upper tool area restores disabled form before any use");
 // Merely forging the active item on the floor does not grant a scene request.
 auto floor=e;floor.playerTile={27,7};floor.session.inventory[62]=0;floor.session.inventory[63]=1;floor.session.flags[70]=true;
 GameSession grounded(c.items,floor.session,c.facts);CombatScene wrongHeight(c,grounded,floor);
 require(!wrongHeight.requestToolUse(),"Wrong height and position cannot offer a tool request");
 // Independent 46 terrain route, now driven through actual movement and pushes.
 auto natural=e;natural.playerTile={4,8};natural.positions.clear();c.entries["push-route-fixture"]=natural;
 GameApplication walking(c,"push-route-fixture");
 const std::vector<std::pair<TilePosition,TilePosition>> route={{{4,8},{31,8}},{{5,8},{31,8}},{{6,8},{31,8}},{{7,8},{31,8}},{{8,8},{31,8}},{{9,8},{31,8}},{{10,8},{31,8}},{{11,8},{31,8}},{{11,9},{31,8}},{{11,10},{31,8}},{{11,11},{31,8}},{{11,12},{31,8}},{{11,13},{31,8}},{{11,14},{31,8}},{{11,15},{31,8}},{{11,16},{31,8}},{{11,17},{31,8}},{{12,17},{31,8}},{{13,17},{31,8}},{{14,17},{31,8}},{{15,17},{31,8}},{{16,17},{31,8}},{{17,17},{31,8}},{{18,17},{31,8}},{{19,17},{31,8}},{{20,17},{31,8}},{{21,17},{31,8}},{{22,17},{31,8}},{{23,17},{31,8}},{{24,17},{31,8}},{{25,17},{31,8}},{{26,17},{31,8}},{{27,17},{31,8}},{{28,17},{31,8}},{{28,16},{31,8}},{{28,15},{31,8}},{{28,14},{31,8}},{{28,13},{31,8}},{{28,12},{31,8}},{{28,11},{31,8}},{{28,10},{31,8}},{{28,9},{31,8}},{{28,8},{31,8}},{{29,8},{31,8}},{{30,8},{31,8}},{{30,7},{31,8}},{{31,7},{31,8}},{{32,7},{31,8}},{{32,8},{31,8}},{{31,8},{30,8}},{{30,8},{29,8}},{{29,8},{28,8}},{{28,8},{27,8}},{{28,9},{27,8}},{{27,9},{27,8}},{{27,8},{27,7}},{{26,8},{27,7}},{{26,7},{27,7}},{{27,7},{28,7}}};
 const auto ladderSprite=findCharacter(walking.world(),"m12-e9").spriteId;
 for(std::size_t n=1;n<route.size();++n){
  const auto target=route[n].first;const auto from=walking.world().player().motion.tile();std::optional<Direction> toward;
  for(int d=0;d<4;++d){if(adjacentTile(from,static_cast<Direction>(d))==target){toward=static_cast<Direction>(d);}}
  require(bool(toward),"Audited route has adjacent movement");InputFrame move;move.movement=toward;walking.update(move);settle(walking);
  const auto ladder=findCharacter(walking.world(),"m12-e9");require(walking.world().player().motion.tile()==target&&ladder.motion.tile()==route[n].second,"Real collision and push match the independently audited route");
  require(ladder.spriteId==ladderSprite&&ladder.graphicFacing()==Direction::Left,"All six pushes retain ladder graphics");
 }
 walking.update(key());settle(walking);walking.update({});require(walking.session().count(63)==1,"Natural six-push route reaches usable wrench area");
 auto varied=c;const auto rename=[&](const std::string& old,const std::string& replacement){
  auto node=varied.objects.extract(old);node.key()=replacement;node.mapped().id=replacement;
  for(auto& v:node.mapped().variants){v.definition.id=replacement;}varied.objects.insert(std::move(node));
  for(auto& [id,f]:varied.catalog["flows"].items()){if(f["object"]==old){f["object"]=replacement;}}
 };
 rename("m12-e14","fixture-vent");varied.expansion.tool["vent_object"]="fixture-vent";
 auto changed=e;GameSession changedState(varied.items,e.session,varied.facts);CombatScene changedScene(varied,changedState,changed);
 changedScene.update(key());for(int n=0;n<5000;++n){changedScene.update({});if(changedScene.foreground()==ForegroundKind::None&&!changedScene.world().player().motion.moving()){break;}}changedScene.update({});
 auto changedRequest=changedScene.requestToolUse();require(changedRequest&&changedRequest->target=="fixture-vent"&&changedScene.useTool(*changedRequest),"Tool target identity comes from content");
 require(!changedScene.useTool(*changedRequest),"Tool request commits at most once");
 auto disabled=c;auto hidden=disabled.objects.at("m12-e15").variants.back();hidden.page=99;hidden.flags.clear();hidden.inventoryItem.reset();hidden.definition.trigger=0;
 disabled.objects.at("m12-e15").variants.push_back(hidden);GameSession disabledState(disabled.items,e.session,disabled.facts);CombatScene disabledScene(disabled,disabledState,e);
 disabledScene.update(key());for(int n=0;n<5000;++n){disabledScene.update({});if(disabledScene.foreground()==ForegroundKind::None&&!disabledScene.world().player().motion.moving()){break;}}disabledScene.update({});
 require(disabledState.count(62)==1&&disabledState.count(63)==0,"A superseding inactive event page cannot convert tools");
 auto reading=c;reading.entries["tool-fixture"].session.inventory[180]=1;
 GameApplication book(reading,"tool-fixture");book.update(key());settle(book);book.update({});book.update(key(InputAction::Cancel));book.update(key());
 for(int n=0;n<100&&book.menuView().rows.at(book.menuView().selection).id!=180;++n){book.update(key(InputAction::Right));}book.update(key());
 for(int n=0;n<6000&&book.mode()!=ApplicationMode::Equipment;++n){book.update(n%3==0?key():InputFrame{});book.takePresentation();}
 require(book.mode()==ApplicationMode::Equipment,"Manual returns to menu at the tool area");book.update(key());
 for(int n=0;n<100&&book.menuView().rows.at(book.menuView().selection).id!=63;++n){book.update(key(InputAction::Right));}book.update(key());
 require(book.mode()==ApplicationMode::Reading&&!book.session().flag(71),"Tool waits for the preceding reading cleanup");bool erased=false;
 for(int n=0;n<120&&book.mode()!=ApplicationMode::Playing;++n){book.update({});for(const auto& event:book.takePresentation()){erased=erased||event.id=="ce151-c79";}}
 require(erased&&book.mode()==ApplicationMode::Playing&&book.foreground()==ForegroundKind::Content&&book.session().flag(71),"Reading clears its curtain before the map tool submission");
 std::ifstream source(argv[1]);auto data=nlohmann::json::parse(source);
 const auto rejects=[&](nlohmann::json broken){bool invalid=false;try{validateExpandedContent(c,broken);}catch(const std::exception&){invalid=true;}require(invalid,"Malformed tool relationships rejected at the content boundary");};
 for(const auto* name:{"active_area","vent_object","ladder_object"}){auto broken=data;broken["f4"]["tool"][name]="m22-e2";rejects(broken);}
 auto broken=data;broken["f4"]["tool"]["return_areas"][0]="m22-e2";rejects(broken);
 broken=data;broken["f4"]["tool"]["ladder_object"]="m12-e14";rejects(broken);
 broken=data;broken["f4"]["staged_flows"]["vent-remove"]["object"]="m12-e9";rejects(broken);
 std::cout<<"Tool checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
