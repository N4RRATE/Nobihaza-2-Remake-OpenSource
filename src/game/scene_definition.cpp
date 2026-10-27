#include "scene_definition.h"

#include <nlohmann/json.hpp>
#include <cstddef>
#include <fstream>
#include <stdexcept>
#include <string>
#include <set>

namespace{
    nlohmann::json readSceneJson(const std::filesystem::path& scenePath){
        std::ifstream file(scenePath);
        if(!file.is_open()){
            throw std::runtime_error("Cannot open scene file: " + scenePath.string());
        }

        try{
            auto data = nlohmann::json::parse(file);
            if(file.bad()){
                throw std::runtime_error("Cannot read scene file: " + scenePath.string());
            }
            return data;
        }catch(const nlohmann::json::parse_error& error){
            throw std::runtime_error(
                "Invalid JSON in " + scenePath.string() + ": " + error.what());
        }
    }

    const nlohmann::json& sceneField(
        const nlohmann::json& data, const std::string& fieldPath){
        try{
            return data.at(nlohmann::json::json_pointer(fieldPath));
        }catch(const nlohmann::json::exception& error){
            throw std::runtime_error(fieldPath + ": " + error.what());
        }
    }

    int scenePositiveInteger(
        const nlohmann::json& data, const std::string& fieldPath, int maximum){
        const auto& value = sceneField(data, fieldPath);

        if(!value.is_number_integer()){
            throw std::runtime_error(fieldPath + ": expected an integer");
        }

        if(value < 1 || value > maximum){
            throw std::runtime_error(
                fieldPath + ": expected a value from 1 to " + std::to_string(maximum));
        }

        return value.get<int>();
    }

    int sceneIndex(
        const nlohmann::json& data, const std::string& fieldPath, int count){
        const auto& value = sceneField(data, fieldPath);

        if(!value.is_number_integer()){
            throw std::runtime_error(fieldPath + ": expected an integer index");
        }

        if(value < 0 || value >= count){
            throw std::runtime_error(fieldPath + ": index out of range");
        }

        return value.get<int>();
    }

    bool sceneBoolean(const nlohmann::json& data, const std::string& path){
        const auto& value = sceneField(data,path);
        if(!value.is_boolean()){ throw std::runtime_error(path + ": expected a boolean"); }
        return value.get<bool>();
    }

    std::string sceneString(const nlohmann::json& data, const std::string& path){
        const auto& value = sceneField(data, path);
        if(!value.is_string() || value.get_ref<const std::string&>().empty()){
            throw std::runtime_error(path + ": expected a non-empty string");
        }
        return value.get<std::string>();
    }

    std::string sceneDirection(const nlohmann::json& data, const std::string& path){
        const auto value = sceneString(data, path);
        if(value != "up" && value != "right" && value != "down" && value != "left"){
            throw std::runtime_error(path + ": expected up/right/down/left");
        }
        return value;
    }

