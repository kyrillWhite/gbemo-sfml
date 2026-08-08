#include "audio_stream.h"

// channels Ц число каналов, sampleRate Ц частота дискретизации
// prebufferSize Ц минимальное количество сэмплов дл€ предварительной буферизации
ExternalDataStream1::ExternalDataStream1(
    unsigned int channels,
    unsigned int sampleRate,
    std::size_t prebufferSize
)
    : m_channels(channels), m_sampleRate(sampleRate)
{
    initialize(channels, sampleRate);
    m_lastSample = 0;
}

// ћетод дл€ подачи новых сэмплов (чанков) во внутренний буфер.
void ExternalDataStream1::pushSamples(const std::vector<sf::Int16>& samples) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto s : samples) {
            m_buffer.push_back(s);
        }
    }
    m_condVar.notify_one();
}


// onGetData вызываетс€ аудио-подсистемой дл€ получени€ следующего блока данных.
bool ExternalDataStream1::onGetData(Chunk& data) {
    const std::size_t blockSize = 1024; // фиксированный размер блока
    std::vector<sf::Int16> output(blockSize);

    {
        std::unique_lock<std::mutex> lock(m_mutex);
        // ≈сли данных меньше, чем blockSize, ждем короткое врем€ (например, 5 мс)
        //if (m_buffer.size() < blockSize)
        //    m_condVar.wait_for(lock, std::chrono::milliseconds(5));

        std::size_t available = m_buffer.size();
        for (std::size_t i = 0; i < blockSize; ++i) {
            if (i < available) {
                output[i] = m_buffer.front();
                m_lastSample = output[i];
                m_buffer.pop_front();
            }
            else {
                // ≈сли данных недостаточно, заполн€ем оставшуюс€ часть последним значением
                output[i] = m_lastSample;
            }
        }
    }

    // —охран€ем сформированный блок во внутреннем векторе, чтобы данные оставались доступными до следующего вызова
    m_currentChunk = std::move(output);
    data.samples = m_currentChunk.data();
    data.sampleCount = m_currentChunk.size();
    return true;
}

void ExternalDataStream1::onSeek(sf::Time timeOffset) {
    // –еализаци€ перемотки не требуетс€
}