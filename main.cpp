#include <coroutine>
#include <memory>
#include <list>
#include <thread>
#include <assert.h>
#include <iostream>

template<typename T>
struct coroutine_ {
    struct promise;
    friend struct promise;
    using handle_type = std::coroutine_handle<promise>;
    coroutine_(const coroutine_ &) = delete;
    coroutine_(coroutine_ &&s)
            : coro(s.coro) {
        s.coro = nullptr;
    }
    ~coroutine_() {
        if ( coro ) coro.destroy();
    }
    coroutine_ &operator = (const coroutine_ &) = delete;
    coroutine_ &operator = (coroutine_ &&s) {
        coro = s.coro;
        s.coro = nullptr;
        return *this;
    }
    struct promise {
        friend struct coroutine_;
        promise() {
            std::cout << "Promise created" << std::endl;
        }
        ~promise() {
            std::cout << "Promise died" << std::endl;
        }
        auto return_value(T v) {
            value = v;
            return std::suspend_always{};
        }
        auto final_suspend() {
            std::cout << "Finished the coro" << std::endl;
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
        std::cout << "Await " << (ready ? "is ready" : "isn't ready") << std::endl;
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
    coroutine_(handle_type h)
            : coro(h) {
    }
    handle_type coro;
};

template<typename T>
struct my_coroutine : public coroutine_<T> {
    using coroutine_<T>::coroutine_;
    using handle_type = typename coroutine_<T>::handle_type;
    T get() {
        if ( not this->coro.done() ) this->coro.resume();
        return coroutine_<T>::get();
    }

    bool resume() {
        if(not this->coro.done()) {
            this->coro.resume();
            return not this->coro.done();
        }
    }

    struct promise_type : public coroutine_<T>::promise {
        auto get_return_object() {
            return my_coroutine<T>{handle_type::from_promise(*this)};
        }
        auto initial_suspend() {
            return std::suspend_always{};
        }
    };
};

using coro_t = std::coroutine_handle<>;
class evt_awaiter_t {
    struct awaiter;

    // we want to pop front and push back WITHOUT iterator invalidation
    std::list<awaiter> lst_;
    bool set_;

    struct awaiter {
        evt_awaiter_t &event_;
        coro_t coro_ = nullptr;
        awaiter(evt_awaiter_t &event) noexcept : event_(event) {}

        bool await_ready() const noexcept { return event_.is_set(); }

        void await_suspend(coro_t coro) noexcept {
            coro_ = coro;
            event_.push_awaiter(*this);
        }

        void await_resume() noexcept { event_.reset(); }
    };

public:
    evt_awaiter_t(bool set = false) : set_{set} {}
    evt_awaiter_t(const evt_awaiter_t &) = delete;
    evt_awaiter_t &operator=(const evt_awaiter_t &) = delete;
    evt_awaiter_t(evt_awaiter_t &&) = delete;
    evt_awaiter_t &operator=(evt_awaiter_t &&) = delete;

public:
    bool is_set() const noexcept { return set_; }
    void push_awaiter(awaiter a) { lst_.push_back(a); }

    awaiter operator co_await() noexcept { return awaiter{*this}; }

    void set() noexcept {
        set_ = true;
#if INEFF
        auto ntoresume = lst_.size();
    while (ntoresume > 0) {
      lst_.front().coro_.resume();
      lst_.pop_front();
      ntoresume--;
    }
#else
        std::list<awaiter> toresume;
        toresume.splice(toresume.begin(), lst_);
        for (auto s: toresume)
            s.coro_.resume();
#endif
    }

    void reset() noexcept { set_ = false; }
};
class SubClassTest {
public:
    double x;
    double y;
    void foo() {

    }
};

my_coroutine<int> answer() {
    std::cout << "Thinking deep thoghts..." << std::endl;
    co_return 42;
}

template<typename T>
my_coroutine<int> answer_value(T value) {
    int local_value = value;
    for (int i = 0; i < value; i ++) {
        local_value+=i;
    }
    co_return local_value;
}

my_coroutine<SubClassTest> answer_value(SubClassTest value) {
    value.x = 1;
    value.y = 2;
    co_return value;
}

my_coroutine<int> await_answer() {
    std::cout << "Started await_answer" << std::endl;
    std::cout << "Got a coroutine, let's get a value" << std::endl;
    SubClassTest subClassTest;
    if (true) {
        auto v = co_await answer();
        std::cout << "And the coroutine value is: " << v << std::endl;
        auto v2 = co_await answer_value<int>(10);
        co_await std::suspend_always();
        std::cout << "And the coroutine value is still: " << v2 << std::endl;
        auto v3 = co_await answer_value(subClassTest);
        std::cout << "And the coroutine value is still: " << v3.x << " " << v3.y << std::endl;
    }
    co_return 0;
}

/*int main() {
    auto t = await_answer();
    t.resume();
    t.resume();
    return 0;
}*/
struct resumable {
    struct promise_type {
        using coro_handle = std::coroutine_handle<promise_type>;
        auto get_return_object() { return coro_handle::from_promise(*this); }
        auto initial_suspend() { return std::suspend_always(); }
        auto final_suspend() { return std::suspend_always(); }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    using coro_handle = std::coroutine_handle<promise_type>;
    resumable(coro_handle handle) : handle_(handle) { assert(handle); }
    resumable(resumable &) = delete;
    resumable(resumable &&rhs) : handle_(rhs.handle_) { rhs.handle_ = nullptr; }
    bool resume() {
        if (!handle_.done())
            handle_.resume();
        return !handle_.done();
    }
    ~resumable() {
        if (handle_)
            handle_.destroy();
    }

private:
    coro_handle handle_;
};

struct resumable_no_wait {
    struct promise_type {
        using coro_handle = std::coroutine_handle<promise_type>;
        auto get_return_object() { return coro_handle::from_promise(*this); }
        auto initial_suspend() { return std::suspend_never(); }
        auto final_suspend() { return std::suspend_always(); }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    using coro_handle = std::coroutine_handle<promise_type>;
    resumable_no_wait(coro_handle handle) : handle_(handle) { assert(handle); }
    resumable_no_wait(resumable_no_wait &) = delete;
    resumable_no_wait(resumable_no_wait &&rhs) : handle_(rhs.handle_) {
        rhs.handle_ = nullptr;
    }
    bool resume() {
        if (!handle_.done())
            handle_.resume();
        return !handle_.done();
    }
    ~resumable_no_wait() {
        if (handle_)
            handle_.destroy();
    }

private:
    coro_handle handle_;
};

struct resumable_no_own {
    struct promise_type {
        using coro_handle = std::coroutine_handle<promise_type>;
        auto get_return_object() { return coro_handle::from_promise(*this); }
        auto initial_suspend() { return std::suspend_never(); }

