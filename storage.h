#pragma once

#include <exception>
#include <string>
#include <type_traits>
#include "function.h"


struct bad_function_call : std::exception {
    [[nodiscard]] char const* what() const noexcept override {
        return "bad function call";
    }
};

constexpr static size_t INPLACE_BUFFER_SIZE = sizeof(void*);
constexpr static size_t INPLACE_BUFFER_ALIGNMENT = alignof(void*);

using inplace_buffer
= std::aligned_storage<
        INPLACE_BUFFER_SIZE,
        INPLACE_BUFFER_ALIGNMENT
>::type;

template <typename T>
constexpr static bool fits_small_storage =
        sizeof(T) <= INPLACE_BUFFER_SIZE
        && INPLACE_BUFFER_ALIGNMENT % alignof(T) == 0
        && std::is_nothrow_move_constructible<T>::value;

template <typename R, typename... Args>
struct type_descriptor;

template <typename T, typename = void>
struct function_traits;

template <typename R, typename... Args>
struct storage {
    template <typename Friend>
    friend
    struct function;

    storage() noexcept;

    storage(storage const& other);

    storage(storage&& other) noexcept;

    storage& operator=(storage const& other);

    storage& operator=(storage&& other) noexcept;

    ~storage();

    template <typename T>
    explicit storage(T&& val);

    R invoke(Args&&... args);
    R invoke(Args&&... args) const;

    template <typename T>
    T* get_static() noexcept;

    template <typename T>
    T const* get_static() const noexcept;

    template <typename T>
    void set_static(T&& obj) noexcept;

    void set_dynamic(void const* value) noexcept;

    template <typename T>
    T const* get_dynamic() const noexcept;

    template <typename T>
    T* get_dynamic() noexcept;

    template <typename T>
    bool check_type() const noexcept;

    template <typename T>
    bool check_type() noexcept;

    void swap(storage &rhs) noexcept;

    inplace_buffer buf;
    type_descriptor<R, Args...> const* desc;
};

template <typename R, typename... Args>
type_descriptor<R, Args...> const* empty_type_descriptor() {
    using storage_t = storage<R, Args...>;

    constexpr static type_descriptor<R, Args...> impl =
            {
                    [](storage_t const* src, storage_t* dest) {
                        dest->desc = src->desc;
                    },
                    [](storage_t* src, storage_t* dest) {
                        dest->desc = src->desc;
                    },
                    [](storage_t* src, Args&&...) -> R {
                        throw bad_function_call();
                    },
                    [](storage_t*) {}
            };

    return &impl;
}

template <typename R, typename... Args>
storage<R, Args...>::storage() noexcept : buf(), desc(empty_type_descriptor<R, Args...>()) {}

template <typename R, typename... Args>
storage<R, Args...>::storage(storage const& other) : storage() {
    other.desc->copy(&other, this);
}

template <typename R, typename... Args>
storage<R, Args...>::storage(storage&& other) noexcept : storage() {
    other.desc->move(&other, this);
}

template <typename R, typename... Args>
storage<R, Args...>& storage<R, Args...>::operator=(storage const& other) {
    if (this != &other) {
        storage(other).swap(*this);
    }
    return *this;
}

template <typename R, typename... Args>
storage<R, Args...>& storage<R, Args...>::operator=(storage&& other) noexcept {
    if (this != &other) {
        storage(std::move(other)).swap(*this);
    }
    return *this;
}

template <typename R, typename... Args>
storage<R, Args...>::~storage() {
    desc->destroy(this);
}

template <typename R, typename... Args>
template <typename T>
storage<R, Args...>::storage(T&& val) {
    function_traits<T>::template initialize_storage<R, Args...>(*this, std::forward<T>(val));
    desc = function_traits<T>::template get_type_descriptor<R, Args...>();
}

template <typename R, typename... Args>
R storage<R, Args...>::invoke(Args&& ...args) {
    return ((*desc).invoke(this, std::forward<Args>(args)...));
}
template <typename R, typename... Args>
R storage<R, Args...>::invoke(Args&& ...args) const {
    return ((*desc).invoke(this, std::forward<Args>(args)...));
}

template <typename R, typename... Args>
template <typename T>
T* storage<R, Args...>::get_static() noexcept {
    return reinterpret_cast<T*>(&buf);
}

template <typename R, typename... Args>
template <typename T>
T const* storage<R, Args...>::get_static() const noexcept {
    return reinterpret_cast<T const*>(&buf);
}

