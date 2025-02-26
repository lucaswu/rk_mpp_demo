//
// Created by lucas on 2024/9/17.
//
#include <unistd.h>
#include <string>
#include <fstream>
#include "MppEncoder264.h"
#include "utils/ZYLog.h"
#include "../3rd/mpp/inc/mpp_common.h"
#include "../3rd/mpp/inc/mpp_env.h"
#include "utils/ZYTimer.h"

#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"

#define MPP_PACKET_FLAG_INTRA       (0x00000008)

namespace MPP {
    MppEncoder264::MppEncoder264(int width, int height, int fps, int bitRate )
            : mWidth(width), mHeight(height), mFps(fps), mBitRate(bitRate) {
    }

    MppEncoder264::~MppEncoder264() {
        if(mBufferGroup){
            mpp_buffer_group_put(mBufferGroup);
            mBufferGroup = nullptr;
        }

        if(mPktBuffer){
            mpp_buffer_put(mPktBuffer);
            mPktBuffer = nullptr;
        }

        if(mFrameBufer){
            mpp_buffer_put(mFrameBufer);
            mFrameBufer = nullptr;
        }
        if(mMppEncCfg){
            mpp_enc_cfg_deinit(mMppEncCfg);
        }

        if(mCtx){
            mpp_destroy(mCtx);
            mCtx = nullptr;
        }
    }

