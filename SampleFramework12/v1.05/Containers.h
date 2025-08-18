//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  https://therealmjp.github.io/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include "PCH.h"
#include "SF12_Assert.h"

namespace SampleFramework12
{

// Basic heap-based array with arbitrary size
template<typename T> class Array
{

protected:

    uint64_t size = 0;
    T* data = nullptr;

public:

    Array()
    {
    }

    Array(const Array& other)
    {
        Init(other.size);
        for(uint64_t i = 0; i < size; ++i)
            data[i] = other.data[i];
    }

    Array(Array&& other)
    {
        size = other.size;
        data = other.data;

        other.size = 0;
        other.data = nullptr;
    }

    explicit Array(uint64_t numElements)
    {
        Init(numElements);
    }

    explicit Array(uint64_t numElements, const T& fillValue)
    {
        Init(numElements, fillValue);
    }

    Array& operator=(const Array& other)
    {
        if(&other == this)
            return *this;

        Shutdown();

        Init(other.size);
        for(uint64_t i = 0; i < size; ++i)
            data[i] = other.data[i];

        return *this;
    }

    Array& operator=(Array&& other)
    {
        if(&other == this)
            return *this;

        Shutdown();

        size = other.size;
        data = other.data;

        other.size = 0;
        other.data = nullptr;

        return *this;
    }

    ~Array()
    {
        Shutdown();
    }

    void Init(uint64_t numElements)
    {
        Shutdown();

        size = numElements;
        if(size > 0)
            data = new T[size];
    }

    void Init(uint64_t numElements, const T& fillValue)
    {
        Init(numElements);
        Fill(fillValue);
    }

    void Shutdown()
    {
        if(data)
        {
            delete[] data;
            data = nullptr;
        }
        size = 0;
    }

    void Resize(uint64_t numElements)
    {
        if(numElements == size)
            return;

        if(numElements == 0)
        {
            Shutdown();
            return;
        }

        T* newData = new T[numElements];
        for(uint64_t i = 0; i < size; ++i)
            newData[i] = data[i];

        Shutdown();
        data = newData;
        size = numElements;
    }

    uint64_t Size() const
    {
        return size;
    }

    constexpr uint64_t ElementSize() const
    {
        return sizeof(T);
    }

    uint64_t MemorySize() const
    {
        return size * sizeof(T);
    }

    const T& operator[](uint64_t idx) const
    {
        Assert_(idx < size);
        Assert_(data != nullptr);
        return data[idx];
    }

    T& operator[](uint64_t idx)
    {
        Assert_(idx < size);
        Assert_(data != nullptr);
        return data[idx];
    }

    const T* Data() const
    {
        return data;
    }

    T* Data()
    {
        return data;
    }

    void Fill(const T& value)
    {
        for(uint64_t i = 0; i < size; ++i)
            data[i] = value;
    }

    T* begin()
    {
        return data;
    }

    T* end()
    {
        return data + size;
    }

    const T* begin() const
    {
        return data;
    }

    const T* end() const
    {
        return data + size;
    }
};

template<typename T> class List
{

protected:

    T* data = nullptr;
    uint64_t maxCount = 0;
    uint64_t count = 0;

public:

    List()
    {
    }

    List(const List& other)
    {
        Append(other.data, other.count);
    }

    List(List&& other)
    {
        data = other.data;
        maxCount = other.maxCount;
        count = other.count;

        other.data = nullptr;
        other.maxCount = 0;
        other.count = 0;
    }

    explicit List(uint64_t initialMaxCount, uint64_t initialCount = 0)
    {
        Init(initialMaxCount, initialCount);
    }

    List(uint64_t initialMaxCount, uint64_t initialCount, const T& fillValue)
    {
        Init(initialMaxCount, initialCount, fillValue);
    }

    List& operator=(const List& other)
    {
        if(&other == this)
            return *this;

        Shutdown();
        Append(other.data, other.count);

        return *this;
    }

    List& operator=(List&& other)
    {
        if(&other == this)
            return *this;

        Shutdown();

        data = other.data;
        maxCount = other.maxCount;
        count = other.count;

        other.data = nullptr;
        other.maxCount = 0;
        other.count = 0;

        return *this;
    }

    void Init(uint64_t initialMaxCount, uint64_t initialCount = 0)
    {
        const uint64_t reserveAmt = initialMaxCount > initialCount ? initialMaxCount : initialCount;
        Reserve(reserveAmt);
        count = initialCount;
        for(uint64_t i = 0; i < count; ++i)
            new (&data[i]) T;
    }

    void Init(uint64_t initialMaxCount, uint64_t initialCount, const T& fillValue)
    {
        Init(initialMaxCount, initialCount);
        Fill(fillValue);
    }

    void Shutdown()
    {
        for(uint64_t i = 0; i < count; ++i)
            data[i].~T();

        if(data != nullptr)
            std::free(data);
        data = nullptr;
        count = 0;
        maxCount = 0;
    }

    uint64_t Count() const
    {
        return count;
    }

    uint64_t CurrentMaxCount() const
    {
        return maxCount;
    }

    const T& operator[](uint64_t idx) const
    {
        Assert_(idx < count);
        return data[idx];
    }

    T& operator[](uint64_t idx)
    {
        Assert_(idx < count);
        return data[idx];
    }

    const T* Data() const
    {
        return data;
    }

    T* Data()
    {
        return data;
    }

    T& FirstElement()
    {
        Assert_(count > 0);
        return data[0];
    }

    const T& FirstElement() const
    {
        Assert_(count > 0);
        return data[0];
    }

    T& LastElement()
    {
        Assert_(count > 0);
        return data[count - 1];
    }

    const T& LastElement() const
    {
        Assert_(count > 0);
        return data[count - 1];
    }

    void Fill(const T& value)
    {
        for(uint64_t i = 0; i < count; ++i)
            data[i] = value;
    }

    void Reserve(uint64_t newMaxCount)
    {
        if(newMaxCount <= maxCount)
            return;

        if(maxCount == 0)
            maxCount = 16;

        while(newMaxCount > maxCount)
            maxCount *= 2;

        T* newData = reinterpret_cast<T*>(std::malloc(maxCount * sizeof(T)));
        for(uint64_t i = 0; i < count; ++i)
            new (&newData[i]) T(std::move(data[i]));

        if(data != nullptr)
            std::free(data);
        data = newData;
    }

    T& Add()
    {
        Reserve(count + 1);

        const uint64_t idx = count++;
        new (&data[idx]) T;
        return data[idx];
    }

    uint64_t Add(const T& item)
    {
        Reserve(count + 1);

        const uint64_t idx = count++;
        new (&data[idx]) T(item);
        return idx;
    }

    uint64_t Add(T&& item)
    {
        Reserve(count + 1);

        const uint64_t idx = count++;
        new (&data[idx]) T(item);
        return idx;
    }

    void AddMultiple(uint64_t itemCount)
    {
        if(itemCount == 0)
            return;

        Reserve(count + itemCount);

        for(uint64_t i = 0; i < itemCount; ++i)
            new (&data[i + count]) T;

        count += itemCount;
    }

    void AddMultiple(uint64_t itemCount, const T& item)
    {
        if(itemCount == 0)
            return;

        Reserve(count + itemCount);

        for(uint64_t i = 0; i < itemCount; ++i)
            new (&data[i + count]) T(item);

        count += itemCount;
    }

    void Append(const T* items, uint64_t itemCount)
    {
        if(itemCount == 0)
            return;

        Reserve(count + itemCount);

        for(uint64_t i = 0; i < itemCount; ++i)
            new (&data[i + count]) T(items[i]);

        count += itemCount;
    }

    void Insert(const T& item, uint64_t idx)
    {
        Assert_(idx <= count);
        if(idx == count)
        {
            Add(item);
            return;
        }

        Reserve(count + 1);

        new (&data[count]) T(std::move(data[count - 1]));
        for(int64_t i = count - 1; i > int64_t(idx); --i)
            data[i] = std::move(data[i - 1]);

        new (&data[idx]) T(item);
        ++count;
    }

    void Remove(uint64_t idx)
    {
        Assert_(idx < count);

        if(idx == count - 1)
        {
            data[idx].~T();
            --count;
            return;
        }

        for(uint64_t i = idx; i < count - 1; ++i)
            data[idx] = std::move(data[i + 1]);

        --count;
    }

    void RemoveMultiple(uint64_t idx, uint64_t numItems)
    {
        Assert_(idx < count);
        Assert_(idx + numItems <= count);

        for(uint64_t i = 0; i < numItems; ++i)
        {
            if(i + idx + numItems < count)
                data[i + idx] = std::move(data[i + idx + numItems]);
            else
                data[i + idx].~T();
        }

        count -= numItems;
    }

    void RemoveAll()
    {
        for(uint64_t i = 0; i < count; ++i)
            data[i].~T();
        count = 0;
    }

    void Trim(uint64_t newCount)
    {
        Assert_(newCount <= count);
        if(newCount < count)
            RemoveMultiple(newCount, count - newCount);
    }

    T* begin()
    {
        return data;
    }

    T* end()
    {
        return data + count;
    }

    const T* begin() const
    {
        return data;
    }

    const T* end() const
    {
        return data + count;
    }
};

}