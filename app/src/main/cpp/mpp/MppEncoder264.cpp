//
// Created by lucas on 2024/9/17.
//
#include <unistd.h>
#include <string>
#include "MppEncoder264.h"
#include "utils/ZYLog.h"
#include "../3rd/mpp/inc/mpp_common.h"
#include "../3rd/mpp/inc/mpp_env.h"
#include "utils/ZYTimer.h"
namespace MPP {
    MppEncoder264::MppEncoder264(int width, int height, int fps, int bitRate )
            : mWidth(width), mHeight(height), mFps(fps), mBitRate(bitRate) {

    }

    MppEncoder264::~MppEncoder264() {

    }

    bool MppEncoder264::init() {
        auto ret = mpp_create(&mCtx, &mMpi);
        if (MPP_OK != ret) {
            ZY_LOG("failed to create mpp ctx ret:%d.", ret);
            return false;
        }


        ret = mpp_init(mCtx, MPP_CTX_ENC, MPP_VIDEO_CodingAVC);
        if (MPP_OK != ret) {
            ZY_LOG("Failed to mpp_init,error:%d", ret);
            return false;
        }

        ret = mpp_buffer_group_get_internal(&mMppBufferGroup, MPP_BUFFER_TYPE_DMA_HEAP);
        if (MPP_OK != ret) {
            ZY_LOG("Failed to mpp_buffer_group_get_internal,error:%d\n", ret);
            return false;
        }

        ret = mpp_buffer_group_limit_config(mMppBufferGroup,0,20);
        if(MPP_OK != ret){
            ZY_LOG("Failed to mpp_buffer_group_limit_config,Error:%d\n",ret);
            return false;
        }

        ret = mpp_enc_cfg_init(&mMppEncCfg);
        if (MPP_OK != ret) {
            ZY_LOG("mpp_enc_cfg_init failed ret %d\n", ret);
            return false;
        }


        mpp_enc_cfg_set_s32(mMppEncCfg, "prep:width", mWidth);
        mpp_enc_cfg_set_s32(mMppEncCfg, "prep:height", mHeight);

        int cfg_hor_stride = MPP_ALIGN(mWidth, 16);    // for YUV420P it has tobe devided to 16
        int cfg_ver_stride = MPP_ALIGN(mHeight, 16);
        mpp_enc_cfg_set_s32(mMppEncCfg, "prep:hor_stride", cfg_hor_stride);
        mpp_enc_cfg_set_s32(mMppEncCfg, "prep:ver_stride", cfg_ver_stride);
        mpp_enc_cfg_set_s32(mMppEncCfg, "prep:format", MPP_FMT_YUV420P);

        int cfg_rc_mode = MPP_ENC_RC_MODE_VBR;    //0:vbr 1:cbr 2:avbr 3:cvbr 4:fixqp;
        mpp_enc_cfg_set_s32(mMppEncCfg, "rc:mode", cfg_rc_mode);

        /*fix input / output frame rate */
        mpp_enc_cfg_set_s32(mMppEncCfg, "rc:fps_in_flex", 0);
        //TODO:wcj 输入帧率
        mpp_enc_cfg_set_s32(mMppEncCfg, "rc:fps_in_num", mFps);
        mpp_enc_cfg_set_s32(mMppEncCfg, "rc:fps_in_denorm", 1);
        mpp_enc_cfg_set_s32(mMppEncCfg, "rc:fps_out_flex", 0);
        mpp_enc_cfg_set_s32(mMppEncCfg, "rc:fps_out_num", mFps);
        mpp_enc_cfg_set_s32(mMppEncCfg, "rc:fps_out_denorm", 1);
        mpp_enc_cfg_set_s32(mMppEncCfg, "rc:gop", mFps * 2);

        /*drop frame or not when bitrate overflow */
        mpp_enc_cfg_set_u32(mMppEncCfg, "rc:drop_mode", MPP_ENC_RC_DROP_FRM_DISABLED);
        mpp_enc_cfg_set_u32(mMppEncCfg, "rc:drop_thd", 20); /*20% of max bps */
        mpp_enc_cfg_set_u32(mMppEncCfg, "rc:drop_gap", 1); /*Do not continuous drop frame */

        /*setup bitrate for different rc_mode */
        mpp_enc_cfg_set_s32(mMppEncCfg, "rc:bps_target", mBitRate);

        switch (cfg_rc_mode) {
            case MPP_ENC_RC_MODE_FIXQP: {

            }
                break;
            case MPP_ENC_RC_MODE_CBR: {
                mpp_enc_cfg_set_s32(mMppEncCfg, "rc:bps_max", mBitRate * 17 / 16);
                mpp_enc_cfg_set_s32(mMppEncCfg, "rc:bps_min", mBitRate * 15 / 16);
            }
                break;
            case MPP_ENC_RC_MODE_VBR:
            case MPP_ENC_RC_MODE_AVBR: {
                mpp_enc_cfg_set_s32(mMppEncCfg, "rc:bps_max", mBitRate * 17 / 16);
                mpp_enc_cfg_set_s32(mMppEncCfg, "rc:bps_min", mBitRate * 15 / 16);
            }
                break;
            default: {
                mpp_enc_cfg_set_s32(mMppEncCfg, "rc:bps_max", mBitRate * 17 / 16);
                mpp_enc_cfg_set_s32(mMppEncCfg, "rc:bps_min", mBitRate * 15 / 16);
            }
                break;
        }


        switch (cfg_rc_mode) {
            case MPP_ENC_RC_MODE_FIXQP: {
                mpp_enc_cfg_set_s32(mMppEncCfg, "rc:qp_init", 20);
                mpp_enc_cfg_set_s32(mMppEncCfg, "rc:qp_max", 20);
                mpp_enc_cfg_set_s32(mMppEncCfg, "rc:qp_min", 20);
                mpp_enc_cfg_set_s32(mMppEncCfg, "rc:qp_max_i", 20);
                mpp_enc_cfg_set_s32(mMppEncCfg, "rc:qp_min_i", 20);
                mpp_enc_cfg_set_s32(mMppEncCfg, "rc:qp_ip", 2);
            }
                break;
            case MPP_ENC_RC_MODE_CBR:
            case MPP_ENC_RC_MODE_VBR:
            case MPP_ENC_RC_MODE_AVBR: {
                mpp_enc_cfg_set_s32(mMppEncCfg, "rc:qp_init", 26);
                mpp_enc_cfg_set_s32(mMppEncCfg, "rc:qp_max", 51);
                mpp_enc_cfg_set_s32(mMppEncCfg, "rc:qp_min", 10);
                mpp_enc_cfg_set_s32(mMppEncCfg, "rc:qp_max_i", 51);
                mpp_enc_cfg_set_s32(mMppEncCfg, "rc:qp_min_i", 10);
                mpp_enc_cfg_set_s32(mMppEncCfg, "rc:qp_ip", 2);
            }
                break;
            default: {
                ZY_LOG("unsupport encoder rc mode %d\n", cfg_rc_mode);
            }
                break;
        }

        /*setup codec  */

        mpp_enc_cfg_set_s32(mMppEncCfg, "codec:type", MPP_VIDEO_CodingAVC);

        /*
                     *H.264 profile_idc parameter
                     *66  - Baseline profile
                     *77  - Main profile
                     *100 - High profile
                     */
        mpp_enc_cfg_set_s32(mMppEncCfg, "h264:profile", 66);
        /*
         *H.264 level_idc parameter
         *10 / 11 / 12 / 13    - qcif@15fps / cif@7.5fps / cif@15fps / cif@30fps
         *20 / 21 / 22         - cif@30fps / half-D1@@25fps / D1@12.5fps
         *30 / 31 / 32         - D1@25fps / 720p@30fps / 720p@60fps
         *40 / 41 / 42         - 1080p@30fps / 1080p@30fps / 1080p@60fps
         *50 / 51 / 52         - 4K@30fps
         */
        mpp_enc_cfg_set_s32(mMppEncCfg, "h264:level", 31);
        mpp_enc_cfg_set_s32(mMppEncCfg, "h264:cabac_en", 1);
        mpp_enc_cfg_set_s32(mMppEncCfg, "h264:cabac_idc", 0);
        mpp_enc_cfg_set_s32(mMppEncCfg, "h264:trans8x8", 0);

        RK_U32 constraint_set;
        mpp_env_get_u32("constraint_set", &constraint_set, 0);
        if (constraint_set & 0x3f0000) {
            mpp_enc_cfg_set_s32(mMppEncCfg, "h264:constraint_set", constraint_set);
        }

        ret = mMpi->control(mCtx, MPP_ENC_SET_CFG, mMppEncCfg);
        if(MPP_OK != ret){
            ZY_LOG("mpi control enc set cfg failed ret %d\n", ret);
            return false;
        }

        ret = mpp_frame_init(&mFrameBufer);
        if(MPP_OK != ret){
            ZY_LOG("mpp_frame_init failed\n");
            return false;
        }


        mpp_frame_set_width(mFrameBufer, mWidth);
        mpp_frame_set_height(mFrameBufer, mHeight);
        mpp_frame_set_hor_stride(mFrameBufer, cfg_hor_stride);
        mpp_frame_set_ver_stride(mFrameBufer, cfg_ver_stride);
        mpp_frame_set_fmt(mFrameBufer, MPP_FMT_YUV420P);
        mpp_frame_set_eos(mFrameBufer, 0);

        auto cfg_frame_size = MPP_ALIGN(cfg_hor_stride,64)*MPP_ALIGN(cfg_ver_stride,64)*3/2;
        mFrameSize = mWidth*mHeight*3/2;

        auto flag = MPP_FRAME_FMT_IS_FBC(MPP_FMT_YUV420P);

        int cfg_header_size = 0 ;
        if(flag){
            cfg_header_size = MPP_ALIGN(MPP_ALIGN(mWidth, 16) *MPP_ALIGN(mHeight, 16) / 16, SZ_4K);
        }

        ret = mpp_buffer_get(mMppBufferGroup,&mMapBuffer,cfg_frame_size+cfg_header_size);
        if( MPP_OK != ret){
            ZY_LOG("failed to get buffer for input frame ret %d\n", ret);
            return false;
        }

        mIsInit = true;
        return true;

    }

