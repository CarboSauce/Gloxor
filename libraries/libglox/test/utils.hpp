#pragma once

struct alloc_tracker
{
    static inline int dtor_counter = 0;
    static inline int copy_ctor_counter = 0;
    static inline int move_ctor_counter = 0;
    static inline int copy_assignment_counter = 0;
    static inline int move_assignment_counter = 0;

    int value;

    alloc_tracker(int value = 0)
        : value(value)
    {
    }
    alloc_tracker(const alloc_tracker& other)
    {
        copy_ctor_counter++;
        value = other.value;
    }
    alloc_tracker(alloc_tracker&& other)
    {
        move_ctor_counter++;
        value = other.value;
    }
    alloc_tracker& operator=(const alloc_tracker& other)
    {
        copy_assignment_counter++;
        value = other.value;
        return *this;
    }
    alloc_tracker& operator=(alloc_tracker&& other)
    {
        move_assignment_counter++;
        value = other.value;
        return *this;
    }
    ~alloc_tracker()
    {
        dtor_counter++;
    }
    static void reset_counters()
    {
        dtor_counter = 0;
        copy_ctor_counter = 0;
        move_ctor_counter = 0;
        copy_assignment_counter = 0;
        move_assignment_counter = 0;
    }
};
