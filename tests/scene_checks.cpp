#include "scene_definition.h"
#include "scene_renderer.h"
#include "dialogue_renderer.h"

#include <SFML/Graphics.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace{
    int checks{};
    void require(bool condition, const std::string& message){
        ++checks;
        if(!condition){
            throw std::runtime_error(message);
        }
    }
    struct TemporaryDirectory{
        std::filesystem::path path = std::filesystem::temp_directory_path() /
            ("nobihaza-scene-checks-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        TemporaryDirectory(){ std::filesystem::create_directory(path); }
        ~TemporaryDirectory(){ std::error_code error; std::filesystem::remove_all(path, error); }
    };
    void save(const sf::Image& image, const std::filesystem::path& path){
        if(!image.saveToFile(path)){ throw std::runtime_error("Cannot save fixture"); }
    }
    sf::Image render(const SceneDefinition& scene, const ExplorationScene* runtime = nullptr){
        ExplorationScene world(scene);
        SceneRenderer renderer(scene);
        sf::RenderTexture target({static_cast<unsigned>(scene.width * scene.tileSize),
                                  static_cast<unsigned>(scene.height * scene.tileSize)});
        target.clear(sf::Color::Black);
        renderer.draw(target, runtime ? *runtime : world);
        target.display();
        return target.getTexture().copyToImage();
    }
    void rejects(const std::function<void()>& action, const std::string& location){
        try{
            action();
        }catch(const std::exception& error){
            require(std::string(error.what()).find(location) != std::string::npos,
                    "Missing error location: " + location + ": " + error.what());
            return;
        }
        require(false, "Expected failure: " + location);
    }
}

