#include <stdio.h>
#include <windows.h>

#include <vector>

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

class KeyProtection
{
public:
    KeyProtection() : owned_(false), keycode_(0), shutdown_(false), thread_(INVALID_HANDLE_VALUE) {}

    int value() { return pv_.value(); }

    void setKeycode(int keycode) { keycode_ = keycode; }
    int keycode() { return keycode_; }

    void setOwned(bool owned)
    {
        if (owned_ != owned) {
            owned_ = owned;

            if (owned_) {
                printf("[Main][%c] Claiming\n", keycode_);
                pv_.acquire();
                printf("[Main][%c] Claimed\n", keycode_);
            } else {
                printf("[Main][%c] Releasing\n", keycode_);
                pv_.release();
                printf("[Main][%c] Released\n", keycode_);
            }
        }
    }

    void start()
    {
        DWORD ignored = 0;
        thread_ = CreateThread(NULL, 0, KeyProtection::threadFunc, this, 0, &ignored);
    }

    void stop()
    {
        setOwned(false);

        shutdown_ = true;
        WaitForSingleObject(thread_, INFINITE);
        CloseHandle(thread_);
    }

    static DWORD WINAPI threadFunc(void * param)
    {
        KeyProtection * kp = static_cast<KeyProtection *>(param);

        printf("[Poke][%c] Created.\n", kp->keycode());

        for (;;) {
            int * v = kp->pv_.acquire();
            if (kp->shutdown_) {
                kp->pv_.release();
                break;
            }

            int newValue = *v + 1;
            printf("[Poke][%c] Changing protected value: %d -> %d\n", kp->keycode(), *v, newValue);
            *v = newValue;
            kp->pv_.release();

            Sleep(50);
        }

        printf("[Poke][%c] Shutdown.\n", kp->keycode());
        return 0;
    }

private:
    ProtectedInteger pv_;
    bool owned_;
    int keycode_;
    bool shutdown_;
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
    for (auto it = keyProtections.begin(); it != keyProtections.end(); ++it) {
        it->stop();
    }

    for (auto it = keyProtections.begin(); it != keyProtections.end(); ++it) {
        printf("[Main][%c] Final value: %d\n", it->keycode(), it->value());
    }

    printf("[Main] Shutdown complete.\n");
    return 0;
}
