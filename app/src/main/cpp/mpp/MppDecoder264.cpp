//
// Created by lucas on 2024/9/15.
//
#include <unistd.h>
#include <string>
#include "MppDecoder264.h"
#include "utils/ZYLog.h"

namespace MPP{
    MppDecoder264::MppDecoder264(){

    }
    MppDecoder264::~MppDecoder264() {
        if(mBuffer){
           mBuffer.reset();
           mBuffer = nullptr;
        }

        deinit();
    }

    void MppDecoder264::deinit()
    {
        auto ret = mMpi->reset(mCtx);
        if(MPP_OK != ret){
            return;
        }
        if(mPkt){
            mpp_packet_deinit(&mPkt);
            mPkt = nullptr;
        }

        if(mCtx){
            mpp_destroy(mCtx);
            mCtx = nullptr;
        }

    }

    bool MppDecoder264::init() {
        auto ret = mpp_create(&mCtx,&mMpi);
        if(ret != MPP_SUCCESS){
            ZY_LOG("failed to create mpp ctx ret:%d.", ret);
            return false;
        }

        RK_U32  needSplit = -1;

        ret = mMpi->control(mCtx,MPP_DEC_SET_PARSER_SPLIT_MODE,(MppParam*)&needSplit);
        if(MPP_OK != ret){
            ZY_LOG("mpi->control error MPP_DEC_SET_PARSER_SPLIT_MODE\n");
            return false;
        }

        ret = mpp_init(mCtx,MPP_CTX_DEC,MPP_VIDEO_CodingAVC);
        if(MPP_OK!=ret){
            ZY_LOG("mpp_init error!\n");
            return false;
        }

//        mBuffer = new uint8_t[mBufferSize];
        mBuffer.reset(new uint8_t[mBufferSize], std::default_delete<uint8_t[]>());
        auto pBuffer = mBuffer.get();
        ret = mpp_packet_init(&mPkt,pBuffer,mBufferSize);
        if(MPP_OK != ret){
            ZY_LOG("mpp_packet_init error\n");
            return false;
        }

        mIsInit = true;
        return true;
    }

    void MppDecoder264::dumFrame(MppFrame frame) {
        if(mpp_frame_get_info_change(frame)){
            auto ret = mMpi->control(mCtx,MPP_DEC_SET_INFO_CHANGE_READY,NULL);
            if (ret) {
                ZY_LOG("mpp_frame_get_info_change mpi->control error"
                       "MPP_DEC_SET_INFO_CHANGE_READY %d\n", ret);
            }
            return;
        }

        auto errInfo = mpp_frame_get_errinfo(frame);
        auto discard = mpp_frame_get_discard(frame);
        if(errInfo){
            return;
        }

        auto width = mpp_frame_get_width(frame);
        auto height = mpp_frame_get_height(frame);
        auto hStride = mpp_frame_get_hor_stride(frame);
        auto vStride = mpp_frame_get_ver_stride(frame);
        auto fmt = mpp_frame_get_fmt(frame);
        auto buffer = mpp_frame_get_buffer(frame);

        auto bufferSize = mpp_frame_get_buf_size(frame);

        ZY_LOG("w x h: %dx%d hor_stride:%d ver_stride:%d buf_size:%d\n",
               width, height, hStride, vStride, bufferSize);
        if(buffer == nullptr){
            ZY_LOG("buffer is nullptr");
            return;
        }
        if(fmt != MPP_FMT_YUV420SP){
            ZY_LOG("fmt %d not supported!\n",fmt);
            return;
        }

//        auto pBase = (RK_U8*)mpp_buffer_get_ptr(buffer);
        auto pY = (RK_U8*)mpp_buffer_get_ptr(buffer);;
        auto pUV = pY + hStride *vStride;

        static int index = 0 ;
        std::string fileName = std::string("/storage/emulated/0/Android/data/com.example.rk_mpp_test/cache/")+
                std::to_string(index++)+"_"+std::to_string(width) +"x"+std::to_string(height)+".yuv";
        FILE*fp = fopen(fileName.c_str(),"wb");
        if(fp == nullptr){
            ZY_LOG("open %s error!",fileName.c_str());
            return;
        }
        for(int i =0 ;i<height;i++){
            fwrite(pY,1,width,fp);
            pY += hStride;
        }

        for(int i = 0 ;i<height/2;i++){
            fwrite(pUV,1,width,fp);
            pUV += hStride;
        }

        fclose(fp);

        ZY_LOG("size:(%d,%d),");
        ZY_LOG("save %s\n",fileName.c_str());

    }

    void MppDecoder264::decode(uint8_t *pData,int size,RK_U32 eos){
        if(mIsInit == false){
            init();
        }
        if(pData == nullptr){
            return;
        }

        auto pBuffer = mBuffer.get();
        memcpy(pBuffer,pData,size);

        mpp_packet_write(mPkt,0,pBuffer,size);
        mpp_packet_set_pos(mPkt,pBuffer);
        mpp_packet_set_length(mPkt,size);
        if(eos){
            mEos = eos;
            mpp_packet_set_eos(mPkt);
        }

        /*非组赛模式*/

        auto times = 5;
        bool pktDone = false;
        while(1){
            if(!pktDone){
                auto ret = mMpi->decode_put_packet(mCtx,mPkt);
                if(MPP_OK == ret){
                    pktDone = true;
                }
            }

            while(1){
                MppFrame  frame = nullptr;
                RK_U32 frameEos = 0;
                RK_U32 getFrame  = 0;
                auto ret = mMpi->decode_get_frame(mCtx,&frame);
                if(MPP_ERR_TIMEOUT == ret ){
                    if(times > 0){
                        times--;
                        usleep(2000);
                        continue;
                    }
                    ZY_LOG("decode_get_frame failed too much time\n");
                }
                if(MPP_OK != ret){
                    ZY_LOG("decode_get_frame failed ret %d\n", ret);
                    break;
                }

                if(frame){

                    //toDO
                    dumFrame(frame);

                    frameEos = mpp_frame_get_eos(frame);
                    mpp_frame_deinit(&frame);
                    frame = nullptr;
                    getFrame = 1;
                }
                if(mEos && pktDone &&!frameEos){
                    usleep(10 * 1000);
                    continue;
                }

                if(frameEos){
                    ZY_LOG("found last frame %d\n", pktDone);
                    break;
                }
                if(getFrame){
                    continue;
                }

                break;
            }
            if(pktDone){
                break;
            }
            usleep(50 * 1000);
        };

//        bool pktIsSend = false;
//        while(!pktIsSend){
//            if(size > 0){
//                auto ret = mMpi->decode_put_packet(mCtx,mPkt);
//                if(MPP_OK == ret){
//                    ZY_LOG("pkt send success remain:%d\n", mpp_packet_get_length(mPkt));
//                    pktIsSend = true;
//                }
//            }
//
//            MppFrame frame = nullptr;
//            auto ret = mMpi->decode_get_frame(mCtx,&frame);
//            if(MPP_OK !=ret || !frame){
//                ZY_LOG("decode_get_frame falied ret:%d\n", ret);
//                usleep(2000);  // 等待一下2ms，通常1080p解码时间2ms
//                continue;
//            }
//        }

    }
}