    void MppEncoder264::encoder(uint8_t *pData){
        if(pData == nullptr){
            ZY_LOG("pData is nullptr");
            return;
        }

        if(mIsInit == false){
            auto ret = init();
            if(ret ==false){
                ZY_LOG("init error!");
                return;
            }
        }

        ZYTime timer;
        auto ret = mpp_buffer_write(mMapBuffer,0,pData,mFrameSize);
        if(MPP_OK != ret){
            ZY_LOG("could not write data on frame\n");
            return;
        }

        mpp_frame_set_buffer(mFrameBufer,mMapBuffer);

        ret = mMpi->encode_put_frame(mCtx,mFrameBufer);
        if(MPP_OK != ret){
            ZY_LOG("Failed to encode put frame,error:%d",ret);
            return;
        }

        MppPacket  packet = nullptr;
        ret = mMpi->encode_get_packet(mCtx,&packet);
        if(MPP_OK != ret){
            ZY_LOG("encode_get_packet error!,Error:%d",ret);
            return;
        }

        float differ = timer.GetTimeOfDuration();
        ZY_LOG(":%f",differ);

        if(packet){
            auto bufdata = mpp_packet_get_pos(packet);
            auto dataSize = mpp_packet_get_length(packet);


            std::string fileName =
                    std::string("/storage/emulated/0/Android/data/com.example.rk_mpp_test/cache/2.h264");
            FILE*fp =fopen(fileName.c_str(),"ab+");
            if(fp == nullptr){
                ZY_LOG(" fopen %s error!",fileName.c_str());
                return;
            }

            fwrite(bufdata,1,dataSize,fp);
            fclose(fp);
        }
    }
}