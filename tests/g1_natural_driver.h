#include "title_navigation.h"
#pragma once
#include "g0_natural_driver.h"
using namespace g0natural;
using Json=nlohmann::json;
struct Driver{
 std::function<void(const PresentationEvent&)> observe;
 std::function<void()> afterTick;
 GameApplication game;std::ofstream trace;int ticks{};std::vector<int> maps;std::vector<std::string> sounds;
 Driver(const CombatContent& c,const std::filesystem::path& root,const std::string& name="public-inputs.jsonl",CombatScene::Random random=safe,CombatScene::PreparePresentation prepare={}):game(c,"title",std::move(random),std::move(prepare)),trace(root/name){}
 void tick(InputFrame in={}){int held=0,pressed=0;for(std::size_t i=0;i<in.buttons.size();++i){held|=int(in.buttons[i].held)<<i;pressed|=int(in.buttons[i].pressed)<<i;}trace<<Json{{"step",ticks++},{"map",game.hasGame()?game.mapId():0},{"movement",in.movement?int(*in.movement):-1},{"held",held},{"pressed",pressed}}.dump()<<'\n';game.update(in);if(game.hasGame()&&(maps.empty()||maps.back()!=game.mapId())){maps.push_back(game.mapId());std::cerr<<"map="<<game.mapId()<<" step="<<ticks<<"\n";}for(const auto& e:game.takePresentation()){if(observe){observe(e);}if(e.kind==PresentationEvent::Kind::Sound){sounds.push_back(e.sound.resource);}if(e.kind==PresentationEvent::Kind::Notice){std::cerr<<e.id<<'\n';}}if(afterTick){afterTick();}}
 void settle(){int idle=0;for(int n=0;n<20000;++n){if(game.mode()!=ApplicationMode::Playing){return;}if(game.foreground()==ForegroundKind::None&&!game.dialogue().active()&&!game.world().player().motion.moving()){tick();if(++idle>=2&&game.foreground()==ForegroundKind::None){return;}continue;}idle=0;tick(n%8==0?key():InputFrame{});}throw std::runtime_error("Flow never settled");}
 void walk(TilePosition goal,std::function<bool()> done={}){const int m=game.mapId();for(int n=0;n<20000;++n){if(game.mapId()!=m||(done&&done())){return;}if(game.foreground()!=ForegroundKind::None||game.dialogue().active()||game.world().player().motion.moving()){tick(n%8==0?key():InputFrame{});continue;}if(game.world().player().motion.tile()==goal){return;}InputFrame in;in.movement=next(game.world(),goal);need(in.movement.has_value(),"No path on "+std::to_string(m)+" from "+std::to_string(game.world().player().motion.tile().x)+","+std::to_string(game.world().player().motion.tile().y)+" to "+std::to_string(goal.x)+","+std::to_string(goal.y)+" active="+std::to_string(game.session().flag(367)));tick(in);}throw std::runtime_error("Walk never settled");}
 void move(Direction d){InputFrame in;in.movement=d;tick(in);settle();}
 void actAt(TilePosition tile,Direction direction){walk(tile);move(direction);tick(key());settle();}
 void load(int slot=1){game.returnToTitle();finishTitleOpening(game);tick(key());need(game.mode()==ApplicationMode::Loading,"Public Load absent");for(int n=1;n<slot;++n){tick(key(InputAction::Down));}tick(key());need(game.mode()==ApplicationMode::Playing,"Public Load rejected");settle();}
 void save(){walk({9,10});move(Direction::Up);tick(key());need(game.mode()==ApplicationMode::Saving,"Hall save request absent");for(int n=0;n<200;++n){tick();}tick(key());need(game.fileMenuView().has_value(),"Slots absent");tick(key(InputAction::Down));tick(key());need(game.mode()==ApplicationMode::Playing,"Hall save did not continue");}
 void upstairs(){walk({10,7});settle();need(game.mapId()==133,"Hall stairs not delivered");actAt({4,10},Direction::Left);need(game.mapId()==130,"Stone room not delivered");}
};
