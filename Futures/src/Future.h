#pragma once

#include <optional>

#include "SimpleAwaitable.h"

template<class T>
struct CoreBase {
    virtual ~CoreBase() {}
    virtual T get() = 0;
};

// This core wraps an arbitrary Awaitable into a future without a promise involved
template<class T, class AwaitableT>
struct AwaitableCore : CoreBase<T> {
    AwaitableCore(AwaitableT&& awaitable) : awaitable_(std::move(awaitable)) {}
    virtual T get() {
        return sync_await(std::move(awaitable_));
    }

    AwaitableT awaitable_;
};

template<class T>
struct ValueCore : CoreBase<T> {
    T get() {
        std::lock_guard<std::mutex> lg{mtx_};
        if(!value_) {
            throw std::logic_error("Value not set on promise");
        }
        return *std::move(value_);
    }

    void set_value(T value) {
        std::lock_guard<std::mutex> lg{mtx_};
        value_ = std::move(value);
    }

    std::mutex mtx_;
    std::optional<T> value_;
};

template<class T>
class Future;

template<class T>
class Promise {
public:
    Promise() : core_{std::make_shared<ValueCore<T>>()} {
    }

    void set_value(T value) {
        core_->set_value(std::move(value));
    }

    Future<T> get_future() {
        return Future<T>{std::shared_ptr<CoreBase<T>>{core_}};
    }

private:
    std::shared_ptr<ValueCore<T>> core_;
};

template<class T>
class Future {
public:
    T get() {
        if(value_) {
            return *std::move(value_);
        }
        if(core_) {
            return core_->get();
        }
        throw std::logic_error("Incomplete future");
    }

private:
    // Construct a future from a core
    Future(std::shared_ptr<CoreBase<T>> core) : core_(std::move(core)) {
    }

    // Construct a ready future from a value
    Future(T value) : value_{std::move(value)} {}

    template<class FriendT> friend 
    Future<FriendT> make_future(FriendT);
    template<class FriendT, class AwaitableT> friend
    Future<FriendT> make_awaitable_future(AwaitableT);
    friend class Promise<T>;

    std::optional<T> value_;
    std::shared_ptr<CoreBase<T>> core_;
};

template<class T>
Future<T> make_future(T value) {
    return Future(std::move(value));
}

template<class T, class AwaitableT>
Future<T> make_awaitable_future(AwaitableT awaitable) {
    auto core = std::make_shared<AwaitableCore<T, AwaitableT>>(std::move(awaitable));
    return Future<T>(std::move(core));
}

