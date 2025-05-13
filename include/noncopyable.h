#pragma once // Prevent header file from being included multiple times

/**
 * After inheriting from noncopyable, derived class objects can be constructed and destructed normally, but cannot be copy constructed or assignment constructed
 **/
class noncopyable
{
public:
    noncopyable(const noncopyable &) = delete;
    noncopyable &operator=(const noncopyable &) = delete;
    // void operator=(const noncopyable &) = delete;    // muduo changes return value to void, which is actually fine
protected:
    noncopyable() = default;
    ~noncopyable() = default;
};