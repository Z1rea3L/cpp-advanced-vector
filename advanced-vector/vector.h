#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity)
    {
    }

    RawMemory(const RawMemory&) = delete;

    RawMemory operator=(const RawMemory&) = delete;

    RawMemory(RawMemory&& other) noexcept {
        buffer_ = std::exchange(other.buffer_, nullptr);
        capacity_ = std::exchange(other.capacity_, 0);
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    RawMemory& operator=(RawMemory&& other) noexcept {
        buffer_ = std::exchange(other.buffer_, nullptr);
        capacity_ = std::exchange(other.capacity_, 0);
        return *this;
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

    static void CopyOrMoveData(T* data, size_t count, T* new_place) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data, count, new_place);

        }else{
            std::uninitialized_copy_n(data, count, new_place);
        }
    }

private:

    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buffer) noexcept {
        operator delete(buffer);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};


template <typename T>
class Vector {
public:

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }


    Vector() = default;

    Vector(size_t size)
        :data_(size)
        ,size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size_);
    }

    Vector(const Vector& other)
        :data_(other.size_)
        ,size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        :data_(std::move(other.data_))
    {
        size_ = std::exchange(other.size_, 0);
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);

            }else{
                size_t count = std::min(rhs.size_, size_);
                std::copy_n(rhs.data_.GetAddress(), count, data_.GetAddress());

                if (rhs.size_ < size_) {
                    std::destroy_n(data_ + rhs.size_, size_ - rhs.size_);

                }else{
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }

                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        data_ = std::move(rhs.data_);
        size_ = std::exchange(rhs.size_, 0);
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);

        RawMemory<T>::CopyOrMoveData(data_.GetAddress(), size_, new_data.GetAddress());

        std::destroy_n(data_.GetAddress(), size_);

        data_.Swap(new_data);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        assert(index < size_);
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Resize(size_t new_size) {
        if (new_size <= size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
            size_ = new_size;

        }else{
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
            size_ = new_size;
        }
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    template <typename...Args>
    T& EmplaceBack(Args&&...args) {
        return *Emplace(cend(), std::forward<Args>(args)...);
    }

    void PopBack()  noexcept {
        const size_t cnt = 1;
        std::destroy_n(data_.GetAddress() + size_ - 1, cnt);
        --size_;
    }

    iterator Erase(const_iterator pos){
        iterator it = begin() + (pos - cbegin());
        std::move(it + 1, end(), it);

        const size_t cnt = 1;
        std::destroy_n(data_.GetAddress() + size_ - 1, cnt);

        --size_;

        return it;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {

        if (size_ == Capacity()) {
            return EmplaceWithRealloc(pos, std::forward<Args>(args)...);

        }else{
            return EmplaceWithoutRealloc(pos, std::forward<Args>(args)...);
        }
    }

    template <typename... Args>
    iterator EmplaceWithRealloc(const_iterator pos, Args&&... args){
        size_t new_size = size_ + 1;
        size_t new_capacity = (size_>0 ? size_*2 : 1);
        size_t offset = pos - cbegin();

        RawMemory<T> new_data(new_capacity);
        new (new_data + offset) T(std::forward<Args>(args)...);

        try {
            RawMemory<T>::CopyOrMoveData(data_.GetAddress(),offset, new_data.GetAddress());

        }catch (...){
            std::destroy_n(new_data.GetAddress() + offset, 1);
            throw;
        }

        try {
            RawMemory<T>::CopyOrMoveData(data_.GetAddress() + offset, (cend() - pos), new_data.GetAddress() + offset + 1);

        }catch (...){
            std::destroy_n(new_data.GetAddress(), offset);
            throw;
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
        size_ = new_size;

        return begin() + offset;
    }

    template <typename... Args>
    iterator EmplaceWithoutRealloc(const_iterator pos, Args&&... args){
        size_t new_size = size_ + 1;
        size_t offset = pos - cbegin();

        if (pos == cend()) {
            new (data_ + size_) T(std::forward<Args>(args)...);

        }else{
            T&& temp = T(std::forward<Args>(args)...);
            new (data_ + size_) T(std::move(data_[size_ - 1]));

            iterator first = begin() + offset;
            iterator last = begin() + (size_ - 1);

            std::move_backward(first, last, end());
            data_[offset] = std::move(temp);
        }

        size_ = new_size;
        return begin() + offset;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::forward<T>(value));
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};
