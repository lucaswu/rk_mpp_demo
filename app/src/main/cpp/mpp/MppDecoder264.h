#pragma  once
#include  <memory>
#include "../3rd/mpp/inc/rk_mpi.h"
namespace MPP{
    class MppDecoder264{
    public:
        MppDecoder264();
        ~MppDecoder264();
        void decode(uint8_t *pData,int size,RK_U32 eos);
    private:
        bool init();
        void dumFrame(MppFrame frame);

        bool mIsInit = false;
        MppCtx mCtx = nullptr;
        MppApi *mMpi = nullptr;

        std::shared_ptr<uint8_t> mBuffer = nullptr;
//        uint8_t *mBuffer = nullptr;
        const int mBufferSize = 5*1024*1024;
        MppPacket  mPkt = nullptr;
        RK_U32 mEos = 0 ;
    };
}
