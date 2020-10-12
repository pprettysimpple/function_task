#pragma once

#include "storage.h"

template <typename F>
struct function;

template <typename R, typename... Args>
struct function<R(Args...)> {

    template <typename Friend, typename... FriendArgs>
    friend
    struct storage;

    function() noexcept = default;

    function(function const& other) = default;
    function(function&& other) noexcept = default;

    template <typename T>
    function(T val);

    function& operator=(function const& rhs) = default;
    function& operator=(function&& rhs) noexcept = default;

    ~function() = default;

    explicit operator bool() const noexcept;

    R operator()(Args&&... args) const;
    R operator()(Args&&... args);

    template <typename T>
    T* target() noexcept;

    template <typename T>
    T const* target() const noexcept;

private:
    storage<R, Args...> stg;
};

template <typename R, typename... Args>
template <typename T>
function<R(Args...)>::function(T val) : stg(std::move(val)) {}

template <typename R, typename... Args>
function<R(Args...)>::operator bool() const noexcept {
    return stg.desc != empty_type_descriptor<R, Args...>();
}

template <typename R, typename... Args>
R function<R(Args...)>::operator()(Args&&... args) const {
    return stg.invoke(std::forward<Args>(args)...);
}

template <typename R, typename... Args>
R function<R(Args...)>::operator()(Args&&... args) {
    return stg.invoke(std::forward<Args>(args)...);
}

template <typename R, typename... Args>
template <typename T>
T* function<R(Args...)>::target() noexcept {
    if (operator bool() && stg.template check_type<T>()) {
        return function_traits<T>::template get_target<R, Args...>(&stg);
    } else {
        return nullptr;
    }
}

template <typename R, typename... Args>
template <typename T>
T const* function<R(Args...)>::target() const noexcept {
    if (operator bool() && stg.template check_type<T>()) {
        return function_traits<T>::template get_target<R, Args...>(&stg);
    } else {
        return nullptr;
    }
}