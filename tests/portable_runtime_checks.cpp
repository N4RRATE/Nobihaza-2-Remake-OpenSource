#include "game_application.h"
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv){try{
    if(argc!=3){return 2;}
    GameApplication game(loadCombatContent(argv[1]),"title");
    game.configureSaveDirectory(argv[2]);
    if(game.mode()!=ApplicationMode::Title||game.hasGame()){throw std::runtime_error("Title created a gameplay session");}
    game.restartDevelopmentEntry("natural");
    const auto before=game.world().player().motion.tile();
    InputFrame move;move.movement=Direction::Down;
    for(int tick=0;tick<8;++tick){game.update(move);}
    move.movement=Direction::Left;
    for(int tick=0;tick<8;++tick){game.update(move);}
    if(game.world().player().motion.tile()==before){throw std::runtime_error("Development entry did not move through public input");}
    // Reuse the existing natural pickup route; this scene enables menu control after pickup.
    InputFrame confirm;confirm.buttons[static_cast<int>(InputAction::Confirm)]={true,true,false};
    game.update(confirm);
    for(int tick=0;tick<2200&&game.foreground()!=ForegroundKind::None;++tick){game.update(tick%30==29?confirm:InputFrame{});}
    if(!game.session().flag(52)){throw std::runtime_error("Natural pickup did not release menu control");}
    InputFrame cancel;cancel.buttons[static_cast<int>(InputAction::Cancel)]={true,true,false};
    game.update(cancel);
    if(game.mode()!=ApplicationMode::Equipment){throw std::runtime_error("Development entry did not open menu");}
    game.update(cancel);
    if(game.mode()!=ApplicationMode::Playing){throw std::runtime_error("Menu cancellation did not return player control");}
    std::cout<<"Portable content title/development movement/menu checks passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
