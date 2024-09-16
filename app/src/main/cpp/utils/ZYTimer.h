#pragma  once

#include <sys/time.h>
#include <time.h>
class ZYTime {
public:
    ZYTime()
    {
        gettimeofday(&mStartTime,NULL);
    }
    float GetTimeOfDuration()
    {
        gettimeofday(&mEndTime,NULL);

        double elapsed = (mEndTime.tv_sec-mStartTime.tv_sec)  + (mEndTime.tv_usec - mStartTime.tv_usec)* .000001;
        return float(elapsed);
    }
private:
    struct timeval mStartTime;
    struct timeval mEndTime;
};
