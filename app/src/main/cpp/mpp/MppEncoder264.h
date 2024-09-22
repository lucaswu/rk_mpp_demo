#pragma  once
#include  <memory>
#include "../3rd/mpp/inc/rk_mpi.h"
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#ifdef __cplusplus
}
#endif

namespace MPP{

    typedef struct{
        uint8_t *data;
        uint32_t size;
    }SpsHeader;
    class MppEncoder264 {
    public:
        MppEncoder264(int width,int height,int fps,int bitRate = 40000);
        ~MppEncoder264();
        bool getSpsInfo(SpsHeader *spsheader);
        void encoder(uint8_t *pData);
        void encoder(uint8_t*pData,AVPacket *pkt);

    private:
        bool init();

        bool mIsInit = false;
        int mWidth;
        int mHeight;
        int mFps ;
        int mBitRate;

        MppCodingType mEncoderType = MPP_VIDEO_CodingUnused;
        MppCtx mCtx = nullptr;
        MppApi *mMpi = nullptr;
        MppBufferGroup mBufferGroup = nullptr;
        MppBuffer mFrameBufer = nullptr;
        MppBuffer mPktBuffer = nullptr;
        MppEncCfg mMppEncCfg = nullptr;
        int mFrameSize = 0 ;
        MppPollType mTimeOut = MPP_POLL_BLOCK;


    };
}




