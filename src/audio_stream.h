#include <SFML/Audio.hpp>
#include <vector>
#include <mutex>
#include <deque>
#include <condition_variable>
#include <thread>
#include <atomic>

// ����� ��� ���������� ��������������� ������� ������
class ExternalDataStream1 : public sf::SoundStream {
public:
    // channels � ����� �������, sampleRate � ������� �������������
    // prebufferSize � ����������� ���������� ������� ��� ��������������� �����������
    ExternalDataStream1(unsigned int channels, unsigned int sampleRate, std::size_t prebufferSize = 44100);

    // ����� ��� ������ ����� ������� (������) �� ���������� �����.
    void pushSamples(const std::vector<sf::Int16>& samples);

protected:
    // onGetData ���������� �����-����������� ��� ��������� ���������� ����� ������.
    virtual bool onGetData(Chunk& data) override;

    virtual void onSeek(sf::Time timeOffset) override;

private:
    std::deque<sf::Int16> m_buffer;       // ��������� ����� ��� �������
    std::mutex m_mutex;
    std::condition_variable m_condVar;
    std::vector<sf::Int16> m_currentChunk; // ������� ���� ������� ��� onGetData
    unsigned int m_channels;
    unsigned int m_sampleRate;
    std::size_t m_maxBufferSamples;
    sf::Int16 m_lastSample;                // ��������� ����������� ��������, ��� ���������� ��� underflow
    std::thread m_producerThread;
    std::atomic<bool> m_running{ false };
};