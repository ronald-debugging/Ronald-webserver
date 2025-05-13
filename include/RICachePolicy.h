#pragma once

namespace RonaldCache
{

template <typename Key, typename Value>
class RICachePolicy
{
public:
    virtual ~RICachePolicy() {};

    // Add cache interface
    virtual void put(Key key, Value value) = 0;

    // key is the input parameter, accessed value is returned as an output parameter | returns true if access is successful
    virtual bool get(Key key, Value& value) = 0;
    // If key is found in cache, return value directly
    virtual Value get(Key key) = 0;

};

} // namespace RonaldCache