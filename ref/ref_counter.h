#pragma once

#include "../commtypes.h"

#include <atomic>

namespace coid
{

/// @brief Basic reference counting class containt counter for weak and strong referenced.
class ref_counter
{
public: // methods only
    ref_counter() = default;

    ///// @brief Creates counter from the global pool
    ///// @param instance - reference to instance pointer
    //static void create(ref_counter*& instance)
    //{
    //    if (instance == nullptr)
    //    {
    //        instance = SINGLETON(coid::pool<ref_counter>).get_item();
    //    }
    //    else 
    //    {
    //        DASSERT(0); // this shouldn't happen
    //    }
    //}
    ///// @brief Returns the counter back to global the pool and clear the counter variable
    ///// @param instance - reference to instance pointer
    //static void destroy(ref_counter*& instance)
    //{
    //    if (instance != nullptr)
    //    {
    //        SINGLETON(coid::pool<ref_counter>).release_item(instance);
    //    }
    //}

    /// @brief Increase strong counter
    void increase_strong_counter()
    {
        _strong_counter.fetch_add(1, std::memory_order_relaxed);
    }

    /// @brief Increases strong counter if possible (counter is not zero)
    /// @return true if counter was successfuly increased
    bool try_increase_strong_counter()
    {
        uint32 current_value = _strong_counter.load(std::memory_order_relaxed);
        while (true)
        {
            if (current_value == 0)
            {
                return false;
            }

            if (_strong_counter.compare_exchange_weak(current_value, current_value + 1, std::memory_order_relaxed))
            {
                return true;
            }
        }

        return false;
    }

    /// @brief Decreses strong counter
    /// @return true if counter decreased to zero
    bool decrease_strong_counter()
    {
        return _strong_counter.fetch_sub(1, std::memory_order_relaxed) == 1;
    }

    /// @brief Get current counter value
    /// @return counter value
    uint32 get_strong_counter_value() const
    {
        return _strong_counter.load(std::memory_order_relaxed);
    }

    /// @brief Increases weak counter
    void increase_weak_counter()
    {
        _weak_counter.fetch_add(1, std::memory_order_relaxed);
    }

    /// @brief Decreses weak counter
    /// @return true if counter decreased to zero
    bool decrease_weak_counter()
    {
        return _weak_counter.fetch_sub(1, std::memory_order_relaxed) == 1;
    }

    /// @brief Get current counter value
    /// @return counter value
    uint32 get_weak_counter_value() const
    {
        return _weak_counter.load(std::memory_order_relaxed);
    }

protected: // methods only
    ref_counter(const ref_counter&) = delete;
    ref_counter& operator=(const ref_counter&) = delete;
protected: // members only
    std::atomic<uint32> _strong_counter = 0;
    std::atomic<uint32> _weak_counter = 0;
};


}; // end of namespace coid