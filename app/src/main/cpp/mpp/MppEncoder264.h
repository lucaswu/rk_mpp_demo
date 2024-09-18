#pragma  once
#include  <memory>
#include "../3rd/mpp/inc/rk_mpi.h"
namespace MPP{
    class MppEncoder264 {
    public:
        MppEncoder264(int width,int height,int fps,int bitRate = 40000);
        ~MppEncoder264();
        void encoder(uint8_t *pData);

    private:
        bool init();

        bool mIsInit = false;
        int mWidth;
        int mHeight;
        int mFps ;
        int mBitRate;

        MppCtx mCtx = nullptr;
        MppApi *mMpi = nullptr;
        MppBufferGroup mMppBufferGroup = nullptr;
        MppBuffer mFrameBufer = nullptr;
        MppBuffer mMapBuffer = nullptr;
        MppEncCfg mMppEncCfg = nullptr;
        int mFrameSize = 0 ;
    };
}




