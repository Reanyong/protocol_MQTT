// MqttMessageQueue.h - 추가 최적화
#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>

// MQTT Message Structure (기존과 동일)
struct MqttMessage
{
    std::string topic;
    std::string payload;
    CTime timestamp;
    int payloadLength;

    MqttMessage() : payloadLength(0), timestamp(CTime::GetCurrentTime()) {}

    MqttMessage(const char* t, const char* p, int len)
        : topic(t ? t : ""), payload(p ? std::string(p, len) : ""),
        payloadLength(len), timestamp(CTime::GetCurrentTime())
    {
    }

    // Copy constructor
    MqttMessage(const MqttMessage& other)
        : topic(other.topic), payload(other.payload),
        timestamp(other.timestamp), payloadLength(other.payloadLength)
    {
    }

    // Assignment operator
    MqttMessage& operator=(const MqttMessage& other)
    {
        if (this != &other) {
            topic = other.topic;
            payload = other.payload;
            timestamp = other.timestamp;
            payloadLength = other.payloadLength;
        }
        return *this;
    }
};

// Thread-safe message queue class with enhanced optimization
class CMqttMessageQueue
{
private:
    std::queue<MqttMessage> m_messageQueue;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    bool m_shutdown;

    // Configurable values
    size_t m_maxQueueSize;

public:
    CMqttMessageQueue(size_t maxSize = 10000)
        : m_shutdown(false), m_maxQueueSize(maxSize)
    {
        TRACE("MqttMessageQueue created - Max size: %d\n", maxSize);
    }

    ~CMqttMessageQueue()
    {
        Shutdown();
        TRACE("MqttMessageQueue destroyed\n");
    }

    // Add message (Producer) - 최적화된 버전
    bool Push(const MqttMessage& msg)
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        if (m_shutdown) {
            return false;
        }

        // Handle full queue with improved strategy
        if (m_messageQueue.size() >= m_maxQueueSize) {
            TRACE("Warning: Message queue full! Size: %d, Removing old messages\n",
                m_messageQueue.size());

            // 더 적극적으로 오래된 메시지 제거 (20% 제거)
            size_t removeCount = m_maxQueueSize / 5;
            for (size_t i = 0; i < removeCount && !m_messageQueue.empty(); i++) {
                m_messageQueue.pop();
            }
        }

        m_messageQueue.push(msg);

        // Debug log with reduced frequency
        static int logCounter = 0;
        if (++logCounter % 200 == 0) { // 200번에 1번만 로그
            TRACE("Message added to queue - Current size: %d, Topic: %s\n",
                m_messageQueue.size(), msg.topic.c_str());
        }

        lock.unlock();
        m_condition.notify_one();
        return true;
    }

    // Get message (Consumer) - 기존과 동일
    bool Pop(MqttMessage& msg, int timeoutMs = 1000)
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        bool hasMessage = m_condition.wait_for(
            lock,
            std::chrono::milliseconds(timeoutMs),
            [this] { return !m_messageQueue.empty() || m_shutdown; }
        );

        if (hasMessage && !m_messageQueue.empty()) {
            msg = m_messageQueue.front();
            m_messageQueue.pop();
            return true;
        }

        return false; // Timeout or shutdown signal
    }

    // Shutdown queue
    void Shutdown()
    {
        TRACE("MqttMessageQueue shutdown signal sent\n");
        std::lock_guard<std::mutex> lock(m_mutex);
        m_shutdown = true;
        m_condition.notify_all();
    }

    // Check if shutdown
    bool IsShutdown() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_shutdown;
    }

    // Current queue size
    size_t Size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_messageQueue.size();
    }

    // Check if queue is empty
    bool Empty() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_messageQueue.empty();
    }

    // Queue status info
    void PrintStatus() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        TRACE("Queue Status - Size: %d/%d, Shutdown: %s\n",
            m_messageQueue.size(), m_maxQueueSize,
            m_shutdown ? "Yes" : "No");
    }

    // Change max queue size
    void SetMaxSize(size_t maxSize)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_maxQueueSize = maxSize;
        TRACE("Queue max size changed: %d\n", maxSize);
    }

    // 큐 상태 체크 (워커 스레드에서 안전하게 호출 가능)
    bool IsHealthy() const
    {
        try {
            std::lock_guard<std::mutex> lock(m_mutex);
            return !m_shutdown && m_messageQueue.size() < m_maxQueueSize;
        }
        catch (...) {
            return false;
        }
    }
};