    bool MppEncoder264::init() {

        int cfg_frame_width = mWidth;
        int cfg_frame_height = mHeight;
        int cfg_hor_stride = MPP_ALIGN(cfg_frame_width, 16);	// for YUV420P it has tobe devided to 16
        int cfg_ver_stride = MPP_ALIGN(cfg_frame_height, 16);	// for YUV420P it has tobe devided to 16
        MppFrameFormat cfg_format = MPP_FMT_YUV420P;	// format of input frame YUV420P MppFrameFormat inside mpp_frame.h
        MppCodingType cfg_type = MPP_VIDEO_CodingAVC;
        int cfg_bps = mBitRate;
        int cfg_bps_min = mBitRate;
        int cfg_bps_max = mBitRate;
        int cfg_rc_mode = 0;	//0:vbr 1:cbr 2:avbr 3:cvbr 4:fixqp;
        int cfg_gop_len = 0;
        int cfg_fps_in_flex = 0;
        int cfg_fps_in_den = 1;
        int cfg_fps_in_num = mFps;	// input fps
        int cfg_fps_out_flex = 0;
        int cfg_fps_out_den = 1;
        int cfg_fps_out_num = mFps;
        int cfg_frame_size = 0;
        int cfg_header_size = 0;


        MppEncCfg cfg;

        switch (cfg_format & MPP_FRAME_FMT_MASK) {
            case MPP_FMT_YUV420P:
            case MPP_FMT_YUV420SP:
                {
                    cfg_frame_size = MPP_ALIGN(cfg_hor_stride,64) * MPP_ALIGN(cfg_ver_stride,64)*3/2;
                }
                break;
        }

        mFrameSize = mWidth*mHeight*3/2;

        if(MPP_FRAME_FMT_IS_FBC(cfg_format)){
            cfg_header_size = MPP_ALIGN(MPP_ALIGN(cfg_frame_width, 16) *MPP_ALIGN(cfg_frame_height, 16) / 16, SZ_4K);
        }
        else{
            cfg_header_size = 0 ;
        }

        auto ret = mpp_buffer_group_get_internal(&mBufferGroup,MPP_BUFFER_TYPE_DRM);
        if(ret){
            ZY_LOG("failed to get mpp buffer group ret %d\n",ret);
            return false;
        }

        ret = mpp_buffer_get(mBufferGroup,&mFrameBufer,cfg_frame_size+cfg_header_size);
        if(ret){
            ZY_LOG("faled to get buffer for input frame ret %d\n",ret);
            return false;
        }

        ret = mpp_buffer_get(mBufferGroup,&mPktBuffer,cfg_frame_size);
        if(ret){
            ZY_LOG("Failed to get buffer for output packet ret %d\n",ret);
            return false;
        }

        ret = mpp_create(&mCtx,&mMpi);
        if(ret){
            ZY_LOG("mpp create error ret %d\n",ret);
            return false;
        }

        ret = mMpi->control(mCtx,MPP_SET_OUTPUT_TIMEOUT,&mTimeOut);
        if(ret){
            ZY_LOG("mpi control set output timeout %d ret %d\n",mTimeOut,ret);
            return false;
        }

        ret = mpp_init(mCtx,MPP_CTX_ENC,cfg_type);
        if(ret){
            ZY_LOG("mpp_init error ret %d\n",ret);
            return false;
        }

        ret = mpp_enc_cfg_init(&cfg);
        if(ret){
            ZY_LOG("mpp_enc_cfg_init error ret %d",ret);
            return false;
        }

        //configuring the ctx

        mpp_enc_cfg_set_s32(cfg, "prep:width", cfg_frame_width);
        mpp_enc_cfg_set_s32(cfg, "prep:height", cfg_frame_height);
        mpp_enc_cfg_set_s32(cfg, "prep:hor_stride", cfg_hor_stride);
        mpp_enc_cfg_set_s32(cfg, "prep:ver_stride", cfg_ver_stride);
        mpp_enc_cfg_set_s32(cfg, "prep:format", cfg_format);

        mpp_enc_cfg_set_s32(cfg, "rc:mode", cfg_rc_mode);

        /*fix input / output frame rate */
        mpp_enc_cfg_set_s32(cfg, "rc:fps_in_flex", cfg_fps_in_flex);
        mpp_enc_cfg_set_s32(cfg, "rc:fps_in_num", cfg_fps_in_num);
        mpp_enc_cfg_set_s32(cfg, "rc:fps_in_denorm", cfg_fps_in_den);
        mpp_enc_cfg_set_s32(cfg, "rc:fps_out_flex", cfg_fps_out_flex);
        mpp_enc_cfg_set_s32(cfg, "rc:fps_out_num", cfg_fps_out_num);
        mpp_enc_cfg_set_s32(cfg, "rc:fps_out_denorm", cfg_fps_out_den);
        mpp_enc_cfg_set_s32(cfg, "rc:gop", cfg_gop_len ? cfg_gop_len : cfg_fps_out_num *2);

        /*drop frame or not when bitrate overflow */
        mpp_enc_cfg_set_u32(cfg, "rc:drop_mode", MPP_ENC_RC_DROP_FRM_DISABLED);
        mpp_enc_cfg_set_u32(cfg, "rc:drop_thd", 20); /*20% of max bps */
        mpp_enc_cfg_set_u32(cfg, "rc:drop_gap", 1); /*Do not continuous drop frame */

        /*setup bitrate for different rc_mode */
        mpp_enc_cfg_set_s32(cfg, "rc:bps_target", cfg_bps);
        switch (cfg_rc_mode)
        {
            case MPP_ENC_RC_MODE_FIXQP:
            { 	/*do not setup bitrate on FIXQP mode */
            }
                break;
            case MPP_ENC_RC_MODE_CBR:
            { 	/*CBR mode has narrow bound */
                mpp_enc_cfg_set_s32(cfg, "rc:bps_max", cfg_bps_max ? cfg_bps_max : cfg_bps *17 / 16);
                mpp_enc_cfg_set_s32(cfg, "rc:bps_min", cfg_bps_min ? cfg_bps_min : cfg_bps *15 / 16);
            }
                break;
            case MPP_ENC_RC_MODE_VBR:
            case MPP_ENC_RC_MODE_AVBR:
            { 	/*VBR mode has wide bound */
                mpp_enc_cfg_set_s32(cfg, "rc:bps_max", cfg_bps_max ? cfg_bps_max : cfg_bps *17 / 16);
                mpp_enc_cfg_set_s32(cfg, "rc:bps_min", cfg_bps_min ? cfg_bps_min : cfg_bps *1 / 16);
            }
                break;
            default:
            { 	/*default use CBR mode */
                mpp_enc_cfg_set_s32(cfg, "rc:bps_max", cfg_bps_max ? cfg_bps_max : cfg_bps *17 / 16);
                mpp_enc_cfg_set_s32(cfg, "rc:bps_min", cfg_bps_min ? cfg_bps_min : cfg_bps *15 / 16);
            }
                break;
        }

        /*setup qp for different codec and rc_mode */
        switch (cfg_type)
        {
            case MPP_VIDEO_CodingAVC:
            case MPP_VIDEO_CodingHEVC:
            {
                switch (cfg_rc_mode)
                {
                    case MPP_ENC_RC_MODE_FIXQP:
                    {
                        mpp_enc_cfg_set_s32(cfg, "rc:qp_init", 20);
                        mpp_enc_cfg_set_s32(cfg, "rc:qp_max", 20);
                        mpp_enc_cfg_set_s32(cfg, "rc:qp_min", 20);
                        mpp_enc_cfg_set_s32(cfg, "rc:qp_max_i", 20);
                        mpp_enc_cfg_set_s32(cfg, "rc:qp_min_i", 20);
                        mpp_enc_cfg_set_s32(cfg, "rc:qp_ip", 2);
                    }
                        break;
                    case MPP_ENC_RC_MODE_CBR:
                    case MPP_ENC_RC_MODE_VBR:
                    case MPP_ENC_RC_MODE_AVBR:
                    {
                        mpp_enc_cfg_set_s32(cfg, "rc:qp_init", 26);
                        mpp_enc_cfg_set_s32(cfg, "rc:qp_max", 51);
                        mpp_enc_cfg_set_s32(cfg, "rc:qp_min", 10);
                        mpp_enc_cfg_set_s32(cfg, "rc:qp_max_i", 51);
                        mpp_enc_cfg_set_s32(cfg, "rc:qp_min_i", 10);
                        mpp_enc_cfg_set_s32(cfg, "rc:qp_ip", 2);
                    }
                        break;
                    default:
                    {
                        ZY_LOG("unsupport encoder rc mode %d\n", cfg_rc_mode);
                    }
                        break;
                }
            }
                break;
            case MPP_VIDEO_CodingVP8:
            { 	/*vp8 only setup base qp range */
                mpp_enc_cfg_set_s32(cfg, "rc:qp_init", 40);
                mpp_enc_cfg_set_s32(cfg, "rc:qp_max", 127);
                mpp_enc_cfg_set_s32(cfg, "rc:qp_min", 0);
                mpp_enc_cfg_set_s32(cfg, "rc:qp_max_i", 127);
                mpp_enc_cfg_set_s32(cfg, "rc:qp_min_i", 0);
                mpp_enc_cfg_set_s32(cfg, "rc:qp_ip", 6);
            }
                break;
            case MPP_VIDEO_CodingMJPEG:
            { 	/*jpeg use special codec config to control qtable */
                mpp_enc_cfg_set_s32(cfg, "jpeg:q_factor", 80);
                mpp_enc_cfg_set_s32(cfg, "jpeg:qf_max", 99);
                mpp_enc_cfg_set_s32(cfg, "jpeg:qf_min", 1);
            }
                break;
            default: {}
                break;
        }

        /*setup codec  */
        mpp_enc_cfg_set_s32(cfg, "codec:type", cfg_type);
        switch (cfg_type)
        {
            case MPP_VIDEO_CodingAVC:
            { 	/*
				 *H.264 profile_idc parameter
				 *66  - Baseline profile
				 *77  - Main profile
				 *100 - High profile
				 */
                mpp_enc_cfg_set_s32(cfg, "h264:profile", 100);
                /*
                 *H.264 level_idc parameter
                 *10 / 11 / 12 / 13    - qcif@15fps / cif@7.5fps / cif@15fps / cif@30fps
                 *20 / 21 / 22         - cif@30fps / half-D1@@25fps / D1@12.5fps
                 *30 / 31 / 32         - D1@25fps / 720p@30fps / 720p@60fps
                 *40 / 41 / 42         - 1080p@30fps / 1080p@30fps / 1080p@60fps
                 *50 / 51 / 52         - 4K@30fps
                 */
                mpp_enc_cfg_set_s32(cfg, "h264:level", 40);
                mpp_enc_cfg_set_s32(cfg, "h264:cabac_en", 1);
                mpp_enc_cfg_set_s32(cfg, "h264:cabac_idc", 0);
                mpp_enc_cfg_set_s32(cfg, "h264:trans8x8", 0);
            }
                break;
            case MPP_VIDEO_CodingHEVC:
            case MPP_VIDEO_CodingMJPEG:
            case MPP_VIDEO_CodingVP8: {}
                break;
            default:
            {
                ZY_LOG("unsupport encoder coding type %d\n", cfg_type);
            }
                break;
        }

        RK_U32 cfg_split_mode = 0;
        RK_U32 cfg_split_arg = 0;

        mpp_env_get_u32("split_mode", &cfg_split_mode, MPP_ENC_SPLIT_NONE);
        mpp_env_get_u32("split_arg", &cfg_split_arg, 0);

        ret = mMpi->control(mCtx,MPP_ENC_SET_CFG,cfg);
        if(ret){
            ZY_LOG("mpi control enc set cfg failed ret %d",ret);
            return false;
        }

        MppEncSeiMode cfg_sei_mode = MPP_ENC_SEI_MODE_ONE_FRAME;
        ret = mMpi->control(mCtx, MPP_ENC_SET_SEI_CFG, &cfg_sei_mode);
        if (ret)
        {
            ZY_LOG_ASSERT("mpi control enc set sei cfg failed ret %d\n", ret);
        }

        if (cfg_type == MPP_VIDEO_CodingAVC || cfg_type == MPP_VIDEO_CodingHEVC)
        {
            MppEncHeaderMode cfg_header_mode = MPP_ENC_HEADER_MODE_EACH_IDR;
            ret = mMpi->control(mCtx, MPP_ENC_SET_HEADER_MODE, &cfg_header_mode);
            if (ret)
            {
                ZY_LOG_ASSERT("mpi control enc set header mode failed ret %d\n", ret);
            }
        }



        mEncoderType = cfg_type;

        mIsInit = true;

        return true;

    }

