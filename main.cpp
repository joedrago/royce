#include <stdio.h>
#include <windows.h>

#include <vector>

static bool shutdown_ = false;

// ---------------------------------------------------------------------------------

class Semaphore
{
public:
    Semaphore(int initialCount) { handle_ = CreateSemaphore(nullptr, initialCount, initialCount, nullptr); }

    ~Semaphore() { CloseHandle(handle_); }

    void acquire() { WaitForSingleObject(handle_, INFINITE); }

    void release() { ReleaseSemaphore(handle_, 1, nullptr); }

private:
    HANDLE handle_;
};

// ---------------------------------------------------------------------------------

class ProtectedInteger
{
public:
    ProtectedInteger() : semaphore_(1), value_(0) {}

    int * acquire()
    {
        semaphore_.acquire();
        return &value_;
    }

    void release() { semaphore_.release(); }

    const int value() { return value_; }

private:
    Semaphore semaphore_;
    int value_;
};

// ---------------------------------------------------------------------------------

DWORD WINAPI pokeProc(void * param)
{
    ProtectedInteger * p = static_cast<ProtectedInteger *>(param);

    printf("[Poke][%p] Created.\n", param);

    for (;;) {
        int * v = p->acquire();
        if (shutdown_) {
            p->release();
            break;
        }

        int newValue = *v + 1;
        printf("[Poke][%p] Changing protected value: %d -> %d\n", param, *v, newValue);
        *v = newValue;
        p->release();

        Sleep(50);
    }

    printf("[Poke][%p] Shutdown.\n", param);
    return 0;
}

// ---------------------------------------------------------------------------------

class KeyProtection
{
public:
    KeyProtection() : owned_(false), keycode_(0), thread_(INVALID_HANDLE_VALUE) {}

    int value() { return pv_.value(); }

    void setKeycode(int keycode) { keycode_ = keycode; }
    int keycode() { return keycode_; }

    void setOwned(bool owned)
    {
        if (owned_ != owned) {
            owned_ = owned;

            if (owned_) {
                printf("[Main][%p] Claiming : %c\n", &pv_, keycode_);
                pv_.acquire();
                printf("[Main][%p] Claimed  : %c\n", &pv_, keycode_);
            } else {
                printf("[Main][%p] Releasing: %c\n", &pv_, keycode_);
                pv_.release();
                printf("[Main][%p] Released : %c\n", &pv_, keycode_);
            }
        }
    }

    void start()
    {
        DWORD ignored = 0;
        thread_ = CreateThread(NULL, 0, pokeProc, &pv_, 0, &ignored);
    }

    void stop()
    {
        setOwned(false);

        WaitForSingleObject(thread_, INFINITE);
        CloseHandle(thread_);
    }

private:
    ProtectedInteger pv_;
    bool owned_;
    int keycode_;
    HANDLE thread_;
};

// ---------------------------------------------------------------------------------

int main(int argc, char * argv[])
{
    static const size_t keyCount = 3;
    std::vector<KeyProtection> keyProtections;
    keyProtections.resize(keyCount);
    keyProtections[0].setKeycode('1');
    keyProtections[1].setKeycode('2');
    keyProtections[2].setKeycode('3');

    printf("[Main] Taking ownership of all keys...\n");
    for (auto it = keyProtections.begin(); it != keyProtections.end(); ++it) {
        it->setOwned(true);
    }

    // Spawn all worker threads
    printf("[Main] Spawning worker threads...\n");
    for (auto it = keyProtections.begin(); it != keyProtections.end(); ++it) {
        it->start();
    }

    printf("[Main] Hold any of the 123 keys, or press Space to cleanup.\n");
    while (!GetAsyncKeyState(VK_SPACE)) {
        for (auto it = keyProtections.begin(); it != keyProtections.end(); ++it) {
            it->setOwned(GetAsyncKeyState(it->keycode()) == 0);
        }

        Sleep(50);
    }

    printf("[Main] Shutting down...\n");
    shutdown_ = true;
    for (auto it = keyProtections.begin(); it != keyProtections.end(); ++it) {
        it->stop();
    }

    for (auto it = keyProtections.begin(); it != keyProtections.end(); ++it) {
        printf("[Main][%p] Final value: %d\n", &(*it), it->value());
    }

    printf("[Main] Shutdown complete.\n");
    return 0;
}