    MessageDisplay readMessageDisplay(const nlohmann::json& data,const std::string& name,
                                       const std::string& rawName){
        MessageDisplay result;
        result.transparent = sceneBoolean(data,name + "/transparent");
        const auto position = sceneString(data,name + "/position");
        if(position == "top"){result.position = 0;}
        else if(position == "middle"){result.position = 1;}
        else if(position == "bottom"){result.position = 2;}
        else{throw std::runtime_error(name + "/position: unsupported position");}
        result.fixedPosition = sceneBoolean(data,name + "/fixed_position");
        result.continueEvents = sceneBoolean(data,name + "/continue_events");
        const auto expected = nlohmann::json::array({int(result.transparent),result.position,
                                                    int(!result.fixedPosition),int(result.continueEvents)});
        if(sceneField(data,rawName) != expected){throw std::runtime_error(rawName + ": inconsistent display options");}
        return result;
    }
    MessageDefinition readMessage(const nlohmann::json& data,int shortWait){
        MessageDefinition message;
        message.shortWaitUpdates = shortWait;
        message.display = readMessageDisplay(data,"/display","/options_before");
        message.restoreDisplay = readMessageDisplay(data,"/restore_display","/options_after");
        if(message.display.continueEvents || !message.display.fixedPosition){
            throw std::runtime_error("/display: automatic placement and continuing autonomous events are not implemented");
        }
        if(sceneField(data,"/face/name") != "" || sceneField(data,"/face/options") != nlohmann::json::array({0,0,0})){
            throw std::runtime_error("/face: portraits are not supported by exploration messages");
        }
        const auto& lines = sceneField(data,"/lines");
        if(!lines.is_array() || lines.empty() || lines.size() > 4){
            throw std::runtime_error("/lines: expected one page of 1..4 lines");
        }
        for(std::size_t i=0;i<lines.size();++i){
            const auto& line = lines[i];
            const auto prefix = "/lines/" + std::to_string(i);
            try{
                MessageLine parsed;
                if(!sceneField(line,"/raw").is_string()){throw std::runtime_error("/raw: expected text");}
                parsed.raw = sceneField(line,"/raw").get<std::string>();
                const auto& tokens = sceneField(line,"/tokens");
                if(!tokens.is_array()){throw std::runtime_error("/tokens: expected array");}
                std::string reconstructed;
                int width{};
                for(std::size_t j=0;j<tokens.size();++j){
                    const auto& token = tokens[j];
                    const auto type = sceneString(token,"/type");
                    if(type == "text"){
                        const auto text = sceneString(token,"/text");
                        auto decoded = decodeMessageText(text);
                        for(auto c : decoded){width += messageCharacterWidth(c);}
                        parsed.tokens.push_back({MessageTokenKind::Text,std::move(decoded),0});
                        reconstructed += text;
                    }else if(type == "instant_line"){
                        parsed.tokens.push_back({MessageTokenKind::InstantLine,{},0}); reconstructed += "\\>";
                    }else if(type == "speed"){
                        const int value = scenePositiveInteger(token,"/value",2);
                        parsed.tokens.push_back({MessageTokenKind::Speed,{},value});
                        reconstructed += "\\S[" + std::to_string(value) + "]";
                    }else if(type == "short_wait"){
                        parsed.tokens.push_back({MessageTokenKind::ShortWait,{},0}); reconstructed += "\\.";
                    }else{throw std::runtime_error("/tokens/" + std::to_string(j) + ": unsupported control " + type);}
                }
                if(reconstructed != parsed.raw){throw std::runtime_error("/raw: does not match supported tokens");}
                if(width > 50){throw std::runtime_error("/tokens: line exceeds 50 logical text cells");}
                message.lines.push_back(std::move(parsed));
            }catch(const std::exception& e){throw std::runtime_error(prefix + ": " + e.what());}
        }
        return message;
    }

    SceneObjectDefinition readSceneObject(const nlohmann::json& data,
                                          const SceneDefinition& scene){
        SceneObjectDefinition object;
        object.id = sceneString(data, "/id");
        object.sourceEventId = scenePositiveInteger(data, "/source_event_id", 65535);
        const auto& tile = sceneField(data, "/tile");
        if(!tile.is_array() || tile.size() != 2){
            throw std::runtime_error("/tile: expected [x, y]");
        }
        object.tileX = sceneIndex(data, "/tile/0", scene.width);
        object.tileY = sceneIndex(data, "/tile/1", scene.height);
        object.direction = sceneDirection(data, "/direction");
        object.initialColumn = sceneIndex(data, "/initial_column", 3);
        object.layer = sceneIndex(data, "/layer", 3);
        object.through = sceneBoolean(data, "/through");
        object.overlapForbidden = sceneBoolean(data, "/overlap_forbidden");
        object.moveSpeed = scenePositiveInteger(data, "/move_speed", 6);
        object.animationType = sceneIndex(data,"/animation_type",5);
        const auto& page = sceneField(data, "/source_page_id");
        object.active = !page.is_null();
        if(object.active){ (void)scenePositiveInteger(data, "/source_page_id", 65535); }
        const auto& sprite = sceneField(data, "/sprite");
        if(!sprite.is_null()){
            object.spriteId = sceneString(data, "/sprite");
            if(!scene.sprites->contains(*object.spriteId)){
                throw std::runtime_error("/sprite: missing sprite definition: " + *object.spriteId);
            }
        }
        object.trigger = sceneIndex(data,"/trigger",5);
        const auto geometry = sceneString(data,"/interaction/geometry");
        if(geometry == "same-tile"){object.interaction.geometry = InteractionGeometry::SameTile;}
        else if(geometry == "adjacent-facing"){object.interaction.geometry = InteractionGeometry::AdjacentFacing;}
        else if(geometry != "none"){throw std::runtime_error("/interaction/geometry: unsupported geometry");}
        const auto type = sceneString(data,"/interaction/type");
        const auto implementation = sceneString(data,"/interaction/implementation");
        if(type == "soldier-investigation" && implementation == "consumer-15"){
            object.interaction.kind = InteractionKind::Message;
            object.interaction.messageId = sceneString(data,"/interaction/message");
            if(!scene.messages.contains(object.interaction.messageId)){
                throw std::runtime_error("/interaction/message: unknown message ID");
            }
            if(object.trigger != 0 || object.interaction.geometry == InteractionGeometry::None){
                throw std::runtime_error("/interaction: message must be an Action investigation");
            }
        }else if((type == "gun-pickup" || type == "exit-contact") && implementation == "deferred"){
            object.interaction.kind = InteractionKind::Deferred;
        }else if(type != "none" || implementation != "empty" || geometry != "none"){
            throw std::runtime_error("/interaction: unsupported interaction");
        }
        return object;
    }

