#pragma once
#include "combat_presentation.h"
#include "scene_renderer.h"
#include "dialogue_renderer.h"
struct DoraRender{
 GameApplication& game;const CombatContent& content;sf::Font font;sf::RenderTexture surface{{320,240}};
 std::unique_ptr<CombatPresentation> presentation,staged;std::vector<PresentationEvent> pending;std::set<std::string> frames;std::filesystem::path root;
 DoraRender(GameApplication& r,const CombatContent& c,const std::filesystem::path& path):game(r),content(c),root(path){std::filesystem::create_directories(root);need(font.openFromFile(c.maps.begin()->second.fontPath),"Dora font missing");presentation=std::make_unique<CombatPresentation>(c,font);r.configureRestorePresentation([&](const SceneDefinition&,const SavedPresentation& saved){auto next=std::make_unique<CombatPresentation>(content,font);next->prepareRestoredState(saved);staged=std::move(next);});}
 void tick(){if(staged){presentation=std::move(staged);presentation->activateRestoredState();}presentation->update(pending);pending.clear();game.setPresentationState(presentation->saveState());}
 void frame(const std::string& name){if(!frames.insert(name).second){return;}const auto center=game.cameraCenter();surface.setView(sf::View({float(center.x),float(center.y)},{320,240}));surface.clear(sf::Color::Black);presentation->drawBackground(surface,game);SceneRenderer renderer(game.definition());renderer.draw(surface,game.world(),presentation->characterFlashes(),game.playerVisible(),presentation->saturation());presentation->draw(surface,game);DialogueRenderer dialogue(font);dialogue.setMedia(presentation->images(),presentation->palette());dialogue.draw(surface,game.dialogue().view());surface.display();auto pixels=surface.getTexture().copyToImage();need(pixels.saveToFile(root/(name+".png")),"Dora offscreen pixels failed");}
};
