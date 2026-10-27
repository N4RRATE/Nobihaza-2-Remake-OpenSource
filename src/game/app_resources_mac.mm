#include "app_resources.h"
#import <Foundation/Foundation.h>
#include <stdexcept>

std::filesystem::path bundledContentPath(){
    @autoreleasepool{
        NSBundle* bundle=[NSBundle mainBundle];
        if(![[[bundle bundleURL] pathExtension] isEqualToString:@"app"]){
            throw std::runtime_error("Default content requires a macOS .app bundle. Usage: nobihaza <scene.json|combat.json> [--entry name] [--frames N] [--save-dir path]");
        }
        NSURL* resources=[bundle resourceURL];
        if(!resources){throw std::runtime_error("Application bundle has no Resources directory");}
        return std::filesystem::path([[resources path] fileSystemRepresentation])/"runtime"/"content.json";
    }
}
