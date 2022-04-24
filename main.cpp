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

class ProtectedValue
{
public:
    ProtectedValue() : semaphore_(1), value_(0) {}

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
    KeyProtection() : keycode_(0), shutdown_(false), thread_(INVALID_HANDLE_VALUE) {}

    void configure(ProtectedValue *pv, int keycode, int valueToSet) {
        pv_ = pv;
        keycode_ = keycode;
        valueToSet_ = valueToSet;
    }
    int keycode() { return keycode_; }

    void start()
    {
        DWORD ignored = 0;
        thread_ = CreateThread(NULL, 0, KeyProtection::workerThreadProc, this, 0, &ignored);
    }

    void stop()
    {
        shutdown_ = true;
        WaitForSingleObject(thread_, INFINITE);
        CloseHandle(thread_);
    }

    DWORD workerThread()
    {
        printf("[Worker][%c] Created.\n", keycode_);

        for (;;) {
            Sleep(50);

            if (shutdown_) {
                break;
            }

            if(GetAsyncKeyState(keycode_) == 0) {
                continue;
            }

            int * v = pv_->acquire();
            printf("[Worker][%c] Changing: %d -> %d\n", keycode_, *v, valueToSet_);
            *v = valueToSet_;
            pv_->release();
        }

        printf("[Worker][%c] Shutdown.\n", keycode_);
        return 0;
    }

    static DWORD WINAPI workerThreadProc(void * param)
    {
        // Switch from a static func to a method
        KeyProtection * kp = static_cast<KeyProtection *>(param);
        return kp->workerThread();
    }
private:
    ProtectedValue *pv_;
    int keycode_;
    int valueToSet_;

    bool shutdown_;
    HANDLE thread_;
};

// ---------------------------------------------------------------------------------

int main(int argc, char * argv[])
{
    ProtectedValue theOneToRuleThemAll;

    static const size_t keyCount = 3;
    std::vector<KeyProtection> keyProtections;
    keyProtections.resize(keyCount);
    keyProtections[0].configure(&theOneToRuleThemAll, '1', 1);
    keyProtections[1].configure(&theOneToRuleThemAll, '2', 2);
    keyProtections[2].configure(&theOneToRuleThemAll, '3', 3);

    printf("[Main] Spawning worker threads...\n");
    for (auto it = keyProtections.begin(); it != keyProtections.end(); ++it) {
        it->start();
    }

    printf("[Main] Hold any of the 123 keys, or press Space to cleanup.\n");
    while (!GetAsyncKeyState(VK_SPACE)) {
        Sleep(50);
    }

    printf("[Main] Shutting down...\n");
    for (auto it = keyProtections.begin(); it != keyProtections.end(); ++it) {
        it->stop();
    }

    printf("[Main] Shutdown complete. Final value: %d\n", theOneToRuleThemAll.value());
    return 0;
}
