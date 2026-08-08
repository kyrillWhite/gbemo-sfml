#include "engine.h"

using namespace sf;

Engine::Engine()
    : scale(1)
{
}

void Engine::Initialize(int W, int H, int scale, const char* name)
{
	srand(time(0));
	setlocale(LC_ALL, "RUS");
	ContextSettings settings;
	settings.antialiasingLevel = 0;
	window.create(VideoMode(W * scale, H * scale), name, Style::Titlebar | Style::Close, settings);
	font.loadFromFile("assets/Roboto-Regular.ttf");
	backgroundColor = Color(0, 0, 0, 0);

	this->scale = scale;
	texture.create(W, H);
	sprite = sf::Sprite(texture);
}

void Engine::Update()
{
	Event event;
	while (window.pollEvent(event))
	{
		switch (event.type) {
		case Event::KeyPressed:
			switch (event.key.code) {
			case Keyboard::Escape:
				window.close();
				break;
			}
			break;
		case Event::Closed:
			window.close();
			break;
		}
	}

	// UPDATE
}

void Engine::Clear()
{
	window.clear(backgroundColor);
}



void Engine::Draw(unsigned char* pixels)
{
	texture.update(pixels);
	sprite.setScale(scale, scale);
	window.draw(sprite);
	window.display();
}