    SpriteDefinition readSpriteMetadata(
        const nlohmann::json& data, const std::filesystem::path& directory){
        SpriteDefinition sprite;

        const auto& file = sceneField(data, "/file");
        if(!file.is_string() || file.get<std::string>().empty()){
            throw std::runtime_error("/file: expected a non-empty path string");
        }
        sprite.texturePath = file.get<std::string>();
        if(sprite.texturePath.is_absolute()){
            throw std::runtime_error("/file: expected a relative path");
        }
        sprite.texturePath = directory / sprite.texturePath;

        const auto& size = sceneField(data, "/size_px");
        if(!size.is_array() || size.size() != 2){
            throw std::runtime_error("/size_px: expected [width, height]");
        }
        sprite.textureWidth = scenePositiveInteger(data, "/size_px/0", 72);
        sprite.textureHeight = scenePositiveInteger(data, "/size_px/1", 128);
        if(sprite.textureWidth != 72 || sprite.textureHeight != 128){
            throw std::runtime_error("/size_px: expected 72 x 128 for exploration.v1");
        }

        const auto& origin = sceneField(data, "/origin_px");
        if(!origin.is_array() || origin.size() != 2){
            throw std::runtime_error("/origin_px: expected [x, y]");
        }
        sprite.originX = sceneIndex(data, "/origin_px/0", 25);
        sprite.originY = sceneIndex(data, "/origin_px/1", 33);
        if(sprite.originX != 12 || sprite.originY != 32){
            throw std::runtime_error("/origin_px: expected [12, 32] for exploration.v1");
        }

        return sprite;
    }

    SpriteFrameDefinition readSpriteFrame(const nlohmann::json& data){
        SpriteFrameDefinition frame;
        const auto& direction = sceneField(data, "/direction");
        int row{};

        if(direction == "up"){
            row = 0;
        }else if(direction == "right"){
            row = 1;
        }else if(direction == "down"){
            row = 2;
        }else if(direction == "left"){
            row = 3;
        }else{
            throw std::runtime_error("/direction: expected up/right/down/left");
        }

        frame.direction = direction.get<std::string>();
        frame.column = sceneIndex(data, "/column", 3);

        const auto& rect = sceneField(data, "/rect_px");
        if(!rect.is_array() || rect.size() != 4){
            throw std::runtime_error("/rect_px: expected [left, top, width, height]");
        }

        frame.left = sceneIndex(data, "/rect_px/0", 72);
        frame.top = sceneIndex(data, "/rect_px/1", 128);
        frame.width = scenePositiveInteger(data, "/rect_px/2", 24);
        frame.height = scenePositiveInteger(data, "/rect_px/3", 32);

        if(frame.width != 24 || frame.height != 32 ||
           frame.left != frame.column * 24 || frame.top != row * 32){
            throw std::runtime_error("/rect_px: rectangle does not match direction and column");
        }

        return frame;
    }

    SpriteDefinition readSpriteDefinition(
        const nlohmann::json& data, const std::filesystem::path& directory){
        SpriteDefinition sprite = readSpriteMetadata(data, directory);
        const auto& frames = sceneField(data, "/frames");
        if(!frames.is_array() || frames.size() != 12){
            throw std::runtime_error("/frames: expected 12 frames");
        }
        std::vector<SpriteFrameDefinition> validated;

        for(std::size_t i = 0; i < frames.size(); ++i){
            try{
                const auto frame = readSpriteFrame(frames.at(i));
                for(const auto& existing : validated){
                    if(existing.direction == frame.direction && existing.column == frame.column){
                        throw std::runtime_error("duplicate direction and column");
                    }
                }
                validated.push_back(frame);
            }catch(const std::runtime_error& error){
                throw std::runtime_error(
                    "/frames/" + std::to_string(i) + ": " + error.what());
            }
        }

        sprite.frames=std::make_shared<const std::vector<SpriteFrameDefinition>>(std::move(validated));
        return sprite;
    }

}

