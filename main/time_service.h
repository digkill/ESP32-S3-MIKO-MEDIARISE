#ifndef TIME_SERVICE_H
#define TIME_SERVICE_H

struct timeval;

class TimeService {
public:
    static TimeService& GetInstance();

    void ApplyTimezone();
    void Start();
    bool IsTimeValid() const;

private:
    TimeService() = default;
    TimeService(const TimeService&) = delete;
    TimeService& operator=(const TimeService&) = delete;

    static void SyncTask(void* arg);
    static void OnSntpSync(struct timeval* tv);

    bool started_ = false;
};

#endif // TIME_SERVICE_H
