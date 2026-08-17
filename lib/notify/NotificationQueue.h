#pragma once

#include <stddef.h>
#include <stdint.h>

// Bounded FIFO of pending push messages, so event detection never waits for a
// slow or unreachable notification server.
//
// Contract:
//   - push() always accepts. When the queue is full the OLDEST message is
//     discarded and counted: an alert about what is happening now is worth
//     more than one about what happened a minute ago.
//   - Messages longer than MaxLength are truncated, never rejected.
//
// Pure logic: no hardware deps and no heap — the storage is inline, so a
// failing server cannot fragment the firmware's memory. Callers own thread
// safety.
template <size_t Capacity, size_t MaxLength>
class NotificationQueue
{
public:
    static_assert(Capacity > 0, "queue needs room for at least one message");
    static_assert(MaxLength > 1, "messages need room for text and a terminator");

    // False when an older message had to be discarded to make room.
    bool push(const char *message)
    {
        bool keptEverything = true;
        if (count_ == Capacity)
        {
            head_ = advance(head_);
            --count_;
            ++dropped_;
            keptEverything = false;
        }
        copy(slots_[(head_ + count_) % Capacity], MaxLength, message);
        ++count_;
        return keptEverything;
    }

    // Copies the oldest message into out and removes it; false when empty.
    bool pop(char *out, size_t outSize)
    {
        if (count_ == 0 || out == nullptr || outSize == 0)
        {
            return false;
        }
        copy(out, outSize, slots_[head_]);
        head_ = advance(head_);
        --count_;
        return true;
    }

    size_t size() const { return count_; }
    bool empty() const { return count_ == 0; }
    uint32_t dropped() const { return dropped_; }

private:
    static size_t advance(size_t index) { return (index + 1) % Capacity; }

    static void copy(char *destination, size_t destinationSize, const char *source)
    {
        size_t i = 0;
        for (; source != nullptr && source[i] != '\0' && i + 1 < destinationSize; ++i)
        {
            destination[i] = source[i];
        }
        destination[i] = '\0';
    }

    char slots_[Capacity][MaxLength] = {};
    size_t head_ = 0;
    size_t count_ = 0;
    uint32_t dropped_ = 0;
};
