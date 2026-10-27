#include "local_text_fixtures.h"
#include "combat_presentation.h"
#include "scene_renderer.h"
#include "dialogue_renderer.h"
#include "g1_piano_driver.h"
// Original room rendered offscreen; public investigation input, no user window.
int main(int argc,char**argv){try{
 need(argc==4,"Cold revision package source isolated-output");auto c=loadCombatContent(argv[1]);const auto saved=decodeSave(c,read(argv[2]));
 need(c.catalog.value("cold_room_revision",std::string{})=="g3-cold-room-v1","Cold correction not enabled");
 auto e=c.entries.at("post-pickup");e.map=172;e.playerTile={12,10};e.direction=Direction::Up;e.session=saved.session;e.session.flags[1]=true;e.session.flags[18]=true;e.positions.clear();e.pages.clear();e.directions.clear();e.through.clear();c.entries["cold-visual"]=e;
 GameApplication game(c,"cold-visual",safe);auto definition=game.definition();
 std::set<std::string> used{definition.player.spriteId};for(const auto& object:definition.objects){if(object.spriteId){used.insert(*object.spriteId);}}
 auto selectedSprites=*definition.sprites;std::erase_if(selectedSprites,[&](const auto& row){return !used.contains(row.first);});definition.sprites=std::make_shared<const SpriteDefinitions>(std::move(selectedSprites));SceneRenderer renderer(definition);
 sf::RenderTexture surface({320,240});surface.setView(sf::View({160,120},{320,240}));
 auto blank=definition;blank.objects.clear();ExplorationScene ground(blank);renderer.draw(surface,ground,{},false);surface.display();const auto floor=surface.getTexture().copyToImage();
 surface.clear();renderer.draw(surface,game.world(),{},false);surface.display();const auto bodies=surface.getTexture().copyToImage();
 for(int id:{5,6,7,8,9,10,11,12,16}){const auto name="m172-e"+std::to_string(id);const auto& actor=findCharacter(game.world(),name);need(actor.active&&actor.spriteId&&actor.animation.column()==1,"Source body missing from actual scene");const auto tile=actor.motion.tile();int changed=0;for(int y=std::max(0,(tile.y+1)*16-32);y<(tile.y+1)*16;++y){for(int x=tile.x*16-4;x<tile.x*16+20;++x){changed+=bodies.getPixel({unsigned(x),unsigned(y)})!=floor.getPixel({unsigned(x),unsigned(y)});}}need(changed>20,"Frozen body rendered no visible pixels: "+name);}
 auto root=std::filesystem::path(argv[3]);std::filesystem::create_directories(root);need(bodies.saveToFile(root/"nine-bodies.png"),"Cannot save bodies evidence");
 sf::Font font;need(font.openFromFile(definition.fontPath),"Missing font");CombatPresentation present(c,font);game.update({});present.update(game.takePresentation());
 surface.clear();renderer.draw(surface,game.world());present.draw(surface,game);surface.display();const auto fog=surface.getTexture().copyToImage();const auto a=bodies.getPixel({160,40}),b=fog.getPixel({160,40});need(b.r>a.r&&b.g>a.g&&b.b>a.b,"Cold fog did not lighten actual room pixels");need(fog.saveToFile(root/"cold-room.png"),"Cannot save cold room evidence");
 // Actual public Confirm at the source E11 body; original map/objects untouched.
 e.playerTile={14,10};e.direction=Direction::Up;c.entries["cold-inspect"]=e;GameApplication inspect(c,"cold-inspect",safe);CombatPresentation view(c,font);inspect.update({});view.update(inspect.takePresentation());inspect.update(key());view.update(inspect.takePresentation());need(inspect.dialogue().active(),"Cold public investigation missing");
 for(int n=0;n<800;++n){const auto& lines=inspect.dialogue().view().lines;if(lines.size()==3&&lines[2].find(decodeMessageText(localSourceText("g3_access_cold_visual.0")))!=std::u32string::npos){break;}inspect.update({});view.update(inspect.takePresentation());}
 const auto& lines=inspect.dialogue().view().lines;need(inspect.dialogue().active()&&lines.size()==3&&lines[2].find(decodeMessageText(localSourceText("g3_access_cold_visual.1")))!=std::u32string::npos,"Full source investigation text missing");
 surface.clear();renderer.draw(surface,inspect.world());view.draw(surface,inspect);surface.display();const auto noText=surface.getTexture().copyToImage();DialogueRenderer dialogue(font);dialogue.setMedia(view.images(),view.palette());dialogue.draw(surface,inspect.dialogue().view());surface.display();const auto text=surface.getTexture().copyToImage();int glyphPixels=0;for(unsigned y=160;y<240;++y){for(unsigned x=0;x<320;++x){glyphPixels+=text.getPixel({x,y})!=noText.getPixel({x,y});}}need(glyphPixels>50,"Investigation text produced no actual glyph pixels");need(text.saveToFile(root/"corpse-investigation.png"),"Cannot save investigation evidence");
 std::cout<<"Nine frozen bodies including zero-command E16 visible; original fog and public corpse dialogue actually rendered offscreen\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