template <typename R, typename... Args>
template <typename T>
void storage<R, Args...>::set_static(T&& obj) noexcept {
    new(&buf) T(std::forward<T>(obj));
}

template <typename R, typename... Args>
void storage<R, Args...>::set_dynamic(void const* value) noexcept {
    reinterpret_cast<void*&>(buf) = const_cast<void*>(value);
}

template <typename R, typename... Args>
template <typename T>
T const* storage<R, Args...>::get_dynamic() const noexcept {
    return *reinterpret_cast<T* const*>(&buf);
}

template <typename R, typename... Args>
template <typename T>
T* storage<R, Args...>::get_dynamic() noexcept {
    return *reinterpret_cast<T**>(&buf);
}

template <typename R, typename... Args>
template <typename T>
bool storage<R, Args...>::check_type() const noexcept {
    return function_traits<T>::template get_type_descriptor<R, Args...>() == desc;
}

template <typename R, typename... Args>
template <typename T>
bool storage<R, Args...>::check_type() noexcept {
    return function_traits<T>::template get_type_descriptor<R, Args...>() == desc;
}

template <typename R, typename... Args>
void storage<R, Args...>::swap(storage& rhs) noexcept {
    using std::swap;
    swap(buf, rhs.buf);
    swap(desc, rhs.desc);
}

template <typename R, typename... Args>
struct type_descriptor {
    using storage_t = storage<R, Args...>;

    void (* copy)(storage_t const* src, storage_t* dest);

    void (* move)(storage_t* src, storage_t* dest);

    R (* invoke)(storage_t* src, Args&&... args);

    void (* destroy)(storage_t* src);
};

template <typename T>
struct function_traits<T, std::enable_if_t<fits_small_storage<T>>> {
    template <typename R, typename... Args>
    static void initialize_storage(storage<R, Args...>& src, T&& obj) {
        src.set_static(std::move(obj));
    }

    template <typename R, typename... Args>
    static T const* get_target(storage<R, Args...> const* src) {
        return src->template get_static<T>();
    }

    template <typename R, typename... Args>
    static T* get_target(storage<R, Args...>* src) {
        return src->template get_static<T>();
    }

    template <typename R, typename... Args>
    static type_descriptor<R, Args...> const* get_type_descriptor() noexcept {
        using storage_t = storage<R, Args...>;

        constexpr static type_descriptor<R, Args...> impl =
                {
                        // Copy
                        [](storage_t const* src, storage_t* dest) {
                            new(&dest->buf) T(*(src->template get_static<T>()));
                            dest->desc = src->desc;
                        },
                        // move
                        [](storage_t* src, storage_t* dest) {
                            new(&dest->buf) T(std::move(*(src->template get_static<T>())));
                            dest->desc = src->desc;
                        },
                        // invoke
                        [](storage_t* src, Args&&... args) -> R {
                            return (*(src->template get_static<T>()))(std::forward<Args>(args)...);
                        },
                        // destroy
                        [](storage_t* src) {
                            src->template get_static<T>()->~T();
                        }
                };

        return &impl;
    }
};

template <typename T>
struct function_traits<T, std::enable_if_t<!fits_small_storage<T>>> {
    template <typename R, typename... Args>
    static void initialize_storage(storage<R, Args...>& src, T&& obj) {
        src.set_dynamic(new T(std::move(obj)));
    }

    template <typename R, typename... Args>
    static T const* get_target(storage<R, Args...> const* src) {
        return src->template get_dynamic<T>();
    }

    template <typename R, typename... Args>
    static T* get_target(storage<R, Args...> *src) {
        return src->template get_dynamic<T>();
    }

    template <typename R, typename... Args>
    static type_descriptor<R, Args...> const* get_type_descriptor() noexcept {
        using storage_t = storage<R, Args...>;

        constexpr static type_descriptor<R, Args...> impl =
                {
                        // Copy
                        [](storage_t const* src, storage_t* dest) {
                            dest->set_dynamic(new T(*(src->template get_dynamic<T>())));
                            dest->desc = src->desc;
                        },
                        // move
                        [](storage_t* src, storage_t* dest) {
                            new(&dest->buf) T*(*(src-> template get_static<T*>()));
                            src->set_dynamic(nullptr);
                            dest->desc = src->desc;
                        },
                        // invoke
                        [](storage_t* src, Args&&... args) -> R {
                            return (*(src->template get_dynamic<T>()))(std::forward<Args>(args)...);
                        },
                        // destroy
                        [](storage_t* src) {
                            delete (src->template get_dynamic<T>());
                        }
                };

        return &impl;
    }
};
