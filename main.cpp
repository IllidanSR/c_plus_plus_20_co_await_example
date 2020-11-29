#include <coroutine>
#include <memory>
#include <future>
#include <iostream>


template<typename T>
struct coreturn {
    struct promise;
    friend struct promise;
    using handle_type = std::coroutine_handle<promise>;
    coreturn(const coreturn &) = delete;
    coreturn(coreturn &&s)
            : coro(s.coro) {
        std::cout << "Coreturn wrapper moved" << std::endl;
        s.coro = nullptr;
    }
    ~coreturn() {
        std::cout << "Coreturn wrapper gone" << std::endl;
        if ( coro ) coro.destroy();
    }
    coreturn &operator = (const coreturn &) = delete;
    coreturn &operator = (coreturn &&s) {
        coro = s.coro;
        s.coro = nullptr;
        return *this;
    }
    struct promise {
        friend struct coreturn;
        promise() {
            std::cout << "Promise created" << std::endl;
        }
        ~promise() {
            std::cout << "Promise died" << std::endl;
        }
        auto return_value(T v) {
            value = v;
            return std::suspend_never{};
        }
        auto final_suspend() {
            return std::suspend_always{};
        }
        void unhandled_exception() {
            std::exit(1);
        }
    private:
        T value;
    };
    bool await_ready() {
        const auto ready = coro.done();
        return coro.done();
    }

    void await_suspend(std::coroutine_handle<> awaiting) {
        coro.resume();
        awaiting.resume();
    }

    auto await_resume() {
        const auto r = coro.promise().value;
        return r;
    }
protected:
    T get() {
        if ( not coro.done() ) coro.resume();
        return coro.promise().value;
    }
    coreturn(handle_type h)
            : coro(h) {
    }
    handle_type coro;
};

template<typename T>
struct sync : public coreturn<T> {
    using coreturn<T>::coreturn;
    using handle_type = typename coreturn<T>::handle_type;
    T get() {
        return coreturn<T>::get();
    }
    bool await_ready() {
        const auto ready = coro.done();
        return coro.done();
    }

    void await_suspend(std::coroutine_handle<> awaiting) {
        coro.resume();
        awaiting.resume();
    }

    auto await_resume() {
        const auto r = coro.promise().value;
        return r;
    }
    handle_type coro;
    struct promise_type : public coreturn<T>::promise {
        auto get_return_object() {
            return sync<T>{handle_type::from_promise(*this)};
        }
        auto initial_suspend() {
            return std::suspend_never{};
        }
    };
};


template<typename T>
struct lazy : public coreturn<T> {
    using coreturn<T>::coreturn;
    using handle_type = typename coreturn<T>::handle_type;;
    T get() {
        if ( not this->coro.done() ) this->coro.resume();
        return coreturn<T>::get();
    }


    struct promise_type : public coreturn<T>::promise {
        auto get_return_object() {
            return lazy<T>{handle_type::from_promise(*this)};
        }
        auto initial_suspend() {
            return std::suspend_always{};
        }
    };
};


void function() {
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << "FUNCTION" << std::endl;
}
template <typename ReturnType>
ReturnType answer() {
    std::cout << "Thinking deep thoghts..." << std::endl;
    function();
    co_return 2;
}

sync<int> await_answer() {
    std::cout << "Started await_answer" << std::endl;
    auto a = answer<lazy<int>>();
    std::cout << "Got a coroutine, let's get a value" << std::endl;
    auto v = co_await a;
    std::cout << "And the coroutine value is: " << std::endl;
    /*v = co_await a;
    std::cout << "And the coroutine value is still: " << v << std::endl;*/
    co_return v;
}

int main() {
    auto value1 = await_answer().get();
    auto value2 = await_answer().get();
    auto value3 = await_answer().get();
    auto value4 = await_answer().get();
    auto value5 = std::async(std::launch::async, [](){
        std::cout << "RUN" << std::endl;
        auto v = await_answer().get();
        std::cout << "VALUE 5 : " << v << std::endl;
        return v;});
    auto value6 = await_answer().get();
    auto value7 = await_answer().get();
    auto value8 = await_answer().get();
    auto value9 = await_answer().get();
    auto value10 = await_answer().get();
    auto value11 = await_answer().get();
    auto value12 = await_answer().get();
    std::cout << "VALUE : " << value1 << std::endl;
    std::cout << "VALUE5 : " << std::endl;
    value5.get();
    std::cout << "VALUE : " << value12 << std::endl;
    return 0;
}