    bool  MppEncoder264::getSpsInfo(SpsHeader *spsheader){
        if(spsheader == nullptr){
            ZY_LOG("spsheader is nullptr");
            return false;
        }
        if(mIsInit == false){
            init();
        }
        if (mEncoderType == MPP_VIDEO_CodingAVC || mEncoderType == MPP_VIDEO_CodingHEVC)
        {
            MppPacket packet = NULL;

            /*
             *Can use packet with normal malloc buffer as input not pkt_buf.
             *Please refer to vpu_api_legacy.cpp for normal buffer case.
             *Using pkt_buf buffer here is just for simplifing demo.
             */
            mpp_packet_init_with_buffer(&packet, mPktBuffer);
            /*NOTE: It is important to clear output packet length!! */
            mpp_packet_set_length(packet, 0);

            auto ret = mMpi->control(mCtx, MPP_ENC_GET_HDR_SYNC, packet);
            if (ret)
            {
                ZY_LOG_ASSERT("mpi control enc get extra info failed\n");
                return false;
            }
            else
            { /*get and write sps/pps for H.264 */

                void *ptr = mpp_packet_get_pos(packet);
                size_t len = mpp_packet_get_length(packet);

                spsheader->data = (uint8_t*)malloc(len);
                spsheader->size = len;
                memcpy(spsheader->data,ptr,len);
            }

            mpp_packet_deinit(&packet);
        }

        return true;

    }

