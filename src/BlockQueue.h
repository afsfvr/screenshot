#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QDeadlineTimer>

template<typename T>
class BlockQueue {
public:
    bool enqueue(T&& item) {
        QMutexLocker locker{&m_mutex};
        if (m_closed) return false;
        bool empty = m_queue.isEmpty();
        m_queue.enqueue(std::move(item));
        if (empty) {
            m_cond.wakeOne();
        }
        return true;
    }

    bool enqueue(const T& item) {
        QMutexLocker locker{&m_mutex};
        if (m_closed) return false;
        bool empty = m_queue.isEmpty();
        m_queue.enqueue(item);
        if (empty) {
            m_cond.wakeOne();
        }
        return true;
    }

    void close() {
        QMutexLocker locker{&m_mutex};
        m_closed = true;
        m_cond.wakeAll();
    }

    bool isClosed() const {
        QMutexLocker locker{&m_mutex};
        return m_closed;
    }

    bool dequeue(T *t) {
        QMutexLocker locker{&m_mutex};
        while (m_queue.isEmpty() && ! m_closed) {
            m_cond.wait(&m_mutex);
        }

        if (m_queue.isEmpty()) {
            return false;
        }
        *t = m_queue.dequeue();
        return true;
    }

    bool tryDequeue(T& item, int timeoutMs = 0) {
        QMutexLocker locker{&m_mutex};
        QDeadlineTimer deadline{timeoutMs};
        while (m_queue.isEmpty() && ! m_closed) {
            if (! m_cond.wait(&m_mutex, deadline.remainingTime())) {
                return false;
            }
        }

        if (m_queue.isEmpty()) {
            return false;
        }
        item = m_queue.dequeue();
        return true;
    }

    int size() const {
        QMutexLocker locker(&m_mutex);
        return m_queue.size();
    }

    bool isEmpty() const {
        QMutexLocker locker(&m_mutex);
        return m_queue.isEmpty();
    }

private:
    QQueue<T> m_queue;
    mutable QMutex m_mutex;
    QWaitCondition m_cond;
    bool m_closed = false;
};

#endif // BLOCKQUEUE_H
