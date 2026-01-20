#include <bootleg/game.hpp>

void boot::Game::update(){
    for(auto & window : this->windows){
        window->update(*this);
    }
}
void boot::Game::draw(){
    BeginDrawing();
    ClearBackground(BLACK);
    for(auto & window : this->windows){
        window->draw(*this);
    }
    EndDrawing();
}