SceneDefinition loadSceneDefinition(const std::filesystem::path& scenePath){
    const auto data = readSceneJson(scenePath);

    if(sceneField(data, "/schema") != "nobihaza.exploration.v1"){
        throw std::runtime_error("/schema: unsupported scene format");
    }

    if(sceneField(data, "/map/grid/origin") != "top-left" ||
       sceneField(data, "/map/grid/order") != "row-major" ||
       sceneField(data, "/map/grid/coordinates") != "zero-based"){
        throw std::runtime_error("/map/grid: unsupported coordinate convention");
    }

    SceneDefinition scene;
    scene.width = scenePositiveInteger(data, "/map/grid/width", 1024);
    scene.height = scenePositiveInteger(data, "/map/grid/height", 1024);
    scene.tileSize = scenePositiveInteger(data, "/map/grid/tile_size_px", 16);

    if(scene.tileSize != 16){
        throw std::runtime_error("/map/grid/tile_size_px: expected 16");
    }

    const auto& masks = sceneField(data,"/map/tiles/passage_masks");
    if(!masks.is_array() || masks.size() != static_cast<std::size_t>(scene.width) * scene.height){
        throw std::runtime_error("/map/tiles/passage_masks: expected one mask per tile");
    }
    for(std::size_t i = 0; i < masks.size(); ++i){
        scene.passageMasks.push_back(sceneIndex(data,"/map/tiles/passage_masks/" + std::to_string(i),16));
    }
    for(const auto& [name,bit] : std::map<std::string,int>{{"down",1},{"left",2},{"right",4},{"up",8}}){
        if(sceneIndex(data,"/map/tiles/direction_bits/" + name,16) != bit){
            throw std::runtime_error("/map/tiles/direction_bits/" + name + ": unsupported mapping");
        }
    }
    scene.controls.ordinaryMovement = sceneBoolean(data,"/entry/controls/ordinary_movement");
    scene.controls.ordinaryAction = sceneBoolean(data,"/entry/controls/ordinary_action");
    scene.controls.customMenu = sceneBoolean(data,"/entry/controls/custom_menu");
    scene.controls.combat = sceneBoolean(data,"/entry/controls/combat");
    scene.controls.defaultShortcutMenu = sceneBoolean(data,"/entry/controls/default_shortcut_menu");

    const auto& background = sceneField(data, "/map/background");
    const auto& foreground = sceneField(data, "/map/foreground");

    if(!background.is_string() || background.get<std::string>().empty()){
        throw std::runtime_error("/map/background: expected a non-empty path string");
    }
    if(!foreground.is_string() || foreground.get<std::string>().empty()){
        throw std::runtime_error("/map/foreground: expected a non-empty path string");
    }

    scene.backgroundPath = background.get<std::string>();
    scene.foregroundPath = foreground.get<std::string>();

    if(scene.backgroundPath.is_absolute()){
        throw std::runtime_error("/map/background: expected a relative path");
    }
    if(scene.foregroundPath.is_absolute()){
        throw std::runtime_error("/map/foreground: expected a relative path");
    }

    const auto directory = scenePath.parent_path();
    scene.backgroundPath = directory / scene.backgroundPath;
    scene.foregroundPath = directory / scene.foregroundPath;

    const auto& tile = sceneField(data, "/entry/player/tile");
    if(!tile.is_array() || tile.size() != 2){
        throw std::runtime_error("/entry/player/tile: expected [x, y]");
    }

    scene.player.tileX = sceneIndex(data, "/entry/player/tile/0", scene.width);
    scene.player.tileY = sceneIndex(data, "/entry/player/tile/1", scene.height);

    const auto& direction = sceneField(data, "/entry/player/direction");
    if(direction != "up" && direction != "right" &&
       direction != "down" && direction != "left"){
        throw std::runtime_error("/entry/player/direction: expected up/right/down/left");
    }
    scene.player.direction = direction.get<std::string>();

    const auto& spriteId = sceneField(data, "/entry/player/sprite");
    if(!spriteId.is_string() || spriteId.get<std::string>().empty()){
        throw std::runtime_error("/entry/player/sprite: expected a non-empty ID");
    }
    scene.player.spriteId = spriteId.get<std::string>();

    const auto& sprites = sceneField(data, "/sprites");
    if(!sprites.is_object() || !sprites.contains(scene.player.spriteId)){
        throw std::runtime_error(
            "/entry/player/sprite: missing sprite definition: " + scene.player.spriteId);
    }

    scene.player.initialColumn = sceneIndex(data, "/entry/player/initial_column", 3);
    scene.player.moveSpeed = scenePositiveInteger(data,"/entry/player/move_speed",6);
    scene.player.subunitsPerTile = scenePositiveInteger(data,"/animation/player/subunits_per_tile",256);
    scene.player.subunitsPerUpdate = scenePositiveInteger(data,"/animation/player/subunits_per_logic_update",256);
    if(scene.player.subunitsPerTile != 256 ||
       scene.player.subunitsPerUpdate != (1 << (1 + scene.player.moveSpeed)) ||
       scenePositiveInteger(data,"/animation/player/move_speed_level",6) != scene.player.moveSpeed ||
       sceneField(data,"/animation/player/unit") != "logic-updates-at-60Hz"){
        throw std::runtime_error("/animation/player: inconsistent speed, units or update frequency");
    }

    scene.walkingAnimation.movingThresholdUpdates = scenePositiveInteger(data,"/animation/player/moving_threshold_updates",60);
    scene.walkingAnimation.continuousThresholdUpdates = scenePositiveInteger(data,"/animation/player/continuous_threshold_updates",60);
    const auto& columns = sceneField(data,"/animation/player/logical_frame_columns");
    if(columns != nlohmann::json::array({0,1,2,1})){
        throw std::runtime_error("/animation/player/logical_frame_columns: expected [0,1,2,1]");
    }
    const auto expectedAnimation = animationForSpeed(scene.player.moveSpeed);
    if(scene.walkingAnimation.movingThresholdUpdates != expectedAnimation.movingThresholdUpdates ||
       scene.walkingAnimation.continuousThresholdUpdates != expectedAnimation.continuousThresholdUpdates){
        throw std::runtime_error("/animation/player: inconsistent animation thresholds for speed");
    }
    if(sceneIndex(data,"/entry/transient/animation_counter",1) != 0){
        throw std::runtime_error("/entry/transient/animation_counter: unsupported initial phase");
    }

    SpriteDefinitions definitions;
    for(const auto& item : sprites.items()){
        try{
            definitions.emplace(item.key(), readSpriteDefinition(item.value(), directory));
        }catch(const std::runtime_error& error){
            throw std::runtime_error("/sprites/" + item.key() + ": " + error.what());
        }
    }

    scene.sprites=std::make_shared<const SpriteDefinitions>(std::move(definitions));

    const int shortWait = scenePositiveInteger(data,"/message_controls/short_wait/at_speed_2_logic_updates",60);
    if(shortWait != 16 || scenePositiveInteger(data,"/message_controls/short_wait/logic_hz",60) != 60 ||
       sceneField(data,"/message_controls/instant_line/scope") != "current-line"){
        throw std::runtime_error("/message_controls: unsupported timing or scope");
    }
    const auto& messages = sceneField(data,"/messages");
    if(!messages.is_object()){throw std::runtime_error("/messages: expected dictionary");}
    for(const auto& item : messages.items()){
        try{ scene.messages.emplace(item.key(),readMessage(item.value(),shortWait)); }
        catch(const std::exception& e){throw std::runtime_error("/messages/" + item.key() + ": " + e.what());}
    }
    const auto& objects = sceneField(data, "/objects");
    if(!objects.is_array()){
        throw std::runtime_error("/objects: expected an array");
    }
    std::set<std::string> objectIds;
    std::set<int> eventIds;
    for(std::size_t i = 0; i < objects.size(); ++i){
        try{
            const auto object = readSceneObject(objects.at(i), scene);
            if(!objectIds.insert(object.id).second){
                throw std::runtime_error("/id: duplicate object ID: " + object.id);
            }
            if(!eventIds.insert(object.sourceEventId).second){
                throw std::runtime_error("/source_event_id: duplicate event ID");
            }
            // Empty graphics still represent objects; only the renderer skips drawing them.
            scene.objects.push_back(object);
        }catch(const std::runtime_error& error){
            throw std::runtime_error("/objects/" + std::to_string(i) + ": " + error.what());
        }
    }

    scene.tone.redPercent = sceneIndex(data, "/entry/screen_tone/red_percent", 101);
    scene.tone.greenPercent = sceneIndex(data, "/entry/screen_tone/green_percent", 101);
    scene.tone.bluePercent = sceneIndex(data, "/entry/screen_tone/blue_percent", 101);
    if(sceneIndex(data, "/entry/screen_tone/saturation_percent", 201) != 100){
        throw std::runtime_error("/entry/screen_tone/saturation_percent: only 100 is supported");
    }
    scene.fontPath = sceneString(data, "/font/file");
    if(scene.fontPath.is_absolute()){
        throw std::runtime_error("/font/file: expected a relative path");
    }
    scene.fontPath = directory / scene.fontPath;

    return scene;
}
