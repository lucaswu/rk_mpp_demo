//
// Created by lucas on 2024/9/17.
//
#include <unistd.h>
#include <string>
#include "MppEncoder264.h"
#include "utils/ZYLog.h"

namespace MPP {
    MppEncoder264::MppEncoder264(int width, int height, int fps, int bitRate = 40000)
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
    }
}