    void MppEncoder264::encoder(uint8_t *pData, AVPacket *pkt) {

        if(pData == nullptr || pkt == nullptr){
            ZY_LOG("input is nullptr");
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

        MppFrame frame = nullptr;
        MppPacket packet = nullptr;
        auto ret = mpp_frame_init(&frame);
        if(ret){
            ZY_LOG("mpp_frame init failed !");
            return;
        }


        mpp_frame_set_width(frame, mWidth);
        mpp_frame_set_height(frame, mHeight);
        mpp_frame_set_hor_stride(frame, MPP_ALIGN(mWidth,16));
        mpp_frame_set_ver_stride(frame, MPP_ALIGN(mHeight,16));
        mpp_frame_set_fmt(frame, MPP_FMT_YUV420P);
        mpp_frame_set_eos(frame, 0);

        ret = mpp_buffer_write(mFrameBufer,0,pData,mFrameSize);
        if(ret){
            ZY_LOG("mpp_buffer_write failed!");
            return;
        }

        mpp_frame_set_buffer(frame,mFrameBufer);
        auto meta = mpp_frame_get_meta(frame);
        mpp_packet_init_with_buffer(&packet,mPktBuffer);
        mpp_packet_set_length(packet,0);
        mpp_meta_set_packet(meta,KEY_OUTPUT_PACKET,packet);

        ret = mMpi->encode_put_frame(mCtx,frame);
        if(ret){
            ZY_LOG("encoder put frame failed!");
            return;
        }

        mpp_frame_deinit(&frame);

        RK_U32 eoi = 1;
        do{
            ret = mMpi->encode_get_packet(mCtx,&packet);
            if(ret){
                ZY_LOG("encode get packet error!\n");
                return;
            }

            float differ = timer.GetTimeOfDuration();
            ZY_LOG(":%f",differ);
            if(packet){
                void*ptr = mpp_packet_get_pos(packet);
                auto len = mpp_packet_get_length(packet);

                pkt->data = (uint8_t*)mpp_packet_get_data(packet);
                pkt->size = mpp_packet_get_length(packet);
                pkt->pts = mpp_packet_get_pts(packet);
                pkt->dts = mpp_packet_get_dts(packet);
                if (pkt->pts <= 0)
                    pkt->pts = pkt->dts;
                if (pkt->dts <= 0)
                    pkt->dts = pkt->pts;
                auto flag = mpp_packet_get_flag(packet);
                if (flag & MPP_PACKET_FLAG_INTRA)
                    pkt->flags |= AV_PKT_FLAG_KEY;
//                std::string fileName =
//                        std::string("/storage/emulated/0/Android/data/com.example.rk_mpp_test/cache/2.h264");
//                FILE*fp =fopen(fileName.c_str(),"ab+");
//                if(fp == nullptr){
//                    ZY_LOG(" fopen %s error!",fileName.c_str());
//                    return;
//                }
//
//                fwrite(ptr,1,len,fp);
//                fclose(fp);
                if(mpp_packet_is_partition(packet)){
                    eoi = mpp_packet_is_eoi(packet);
                }
            }

            mpp_packet_deinit(&packet);
        }while(!eoi);
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

        MppFrame frame = nullptr;
        MppPacket packet = nullptr;
        auto ret = mpp_frame_init(&frame);
        if(ret){
            ZY_LOG("mpp_frame init failed !");
            return;
        }


        mpp_frame_set_width(frame, mWidth);
        mpp_frame_set_height(frame, mHeight);
        mpp_frame_set_hor_stride(frame, MPP_ALIGN(mWidth,16));
        mpp_frame_set_ver_stride(frame, MPP_ALIGN(mHeight,16));
        mpp_frame_set_fmt(frame, MPP_FMT_YUV420P);
        mpp_frame_set_eos(frame, 0);

        ret = mpp_buffer_write(mFrameBufer,0,pData,mFrameSize);
        if(ret){
            ZY_LOG("mpp_buffer_write failed!");
            return;
        }

        mpp_frame_set_buffer(frame,mFrameBufer);
        auto meta = mpp_frame_get_meta(frame);
        mpp_packet_init_with_buffer(&packet,mPktBuffer);
        mpp_packet_set_length(packet,0);
        mpp_meta_set_packet(meta,KEY_OUTPUT_PACKET,packet);

        ret = mMpi->encode_put_frame(mCtx,frame);
        if(ret){
            ZY_LOG("encoder put frame failed!");
            return;
        }

        mpp_frame_deinit(&frame);

        RK_U32 eoi = 1;
        do{
            ret = mMpi->encode_get_packet(mCtx,&packet);
            if(ret){
                ZY_LOG("encode get packet error!\n");
                return;
            }

            float differ = timer.GetTimeOfDuration();
            ZY_LOG(":%f",differ);
            if(packet){
                void*ptr = mpp_packet_get_pos(packet);
                auto len = mpp_packet_get_length(packet);

               // fwrite(ptr,1,len,mFp_out);
                std::string fileName =
                        std::string("/storage/emulated/0/Android/data/com.example.rk_mpp_test/cache/2.h264");
                FILE*fp =fopen(fileName.c_str(),"ab+");
                if(fp == nullptr){
                    ZY_LOG(" fopen %s error!",fileName.c_str());
                    return;
                }

                fwrite(ptr,1,len,fp);
                fclose(fp);
                if(mpp_packet_is_partition(packet)){
                    eoi = mpp_packet_is_eoi(packet);
                }
            }

            mpp_packet_deinit(&packet);
        }while(!eoi);


    }
}