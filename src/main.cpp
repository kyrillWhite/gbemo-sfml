#include <iostream>
#include <iomanip>
#include "engine.h"
#include <chrono>
#include "emu.h"
// #include "audio_stream.h"
#include <functional>
#include <queue>
#include <mutex>
#include <cstring>
#include <SFML/Audio.hpp>

using namespace std;
using namespace sf;
using namespace chrono;

// globalTicks lives in the core (see libs/gbemo-core/src/globals.cpp); common.h
// declares it and we only read it here to pace the emulation.
bool speedUp = false;

void handleButtons(Emu *emu, Engine *engine)
{
    Event event;
    while (engine->window.pollEvent(event))
    {
        switch (event.type)
        {
        case Event::KeyPressed:
            switch (event.key.code)
            {
            case Keyboard::S:
                speedUp = true;
                break;
            case Keyboard::Z:
                emu->pressButton(Button::B_B);
                break;
            case Keyboard::X:
                emu->pressButton(Button::B_A);
                break;
            case Keyboard::Backspace:
                emu->pressButton(Button::B_SELECT);
                break;
            case Keyboard::Enter:
                emu->pressButton(Button::B_START);
                break;
            case Keyboard::Up:
                emu->pressButton(Button::B_UP);
                break;
            case Keyboard::Right:
                emu->pressButton(Button::B_RIGHT);
                break;
            case Keyboard::Down:
                emu->pressButton(Button::B_DOWN);
                break;
            case Keyboard::Left:
                emu->pressButton(Button::B_LEFT);
                break;
            case Keyboard::Escape:
                engine->window.close();
                break;
            }
            break;
        case Event::KeyReleased:
            switch (event.key.code)
            {
            case Keyboard::S:
                speedUp = false;
                break;
            case Keyboard::Z:
                emu->releaseButton(Button::B_B);
                break;
            case Keyboard::X:
                emu->releaseButton(Button::B_A);
                break;
            case Keyboard::Backspace:
                emu->releaseButton(Button::B_SELECT);
                break;
            case Keyboard::Enter:
                emu->releaseButton(Button::B_START);
                break;
            case Keyboard::Up:
                emu->releaseButton(Button::B_UP);
                break;
            case Keyboard::Right:
                emu->releaseButton(Button::B_RIGHT);
                break;
            case Keyboard::Down:
                emu->releaseButton(Button::B_DOWN);
                break;
            case Keyboard::Left:
                emu->releaseButton(Button::B_LEFT);
                break;
            case Keyboard::Escape:
                engine->window.close();
                break;
            }
            break;
        case Event::Closed:
            engine->window.close();
            break;
        }
    }
}

class ExternalDataStream : public sf::SoundStream
{
public:
    ExternalDataStream(unsigned int channels, unsigned int sampleRate)
        : m_channels(channels), m_sampleRate(sampleRate), m_currentChunk(nullptr)
    {
        initialize(channels, sampleRate);
        m_silentChunk = new i16[1024 * m_channels];
        memset(m_silentChunk, 0, 1024 * m_channels * sizeof(i16));
    }

    ~ExternalDataStream()
    {
        if (m_currentChunk)
        {
            delete[] m_currentChunk;
        }
        while (!m_sampleQueue.empty())
        {
            delete[] m_sampleQueue.front();
            m_sampleQueue.pop_front();
        }
        delete[] m_silentChunk;
    }

    void pushChunk(i16 *chunk)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_sampleQueue.size() < 10)
        {
            m_sampleQueue.push_back(chunk);
        }
    }

protected:
    virtual bool onGetData(Chunk &data) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_currentChunk)
        {
            delete[] m_currentChunk;
            m_currentChunk = nullptr;
        }
        if (!m_sampleQueue.empty())
        {
            m_currentChunk = m_sampleQueue.front();
            m_sampleQueue.pop_front();
            data.samples = m_currentChunk;
        }
        else
        {
            data.samples = m_silentChunk;
        }
        data.sampleCount = 1024 * m_channels;
        return true;
    }

    virtual void onSeek(sf::Time timeOffset) override
    {
        // Перемотка не реализована
    }

private:
    unsigned int m_channels;
    unsigned int m_sampleRate;

    // AudioChunk* m_currentChunk;
    std::deque<i16 *> m_sampleQueue;
    i16 *m_currentChunk;
    i16 *m_silentChunk;
    std::mutex m_mutex;
};