        // this one is critical: no suspend on final suspend
        // effectively means "destroy your frame"
        auto final_suspend() { return std::suspend_never(); }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    using coro_handle = std::coroutine_handle<promise_type>;
    resumable_no_own(coro_handle handle) {}
    resumable_no_own(resumable_no_own &) {}
    resumable_no_own(resumable_no_own &&rhs) {}
};

struct suspend_tunable {
    bool tune_;
    suspend_tunable(bool tune = true) : tune_(tune) {}
    bool await_ready() const noexcept { return tune_; }
    void await_suspend(std::coroutine_handle<>) const noexcept {}
    void await_resume() const noexcept {}
};

struct resumable_cancelable{
    struct promise_type {
        bool is_cancelled = false;
        using coro_handle = std::coroutine_handle<promise_type>;
        auto get_return_object() { return coro_handle::from_promise(*this); }
        auto initial_suspend() { return std::suspend_always(); }
        auto final_suspend() { return std::suspend_always(); }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }

        auto await_transform(std::suspend_always){
            if(is_cancelled)
                return suspend_tunable{true};
            return suspend_tunable{false};
        }
    };

    using coro_handle = std::coroutine_handle<promise_type>;
    resumable_cancelable(coro_handle handle) : handle_(handle) { assert(handle); }
    resumable_cancelable(resumable_cancelable &) = delete;
    resumable_cancelable(resumable_cancelable &&rhs) : handle_(rhs.handle_) { rhs.handle_ = nullptr; }
    void cancel() {
        if (handle_.done())
            return;
        handle_.promise().is_cancelled = true;
        handle_.resume();
    }
    bool resume() {
        if (!handle_.done())
            handle_.resume();
        return !handle_.done();
    }
    ~resumable_cancelable() {
        if (handle_)
            handle_.destroy();
    }

private:
    coro_handle handle_;
};

// weakly incapsulated resumable object
// have ability to open handle
struct resumable_noinc {
    struct promise_type {
        using coro_handle = std::coroutine_handle<promise_type>;
        auto get_return_object() { return coro_handle::from_promise(*this); }
        auto initial_suspend() { return std::suspend_always(); }
        auto final_suspend() { return std::suspend_always(); }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    using coro_handle = std::coroutine_handle<promise_type>;
    resumable_noinc(coro_handle handle) : handle_(handle) { assert(handle); }
    resumable_noinc(resumable_noinc &) = delete;
    resumable_noinc(resumable_noinc &&rhs) : handle_(rhs.handle_) { rhs.handle_ = nullptr; }
    bool resume() {
        if (!handle_.done())
            handle_.resume();
        return !handle_.done();
    }
    ~resumable_noinc() {
        if (handle_)
            handle_.destroy();
    }
    coro_handle handle() {
        coro_handle h = handle_;
        handle_ = nullptr;
        return h;
    }

private:
    coro_handle handle_;
};



int g_value;
evt_awaiter_t g_evt;

void producer() {
    std::cout << "Producer started" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    g_value = 42;
    std::cout << "Value ready" << std::endl;
    g_evt.set();
}

resumable_no_own consumer1() {
    std::cout << "Consumer1 started" << std::endl;
    co_await g_evt;
    std::cout << "Consumer1 resumed" << std::endl;
}

resumable_no_own consumer2() {
    std::cout << "Consumer2 started" << std::endl;
    co_await g_evt;
    std::cout << "Consumer2 resumed" << std::endl;
}

resumable_no_own consumer3() {
    std::cout << "Consumer3 started" << std::endl;
    co_await g_evt;
    std::cout << "Consumer3 resumed" << std::endl;
    co_await g_evt;
    std::cout << "Consumer3 resumed again" << std::endl;
}

int main() {
    consumer1();
    consumer2();
    consumer3();
    producer();
    consumer1();
    producer();
}
