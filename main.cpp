#include <coroutine>
#include <memory>
#include <list>
#include <thread>
#include <map>
#include <assert.h>
#include <iostream>

using coro_t = std::coroutine_handle<>;

class evt_awaiter_t {
    struct awaiter;

    // we want to pop front and push back WITHOUT iterator invalidation
    std::list<awaiter> lst_;
    std::map<int, awaiter> awaiter_map;
    bool set_;
    std::atomic_int index = 0;
    int local_id = 0;
    struct awaiter {
        evt_awaiter_t &event_;
        coro_t coro_ = nullptr;
        awaiter(evt_awaiter_t &event) noexcept : event_(event) {}

        bool await_ready() const noexcept { return event_.is_set(); }

        void await_suspend(coro_t coro) noexcept {
            coro_ = coro;
            event_.add_awaiter(*this);
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
    void set_id(int id) {
        local_id = id;
    }
    int add_awaiter(awaiter a) {
        awaiter_map.insert(std::pair<int, awaiter>(local_id, a));
        return local_id;
    }

    awaiter operator co_await() noexcept { return awaiter{*this}; }
    void set_to_map(int ind) {
        set_ = true;
        auto corout = awaiter_map.find(ind);
        corout->second.coro_.resume();
    }

    void delete_from_map(int idn) {
        auto corout = awaiter_map.find(idn);
        corout->second.coro_.done();
    }
    int size() {
       return awaiter_map.size();
    }

    void reset() noexcept {
        set_ = false;
    }

};

class SubClassTest {
public:
    double x;
    double y;
    void foo() {

    }
};


struct resumable {
    struct promise_type {
        using coro_handle = std::coroutine_handle<promise_type>;
        auto get_return_object() { return coro_handle::from_promise(*this); }
        auto initial_suspend() { return std::suspend_always(); }
        auto final_suspend() { return std::suspend_never(); }
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
int g_value;
evt_awaiter_t g_evt;

void producer(int value) {
    if (value == 99) {
        g_evt.delete_from_map(900);
    } else {
        std::cout << "Producer started" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        g_value = 42;
        std::cout << "Value ready" << std::endl;
        g_evt.set_to_map(value);
    }

}

resumable_no_own consumer1() {
    std::cout << "Consumer1 started" << std::endl;
    g_evt.set_id(700);
    co_await g_evt;
    std::cout << "Consumer1 resumed" << std::endl;
}

resumable_no_own consumer2() {
    std::cout << "Consumer2 started" << std::endl;
    g_evt.set_id(800);
    co_await g_evt;
    std::cout << "Consumer2 resumed" << std::endl;
}

resumable_no_own consumer3(int val) {
    std::cout << "Consumer3 started" << std::endl;
    g_evt.set_id(900);
    co_await g_evt;
    std::cout << "Consumer3 resumed : " << val << std::endl;
    co_await g_evt;
    std::cout << "Consumer3 resumed again : " << val << std::endl;
}

int main() {
    consumer1();
    consumer2();
    consumer3(3);
    while (true) {
        int v;
        std::cin >> v;
        producer(v);
    }
    consumer1();
}