int main(int argc, char **argv)
{
    const char *romPath = argc > 1 ? argv[1] : "roms/rom.gb";

    std::cout << "Starting emulator...\n";
    auto emu = new Emu(true);

    ExternalDataStream stream(2, 44100);

    stream.play();

    std::cout << "Loading " << romPath << "\n";
    int res = emu->loadRom(romPath);

    if (res)
    {
        std::cout << "ROM load failed: " << res << "\n";
        return res;
    }
    std::cout << "ROM loaded successfully.\n";
    MonoColor *tiles = new MonoColor[384 * 64];
    u8 *debugPixels = new u8[128 * 192 * 4];
    Engine *debugTiles = new Engine();
    debugTiles->Initialize(128, 192, 4, "debug tiles");

    u8 *pixels = new u8[160 * 144 * 4];
    Engine *screen = new Engine();
    screen->Initialize(160, 144, 4, "main screen");

    long long counter1 = 0;
    long long expectedCount1 = 0, expectedCount2 = 0;
    double errorAcc = 0.0;

    sf::Clock globalClock;

    int pushedSamples = 0;
    const int sampleBufferSize = 1024 * stream.getChannelCount();
    i16 *sampleBuffer = new i16[sampleBufferSize];
    int sampleBufferI = 0;
    long long counter = 0;
    // sf::Clock globalClock;

    long long lastFrame = 0;

    while (true)
    {
        // if (!emu->tick()) continue;

        const double freq1 = 44100.0;
        const double freq2 = 4194304.0;

        double ratio = freq2 / freq1;
        const double interval1 = 1e6 / freq1;
        const double interval2 = 1e6 / freq2;

        long long elapsed = globalClock.getElapsedTime().asMicroseconds();
        expectedCount1 = static_cast<long long>(elapsed / interval1);
        expectedCount2 = static_cast<long long>(elapsed / interval2);

        while ((expectedCount1 - counter1) > 0)
        {
            counter1++;

            errorAcc += ratio;
            while (errorAcc >= 1.0 && (expectedCount2 - globalTicks) > 0)
            {
                emu->tick();
                errorAcc -= 1.0;
            }

            double phase = 0;
            sampleBuffer[sampleBufferI++] = emu->getLeftAudioSample();
            sampleBuffer[sampleBufferI++] = emu->getRightAudioSample();
            if (sampleBufferI == sampleBufferSize)
            {
                sampleBufferI = 0;
                stream.pushChunk(sampleBuffer);
                sampleBuffer = new i16[sampleBufferSize];
            }
        }
        while ((expectedCount2 - globalTicks) > ratio)
        {
            emu->tick();
        }

        if (globalTicks >= lastFrame + 17556)
        {
            lastFrame += 17556;
            // if (globalTicks % 17556 == 0) {
            emu->debugReadTiles(tiles);
            debugTiles->Update();

            for (int tileX = 0; tileX < 16; tileX++)
            {
                for (int tileY = 0; tileY < 24; tileY++)
                {
                    int tileOffset = (tileY * 16 + tileX) * 64;

                    for (int y = 0; y < 8; y++)
                    {
                        for (int x = 0; x < 8; x++)
                        {
                            int pixelIndex = tileOffset + y * 8 + x;
                            u8 color = (3 - tiles[pixelIndex]) * 85;

                            int screenX = tileX * 8 + x;
                            int screenY = tileY * 8 + y;
                            int bufferIndex = (screenY * 128 + screenX) * 4;

                            debugPixels[bufferIndex] = color;
                            debugPixels[bufferIndex + 1] = color;
                            debugPixels[bufferIndex + 2] = color;
                            debugPixels[bufferIndex + 3] = 255;
                        }
                    }
                }
            }
            debugTiles->Clear();
            debugTiles->Draw(debugPixels);

            MonoColor *buffer = emu->getVideoBuffer();
            handleButtons(emu, screen);

            for (int x = 0; x < 144; x++)
            {
                for (int y = 0; y < 160; y++)
                {
                    int offset = x + y * 144;
                    u8 color = (3 - buffer[offset]) * 85;

                    pixels[offset * 4 + 0] = color;
                    pixels[offset * 4 + 1] = color;
                    pixels[offset * 4 + 2] = color;
                    pixels[offset * 4 + 3] = 255;
                }
            }

            screen->Clear();
            screen->Draw(pixels);
        }
    }

    delete[] debugPixels;
    delete[] tiles;
    delete emu;
    delete debugTiles;
    return 0;
}
