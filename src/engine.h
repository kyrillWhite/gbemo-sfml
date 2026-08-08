#pragma once
#include <SFML/Graphics.hpp>
#include <ctime>

class Engine
{
public:
    Engine();

    sf::RenderWindow window;
    sf::Font font;
    sf::Color backgroundColor;
    sf::Texture texture;
    sf::Sprite sprite;

    void Initialize(int W, int H, int scale, const char* name);
    void Update();
    void Clear();
    void Draw(unsigned char* pixels);

private:
    int scale;
};