int main(int argc, char* argv[]){
    try{
        if(argc < 2){ throw std::runtime_error("Expected exploration scene.json"); }
        const auto path = std::filesystem::absolute(argv[1]);
        const auto scene = loadSceneDefinition(path);
        SpriteDefinition retained;
        {
            auto sourceDirectory=*scene.sprites;
            sourceDirectory.at("player").frames=std::make_shared<const std::vector<SpriteFrameDefinition>>(*sourceDirectory.at("player").frames);
            retained=sourceDirectory.at("player");
            require(retained.frames==sourceDirectory.at("player").frames,"Copied definitions share immutable frame values");
        }
        const auto originalLeft=retained.frames->at(0).left;
        auto changedFrames=*retained.frames;changedFrames.at(0).left=originalLeft+1;
        auto variant=retained;variant.frames=std::make_shared<const std::vector<SpriteFrameDefinition>>(std::move(changedFrames));
        require(retained.frames.use_count()==1&&retained.frames->size()==12&&retained.frames->at(0).left==originalLeft&&variant.frames->at(0).left==originalLeft+1,"Destroyed source directory and independent variant preserve original frame values");
        {
            std::weak_ptr<const SpriteDefinitions> directory;
            std::unique_ptr<SceneRenderer> renderer;
            {
                auto transient=scene;
                transient.sprites=std::make_shared<const SpriteDefinitions>(*scene.sprites);
                directory=transient.sprites;renderer=std::make_unique<SceneRenderer>(transient);
            }
            require(!directory.expired(),"Renderer retains definitions after source scene is destroyed");
            ExplorationScene world(scene);sf::RenderTexture target({320,240});
            target.clear();renderer->draw(target,world);target.display();
            renderer.reset();require(directory.expired(),"Renderer releases its independent sprite directory");
        }
        require(scene.width == 20 && scene.height == 15 && scene.tileSize == 16, "Real grid");
        require(scene.objects.size() == 13 && scene.sprites->size() == 6, "Keep all objects/graphics");
        require(scene.player.tileX == 8 && scene.player.tileY == 5 &&
                scene.player.direction == "left" && scene.player.initialColumn == 1, "Real player entry");
        const auto object = [&](const std::string& id) -> const SceneObjectDefinition&{
            for(const auto& candidate : scene.objects){ if(candidate.id == id){ return candidate; } }
            throw std::runtime_error("Missing object " + id);
        };
        require(object("m4-e4").tileX == 7 && object("m4-e4").tileY == 5 &&
                object("m4-e4").spriteId == "event-04", "Soldier entry");
        require(object("m4-e10").tileY == 6 && object("m4-e10").layer == 0 &&
                object("m4-e10").initialColumn == 0, "Gun entry");
        require(!object("m4-e2").spriteId && !object("m4-e12").spriteId, "Keep empty objects");
        require(object("m4-e5").tileX == 7 && object("m4-e6").tileX == 7, "Post-cutscene positions");
        sf::Font font;
        require(font.openFromFile(scene.fontPath), "Real font loads");
        require(font.hasGlyph(U'失') && font.hasGlyph(U'…') && font.hasGlyph(U'　'), "Chinese glyphs");
        const auto real = render(scene);
        require(real.getSize() == sf::Vector2u(320, 240), "Real rendered size");
        if(argc == 3){ save(real, argv[2]); }

        TemporaryDirectory temp;
        nlohmann::json data;
        { std::ifstream input(path); input >> data; }
        // Keep relative file references valid after relocating this JSON.
        for(const auto& item : std::filesystem::directory_iterator(path.parent_path())){
            if(item.is_regular_file()){
                std::filesystem::copy_file(item.path(), temp.path / item.path().filename());
            }
        }
        const auto fixturePath = temp.path / "scene.json";
        const auto writeJson = [&](const nlohmann::json& value){
            std::ofstream output(fixturePath); output << value;
        };
        const auto invalid = [&](const std::function<void(nlohmann::json&)>& mutate,
                                 const std::string& location){
            auto bad = data;
            mutate(bad); writeJson(bad);
            rejects([&]{ (void)loadSceneDefinition(fixturePath); }, location);
        };
        invalid([](auto& d){ d["objects"] = nullptr; }, "/objects");
        invalid([](auto& d){ d["objects"][3]["tile"] = {20, 5}; }, "/objects/3: /tile/0");
        invalid([](auto& d){ d["objects"][3]["tile"] = {-1, 5}; }, "/objects/3: /tile/0");
        invalid([](auto& d){ d["objects"][3]["layer"] = 3; }, "/objects/3: /layer");
        invalid([](auto& d){ d["objects"][3]["sprite"] = "missing"; }, "/objects/3: /sprite");
        invalid([](auto& d){ d["objects"][3]["sprite"] = 4; }, "/objects/3: /sprite");
        invalid([](auto& d){ d["objects"][3]["direction"] = "north"; }, "/objects/3: /direction");
        invalid([](auto& d){ d["objects"][3]["initial_column"] = 3; }, "/objects/3: /initial_column");
        invalid([](auto& d){ d["objects"][3]["id"] = "m4-e1"; }, "/objects/3: /id");
        invalid([](auto& d){ d["objects"][3]["source_event_id"] = 1; }, "/objects/3: /source_event_id");
        invalid([](auto& d){ d["entry"]["screen_tone"]["red_percent"] = 101; }, "/red_percent");
        invalid([](auto& d){ d["entry"]["screen_tone"]["blue_percent"] = 90.5; }, "/blue_percent");
        invalid([](auto& d){ d["entry"]["screen_tone"]["saturation_percent"] = 90; }, "/saturation_percent");
        invalid([](auto& d){ d["font"]["file"] = ""; }, "/font/file");
        invalid([](auto& d){ d["sprites"]["player"]["frames"][11] = d["sprites"]["player"]["frames"][0]; }, "/frames/11");
        invalid([](auto& d){ d["sprites"]["player"]["frames"][0]["rect_px"] = {71,0,24,32}; }, "/frames/0");
        invalid([](auto& d){ d["entry"]["player"]["tile"][0] = 18446744073709551615ULL; }, "/entry/player/tile/0");
        invalid([](auto& d){ d["schema"] = "unknown"; }, "/schema");
        { std::ofstream output(fixturePath); output << "{broken"; }
        rejects([&]{ (void)loadSceneDefinition(fixturePath); }, "Invalid JSON");
        rejects([&]{ (void)loadSceneDefinition(temp.path / "missing.json"); }, "Cannot open");
        invalid([](auto& d){ d["map"]["tiles"]["passage_masks"].erase(0); }, "/map/tiles/passage_masks");
        invalid([](auto& d){ d["map"]["tiles"]["passage_masks"][0] = 16; }, "/map/tiles/passage_masks/0");
        invalid([](auto& d){ d["map"]["tiles"]["direction_bits"]["up"] = 1; }, "/map/tiles/direction_bits/up");
        invalid([](auto& d){ d["objects"][3]["through"] = 1; }, "/objects/3: /through");
        invalid([](auto& d){ d["objects"][3]["source_page_id"] = 0; }, "/objects/3: /source_page_id");
        invalid([](auto& d){ d["objects"][3]["move_speed"] = 7; }, "/objects/3: /move_speed");
        invalid([](auto& d){ d["entry"]["controls"]["ordinary_movement"] = "true"; }, "/entry/controls/ordinary_movement");
        invalid([](auto& d){ d["animation"]["player"]["subunits_per_logic_update"] = 64; }, "/animation/player");
        invalid([](auto& d){ d["animation"]["player"]["unit"] = "seconds"; }, "/animation/player");
        invalid([](auto& d){ d["animation"]["player"]["moving_threshold_updates"] = 0; }, "/animation/player/moving_threshold_updates");
        invalid([](auto& d){ d["animation"]["player"]["logical_frame_columns"] = {0,1,2,2}; }, "/animation/player/logical_frame_columns");
        invalid([](auto& d){ d["objects"][3]["animation_type"] = 5; }, "/animation_type");
        invalid([](auto& d){ d["messages"]["soldier-unconscious"]["lines"][2]["tokens"][1]["value"] = 9; }, "/messages/soldier-unconscious: /lines/2");
        invalid([](auto& d){ d["messages"]["soldier-unconscious"]["lines"][2]["tokens"][1]["type"] = "unknown"; }, "/tokens/1");
        invalid([](auto& d){ d["messages"]["soldier-unconscious"]["lines"][2]["raw"] = "lost control marks"; }, "/raw");
        invalid([](auto& d){ d["messages"]["soldier-unconscious"]["display"]["transparent"] = false; }, "/options_before");
        invalid([](auto& d){ d["messages"]["soldier-unconscious"]["face"]["name"] = "unknown"; }, "/face");
        invalid([](auto& d){ d["messages"]["soldier-unconscious"]["lines"] = nlohmann::json::array(); }, "/lines");
        invalid([](auto& d){ d["objects"][3]["interaction"]["message"] = "missing"; }, "/interaction/message");
        invalid([](auto& d){ d["objects"][3]["interaction"]["geometry"] = "anywhere"; }, "/interaction/geometry");
        invalid([](auto& d){ d["message_controls"]["short_wait"]["at_speed_2_logic_updates"] = 15; }, "/message_controls");
        invalid([](auto& d){ auto& m=d["messages"]["soldier-unconscious"]; m["display"]["fixed_position"]=false; m["options_before"][2]=1; }, "/display");
        invalid([](auto& d){ auto& m=d["messages"]["soldier-unconscious"]; m["display"]["continue_events"]=true; m["options_before"][3]=1; }, "/display");
        writeJson(data);
        require(loadSceneDefinition(fixturePath).fontPath == temp.path / "font.otf", "Relocated package");

        auto badMedia = scene;
        badMedia.backgroundPath = temp.path / "missing.png";
        rejects([&]{ (void)render(badMedia); }, "/map/background");
        badMedia = scene;
        auto badSprites=*scene.sprites;badSprites.at("event-01").texturePath=temp.path/"missing.png";
        badMedia.sprites=std::make_shared<const SpriteDefinitions>(std::move(badSprites));
        rejects([&]{ (void)render(badMedia); }, "/sprites/event-01/file");
        save(sf::Image({1,1}, sf::Color::White), temp.path / "wrong.png");
        badMedia = scene; badMedia.foregroundPath = temp.path / "wrong.png";
        rejects([&]{ (void)render(badMedia); }, "/map/foreground");
        badMedia=scene;badSprites=*scene.sprites;badSprites.at("player").texturePath=temp.path/"wrong.png";
        badMedia.sprites=std::make_shared<const SpriteDefinitions>(std::move(badSprites));
        rejects([&]{ (void)render(badMedia); }, "/sprites/player/file");

        // Synthetic colors make overlap, alpha and selected frame independently observable.
        auto sample = scene;
        sample.objects.clear(); sample.tone = {};
        sample.backgroundPath = temp.path / "background-test.png";
        sample.foregroundPath = temp.path / "foreground-test.png";
        save(sf::Image({320,240}, sf::Color(100,100,100)), sample.backgroundPath);
        save(sf::Image({320,240}, sf::Color::Transparent), sample.foregroundPath);
        auto sampleSprites=*sample.sprites;
        for(auto& [id, graphic] : sampleSprites){
            graphic.texturePath = temp.path / (id + "-test.png");
            save(sf::Image({72,128}, sf::Color::Transparent), graphic.texturePath);
        }
        sf::Image frames({72,128}, sf::Color::Transparent);
        for(unsigned row = 0; row < 4; ++row){
            for(unsigned column = 0; column < 3; ++column){
                const sf::Color color(static_cast<std::uint8_t>(30 + row * 40),
                                      static_cast<std::uint8_t>(40 + column * 50), 200);
                for(unsigned y = row * 32; y < (row+1)*32; ++y){
                    for(unsigned x = column*24; x < (column+1)*24; ++x){ frames.setPixel({x,y}, color); }
                }
            }
        }
        save(frames, sampleSprites.at("player").texturePath);
        auto reordered=*sampleSprites.at("player").frames;
        std::reverse(reordered.begin(), reordered.end());
        sampleSprites.at("player").frames=std::make_shared<const std::vector<SpriteFrameDefinition>>(std::move(reordered));
        sample.sprites=std::make_shared<const SpriteDefinitions>(std::move(sampleSprites));
        const std::string directions[]{"up","right","down","left"};
        for(int row = 0; row < 4; ++row){
            for(int column = 0; column < 3; ++column){
                sample.player.direction = directions[row]; sample.player.initialColumn = column;
                const auto image = render(sample);
                require(image.getPixel({136,80}) == sf::Color(30+row*40,40+column*50,200), "Frame by direction/column");
            }
        }
        sample.player.direction = "left"; sample.player.initialColumn = 1;
        auto image = render(sample);
        require(image.getPixel({124,64}) == sf::Color(150,90,200), "Player top-left from foot anchor");
        require(image.getPixel({147,95}) == sf::Color(150,90,200), "Player bottom-right");
        require(image.getPixel({123,64}) == sf::Color(100,100,100) &&
                image.getPixel({136,96}) == sf::Color(100,100,100), "Frame boundary");
        ExplorationScene moving(sample);
        InputFrame moveDown; moveDown.movement = Direction::Down;
        moving.update(moveDown);
        const auto movingImage = render(sample,&moving);
        require(movingImage.getPixel({124,64}) == sf::Color(100,100,100), "renderer follows runtime position rather than entry tile");
        require(movingImage.getPixel({136,97}) == sf::Color(110,90,200), "renderer follows runtime facing and smooth movement");
        auto overlay = object("m4-e4");
        overlay.tileX = 8; overlay.tileY = 5;
        save(sf::Image({72,128}, sf::Color::Red), sample.sprites->at("event-04").texturePath);
        sample.objects = {overlay};
        require(render(sample).getPixel({136,80}) == sf::Color(150,90,200), "Player tie ordering");
        sample.objects[0].tileY = 6;
        require(render(sample).getPixel({136,85}) == sf::Color::Red, "Lower same-layer object in front");
        sample.objects[0].layer = 0;
        require(render(sample).getPixel({136,85}) == sf::Color(150,90,200), "Below-layer behind player");
        sample.objects[0].layer = 2;
        sf::Image foreground({320,240}, sf::Color::Transparent);
        foreground.setPixel({136,85}, sf::Color::Blue);
        save(foreground, sample.foregroundPath);
        require(render(sample).getPixel({136,85}) == sf::Color::Red, "Above-layer over foreground");
        sample.objects[0].layer = 1;
        require(render(sample).getPixel({136,85}) == sf::Color::Blue, "Foreground over same-layer");
        sample.objects[0].spriteId.reset();
        require(render(sample).getPixel({136,84}) == sf::Color(150,90,200), "Null graphics not drawn");
        sample.objects[0].spriteId = "event-05";
        require(render(sample).getPixel({136,84}) == sf::Color(150,90,200), "Transparent graphics not opaque");
        sample.tone = {90,90,90};
        image = render(sample);
        require(image.getPixel({0,0}) == sf::Color(90,90,90), "Tone on background");
        require(image.getPixel({136,84}) == sf::Color(135,81,180), "Tone on player");
        require(image.getPixel({136,85}) == sf::Color(0,0,230), "Tone on foreground");

        const auto wide = makeSceneView({1000,600},{320.f,240.f}).getViewport();
        require(std::abs(wide.position.x - .1f) < .0001f && std::abs(wide.size.x - .8f) < .0001f,
                "Wide viewport pillarboxes");
        const auto tall = makeSceneView({600,1000},{320.f,240.f}).getViewport();
        require(std::abs(tall.position.y - .275f) < .0001f && std::abs(tall.size.y - .45f) < .0001f,
                "Tall viewport letterboxes");
        rejects([]{ (void)makeSceneView({0,600},{320.f,240.f}); }, "dimensions");
        sf::Font dialogueFont;
        require(dialogueFont.openFromFile(scene.fontPath),"load real dialogue font");
        DialogueRenderer dialogueRenderer(dialogueFont);
        ExplorationScene talking(scene);
        InputFrame inspect; inspect.buttons[static_cast<std::size_t>(InputAction::Confirm)].pressed = true;
        talking.update(inspect);
        for(int i=0;i<56;++i){talking.update({});}
        sf::RenderTexture uiTarget({960,720});
        uiTarget.setView(makeSceneView({960,720},{320,240}));
        const auto originalView = uiTarget.getView();
        uiTarget.clear(sf::Color::Black);
        dialogueRenderer.draw(uiTarget,talking.dialogue().view()); uiTarget.display();
        const auto renderedText = uiTarget.getTexture().copyToImage();
        int textPixels = 0; bool textInExpectedArea = true;
        for(unsigned y=0;y<720;++y){for(unsigned x=0;x<960;++x){
            if(renderedText.getPixel({x,y}) != sf::Color::Black){
                ++textPixels;
                textInExpectedArea &= x >= 340 && x < 620 && y >= 600 && y < 660;
            }
        }}
        require(textInExpectedArea,"text respects original spaces and third-line placement");
        require(textPixels > 300,"dialogue draws high-resolution Chinese glyphs");
        require(uiTarget.getView().getSize() == originalView.getSize(),"UI restores world view");
        static_assert(!std::is_copy_constructible_v<SceneRenderer>);
        static_assert(!std::is_move_constructible_v<SceneRenderer>);
        std::cout << checks << " scene checks passed\n";
        return 0;
    }catch(const std::exception& error){
        std::cerr << "Scene check failed: " << error.what() << '\n';
        return 1;
    }
}
