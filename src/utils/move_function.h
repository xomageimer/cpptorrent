#ifndef CPPTORRENT_MOVE_FUNCTION_H
#define CPPTORRENT_MOVE_FUNCTION_H

#include <memory>
#include <type_traits>

class move_function {
    struct placeholder {
        virtual ~placeholder() = default;
        virtual void call() = 0;
    };

    template<class T>
    struct holder : placeholder {
        holder(T f) : f(std::forward<T>(f)) {}
        T f;
        void call() override { f(); }
    };

    std::unique_ptr<placeholder> f_;

public:
    template<class F,
        class R = std::result_of_t<F &()>,
        std::enable_if_t<!std::is_convertible<std::decay_t<F> *, move_function *>::value, int> = 0>
    move_function(F &&f)
        : f_(new holder<std::decay_t<F>>{std::forward<F>(f)}) {}

    void operator()() const { f_->call(); }
};

#endif // CPPTORRENT_MOVE_FUNCTION_H
