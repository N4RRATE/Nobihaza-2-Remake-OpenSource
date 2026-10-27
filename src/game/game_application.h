#pragma once
#include "combat_scene.h"
#include "menu_controller.h"
#include "reading_controller.h"
#include "save_controller.h"
#include <algorithm>

// Owns one immutable catalogue, one session and its borrowing scene, in that order.
// Application UI consumes input before the scene; presentation only queries this facade.
class GameApplication{
public:
    GameApplication(CombatContent content,const std::string& entry,CombatScene::Random random={},CombatScene::PreparePresentation preparePresentation={});
    GameApplication(const GameApplication&)=delete;
    GameApplication& operator=(const GameApplication&)=delete;
    GameApplication(GameApplication&&)=delete;
    GameApplication& operator=(GameApplication&&)=delete;
    using PrepareRestorePresentation=std::function<void(const SceneDefinition&,const SavedPresentation&)>;
    void configureRestorePresentation(PrepareRestorePresentation prepare){prepareRestorePresentation_=std::move(prepare);}
    bool restore(const SaveSnapshot&,std::string& error);
    SaveSnapshot captureF5Recovery() const;
    bool newGame(std::string& error);
    bool enterBonusRoom(const PostClearRequest&,std::string& error);
    bool startNewCycle(const PostClearRequest&,std::string& error);
    void failOpening(const std::string& error);
    bool openingActive() const{return scene_&&scene_->openingActive();}
    std::string openingStage() const{return scene_?scene_->openingStage():std::string{};}
    void returnToTitle();
    bool hasGame() const{return bool(scene_);}
    void update(const InputFrame& input);
    void configureSaveDirectory(const std::filesystem::path&,SaveFiles::Fault fault={});
    void setPresentationState(SavedPresentation value){presentationState_=std::move(value);}
    std::optional<FileMenuView> fileMenuView() const{return saving_?saving_->fileView():loading_?std::optional<FileMenuView>{loading_->view()}:std::nullopt;}
    void restartDevelopmentEntry(const std::string& entry);
    const CombatContent& content() const{return content_;}
    const GameSession& session() const{if(!session_){throw std::logic_error("No active game");}return *session_;}
    const ExplorationScene& world() const{if(!scene_){throw std::logic_error("No active scene");}return scene_->world();}
    const SceneDefinition& definition() const{if(!scene_){throw std::logic_error("No active scene");}return scene_->definition();}
    const Dialogue& dialogue() const{return saving_?saving_->dialogue():reading_&&mode_==ApplicationMode::Reading?reading_->dialogue():scene_?scene_->dialogue():emptyDialogue_;}
    const DialogueView& inputPromptView() const{return saving_?saving_->promptView():scene_?scene_->inputPromptView():emptyPrompt_;}
    ForegroundKind foreground() const{return scene_?scene_->foreground():ForegroundKind::None;}
    std::uint64_t generation() const{return scene_?scene_->generation():0;}
    int mapId() const{return scene_?scene_->mapId():0;}
    GridPosition cameraCenter() const{return scene_?scene_->cameraCenter():GridPosition{160,120};}
    bool playerVisible() const{return scene_&&scene_->playerVisible();}
    bool reportMusicTicks(const MusicPlaybackIdentity& identity,int ticks){return scene_&&scene_->reportMusicTicks(identity,ticks);}
    bool reportMusicFirstPass(const MusicPlaybackIdentity& identity){return scene_&&scene_->reportMusicFirstPass(identity);}
    const std::optional<PanoramaState>& panorama() const{static const std::optional<PanoramaState> empty;return scene_?scene_->panorama():empty;}
    bool raised() const{return scene_&&scene_->raised();}
    std::vector<EnemyView> enemyStates() const{return scene_?scene_->enemyStates():std::vector<EnemyView>{};}
    bool aiming() const{return scene_&&scene_->aiming();}
    const std::optional<ExitRequest>& exitRequest() const{return scene_?scene_->exitRequest():emptyExit_;}
    std::vector<PresentationEvent> takePresentation();
    std::optional<FinalBossVictory> takeFinalBossVictory(){return scene_?scene_->takeFinalBossVictory():std::nullopt;}
    std::optional<PostClearRequest> postClearRequest() const{return scene_?scene_->postClearRequest():std::nullopt;}
    std::optional<EndingClearRequest> endingClearRequest() const{return scene_?scene_->endingClearRequest():std::nullopt;}
    std::optional<SaveRequest> takeSaveRequest(){return scene_?scene_->takeSaveRequest():std::nullopt;}
    ApplicationMode mode() const{return mode_;}
    bool directSaving() const{return saving_&&saving_->direct();}
    int menuSelection() const{return menuSelection_;}
    bool titleLoadEnabled() const{return titleLoadEnabled_;}
    int titleOpeningTicks() const{return std::max(0,titleOpeningTicks_-35);}
    int titleFadeTicks() const{return std::min(35,titleOpeningTicks_);}
    int menuSlot() const{return menuSlot_;}
    bool quitRequested() const{return quitRequested_;}
    std::vector<int> equipmentChoices() const;
    MenuView menuView() const{return menu_?menu_->view():MenuView{};}
    bool formalMenu() const{return bool(menu_); }
private:
    bool startOpening(SessionEntry,std::string& error,bool cycle=false);
    std::uint64_t attemptedCycleScene_{};
    void updateAlarm();
    void resetAlarm(){alarmNext_=0;alarmWait_=0;}
    std::size_t alarmNext_{};
    int alarmWait_{};
    std::vector<PresentationEvent> alarmPresentation_;
    void updateMenu(const InputFrame& input);
    void submitTool(const ToolUseRequest&);
    void submitMenuAction(const MenuResult&);
    void playUiSound(const std::string& kind);
    const CombatContent content_;
    CombatScene::Random random_;
    CombatScene::PreparePresentation preparePresentation_;
    std::unique_ptr<GameSession> session_;
    std::unique_ptr<CombatScene> scene_;
    std::unique_ptr<MenuController> menu_;
    std::unique_ptr<ReadingController> reading_;
    std::optional<MenuResult> actionAfterReading_;
    PrepareRestorePresentation prepareRestorePresentation_;
    Dialogue emptyDialogue_;DialogueView emptyPrompt_;std::optional<ExitRequest> emptyExit_;
    std::unique_ptr<FileMenu> loading_;
    std::unique_ptr<SaveFiles> saveFiles_;
    std::unique_ptr<SaveController> saving_;
    SavedPresentation presentationState_;
    ApplicationMode mode_{ApplicationMode::Playing};
    int menuSelection_{};
    int menuSlot_{1};
    bool quitRequested_{};
    bool pendingPresentationReset_{};
    bool titleLoadEnabled_{};
    int titleOpeningTicks_{};
    std::vector<PresentationEvent> presentation_;
};
