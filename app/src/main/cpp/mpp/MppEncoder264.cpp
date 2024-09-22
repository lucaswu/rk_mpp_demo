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
static void test()
{
    RK_S32 ret = MPP_NOK;

    MppApi * mpi;
    MppCtx ctx;
    int cfg_frame_width = 1280;
    int cfg_frame_height = 720;
    int cfg_hor_stride = MPP_ALIGN(cfg_frame_width, 16);	// for YUV420P it has tobe devided to 16
    int cfg_ver_stride = MPP_ALIGN(cfg_frame_height, 16);	// for YUV420P it has tobe devided to 16
    MppFrameFormat cfg_foramt = MPP_FMT_YUV420P;	// format of input frame YUV420P MppFrameFormat inside mpp_frame.h
    MppCodingType cfg_type = MPP_VIDEO_CodingAVC;
    int cfg_bps = 600000;
    int cfg_bps_min = 400000;
    int cfg_bps_max = 800000;
    int cfg_rc_mode = 0;	//0:vbr 1:cbr 2:avbr 3:cvbr 4:fixqp;
    int cfg_frame_num = 0;	//max encoding frame number

    int cfg_gop_mode = 0;
    int cfg_gop_len = 0;
    int cfg_vi_len = 0;
    int cfg_fps_in_flex = 0;
    int cfg_fps_in_den = 1;
    int cfg_fps_in_num = 8;	// input fps
    int cfg_fps_out_flex = 0;
    int cfg_fps_out_den = 1;
    int cfg_fps_out_num = 8;
    int cfg_frame_size = 0;
    int cfg_header_size = 0;
    int cfg_quiet = 0;

    MppBufferGroup buf_grp;
    MppBuffer frm_buf;
    MppBuffer pkt_buf;
    MppPollType timeout = MPP_POLL_BLOCK;
    MppEncCfg cfg;
    // get paramter from cmd


    // update resource parameter
    switch (cfg_foramt & MPP_FRAME_FMT_MASK)
    {
        case MPP_FMT_YUV420SP:
        case MPP_FMT_YUV420P:
        {
            cfg_frame_size = MPP_ALIGN(cfg_hor_stride, 64) *MPP_ALIGN(cfg_ver_stride, 64) *3 / 2;

        }
            break;
    }

    if (MPP_FRAME_FMT_IS_FBC(cfg_foramt))
        cfg_header_size = MPP_ALIGN(MPP_ALIGN(cfg_frame_width, 16) *MPP_ALIGN(cfg_frame_height, 16) / 16, SZ_4K);
    else
        cfg_header_size = 0;

    ret = mpp_buffer_group_get_internal(&buf_grp, MPP_BUFFER_TYPE_DRM);
    if (ret)
    {
        ZY_LOG_ASSERT("failed to get mpp buffer group ret %d\n", ret);
    }

    ret = mpp_buffer_get(buf_grp, &frm_buf, cfg_frame_size + cfg_header_size);
    if (ret)
    {
        ZY_LOG_ASSERT("failed to get buffer for input frame ret %d\n", ret);
    }
    ret = mpp_buffer_get(buf_grp, &pkt_buf, cfg_frame_size);
    if (ret)
    {
        ZY_LOG_ASSERT("failed to get buffer for output packet ret %d\n", ret);
    }

    ret = mpp_create(&ctx, &mpi);
    if (ret)
    {
        ZY_LOG_ASSERT("mpp_create failed ret %d\n", ret);
    }

    ZY_LOG( "%p encoder test start w %d h %d type %d\n",
              ctx, cfg_frame_width, cfg_frame_height, cfg_type);

    ret = mpi->control(ctx, MPP_SET_OUTPUT_TIMEOUT, &timeout);
    if (MPP_OK != ret)
    {
        ZY_LOG_ASSERT("mpi control set output timeout %d ret %d\n", timeout, ret);
    }

    ret = mpp_init(ctx, MPP_CTX_ENC, cfg_type);
    if (ret)
    {
        ZY_LOG_ASSERT("mpp_init failed ret %d\n", ret);
    }

    ret = mpp_enc_cfg_init(&cfg);
    if (ret)
    {
        ZY_LOG_ASSERT("mpp_enc_cfg_init failed ret %d\n", ret);
    }

    //configuring the ctx

    mpp_enc_cfg_set_s32(cfg, "prep:width", cfg_frame_width);
    mpp_enc_cfg_set_s32(cfg, "prep:height", cfg_frame_height);
    mpp_enc_cfg_set_s32(cfg, "prep:hor_stride", cfg_hor_stride);
    mpp_enc_cfg_set_s32(cfg, "prep:ver_stride", cfg_ver_stride);
    mpp_enc_cfg_set_s32(cfg, "prep:format", cfg_foramt);

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
                    ZY_LOG_ASSERT("unsupport encoder rc mode %d\n", cfg_rc_mode);
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
            mpp_enc_cfg_set_s32(cfg, "h264:profile", 66);
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
            ZY_LOG_ASSERT("unsupport encoder coding type %d\n", cfg_type);
        }
            break;
    }

    RK_U32 cfg_split_mode = 0;
    RK_U32 cfg_split_arg = 0;

    mpp_env_get_u32("split_mode", &cfg_split_mode, MPP_ENC_SPLIT_NONE);
    mpp_env_get_u32("split_arg", &cfg_split_arg, 0);

    ret = mpi->control(ctx, MPP_ENC_SET_CFG, cfg);
    if (ret)
    {
        ZY_LOG_ASSERT("mpi control enc set cfg failed ret %d\n", ret);
    }

    MppEncSeiMode cfg_sei_mode = MPP_ENC_SEI_MODE_ONE_FRAME;
    ret = mpi->control(ctx, MPP_ENC_SET_SEI_CFG, &cfg_sei_mode);
    if (ret)
    {
        ZY_LOG_ASSERT("mpi control enc set sei cfg failed ret %d\n", ret);
    }

    if (cfg_type == MPP_VIDEO_CodingAVC || cfg_type == MPP_VIDEO_CodingHEVC)
    {
        MppEncHeaderMode cfg_header_mode = MPP_ENC_HEADER_MODE_EACH_IDR;
        ret = mpi->control(ctx, MPP_ENC_SET_HEADER_MODE, &cfg_header_mode);
        if (ret)
        {
            ZY_LOG_ASSERT("mpi control enc set header mode failed ret %d\n", ret);
        }
    }

    RK_U32 gop_mode = cfg_gop_mode;

//    mpp_env_get_u32("gop_mode", &gop_mode, gop_mode);
//    if (gop_mode)
//    {
//        MppEncRefCfg ref;
//
//        mpp_enc_ref_cfg_init(&ref);
//
//        if (cfg_gop_mode < 4)
//            mpi_enc_gen_ref_cfg(ref, gop_mode);
//        else
//            mpi_enc_gen_smart_gop_ref_cfg(ref, cfg_gop_len, cfg_vi_len);
//
//        ret = mpi->control(ctx, MPP_ENC_SET_REF_CFG, ref);
//        if (ret)
//        {
//            mpp_err("mpi control enc set ref cfg failed ret %d\n", ret);
//        }
//        mpp_enc_ref_cfg_deinit(&ref);
//    }
//
//    RK_U32 cfg_osd_enable, cfg_osd_mode, cfg_roi_enable, cfg_user_data_enable;
//    /*setup test mode by env */
//    mpp_env_get_u32("osd_enable", &cfg_osd_enable, 0);
//    mpp_env_get_u32("osd_mode", &cfg_osd_mode, MPP_ENC_OSD_PLT_TYPE_DEFAULT);
//    mpp_env_get_u32("roi_enable", &cfg_roi_enable, 0);
//    mpp_env_get_u32("user_data_enable", &cfg_user_data_enable, 0);

    RK_S32 chn = 1;
    RK_U32 cap_num = 0;
//    DataCrc checkcrc;
    ret = MPP_OK;
//    memset(&checkcrc, 0, sizeof(checkcrc));
//    checkcrc.sum = mpp_malloc(RK_ULONG, 512);
    FILE * fp_output;
    std::string nameOut = std::string("/storage/emulated/0/Android/data/com.example.rk_mpp_test/cache/")+
            std::to_string(cfg_frame_width) +"x"+std::to_string(cfg_frame_height)+"_1.h264";
    fp_output = fopen(nameOut.c_str(), "wb");
    if (cfg_type == MPP_VIDEO_CodingAVC || cfg_type == MPP_VIDEO_CodingHEVC)
    {
        MppPacket packet = NULL;

        /*
         *Can use packet with normal malloc buffer as input not pkt_buf.
         *Please refer to vpu_api_legacy.cpp for normal buffer case.
         *Using pkt_buf buffer here is just for simplifing demo.
         */
        mpp_packet_init_with_buffer(&packet, pkt_buf);
        /*NOTE: It is important to clear output packet length!! */
        mpp_packet_set_length(packet, 0);

        ret = mpi->control(ctx, MPP_ENC_GET_HDR_SYNC, packet);
        if (ret)
        {
            ZY_LOG_ASSERT("mpi control enc get extra info failed\n");
        }
        else
        { /*get and write sps/pps for H.264 */

            void *ptr = mpp_packet_get_pos(packet);
            size_t len = mpp_packet_get_length(packet);

            //if (p->fp_output)
            fwrite(ptr, 1, len, fp_output);
        }

        mpp_packet_deinit(&packet);
    }

//    cv::Mat bgr_frame = cv::imread("/storage/emulated/0/Android/data/com.example.rk_mpp_test/cache/image.jpg");
//    cv::resize(bgr_frame, bgr_frame, cv::Size(cfg_frame_width, cfg_frame_height), 0, 0, cv::INTER_LINEAR);
//    cv::Mat yuv_frame;
//    cvtColor(bgr_frame, yuv_frame, cv::COLOR_BGR2YUV_I420 );
//
//    FILE*fp = fopen("/storage/emulated/0/Android/data/com.example.rk_mpp_test/cache/pic.yuv","wb");
//    if(fp == nullptr){
//        ZY_LOG("open file eror!\n");
//        return ;
//    }
//
//    fwrite(yuv_frame.data,1,cfg_frame_size,fp);
//    fclose(fp);


    std::string name = std::string("/storage/emulated/0/Android/data/com.example.rk_mpp_test/cache/")+
                          std::to_string(cfg_frame_width) +"x"+std::to_string(cfg_frame_height)+"_1.yuv";

    std::ifstream fin(name, std::ios::binary);
    if (!fin) {
        ZY_LOG("failed to open input file %s.\n", name.c_str());
        return ;
    }

    int bufferSize = cfg_frame_width*cfg_frame_height*3/2;
    uint8_t *pData = new uint8_t [bufferSize];
    while(!fin.eof()) {
        fin.read((char *) pData, bufferSize);
        auto len = fin.gcount();

//        static int index = 0 ;
//
//        std::string nameTemp =std::string("/storage/emulated/0/Android/data/com.example.rk_mpp_test/cache/temp/")+
//                std::to_string(index++)+"_"+
//                              std::to_string(cfg_frame_width) +"x"+std::to_string(cfg_frame_height)+".yuv";
//        FILE*fp = fopen(nameTemp.c_str(),"wb");
//        if(fp == nullptr){
//            ZY_LOG("open file eror!\n");
//            return ;
//        }
//
//        fwrite(pData,1,bufferSize,fp);
//        fclose(fp);

//        break;
//    }
//
//    fp = fopen("/storage/emulated/0/Android/data/com.example.rk_mpp_test/cache/buffer.yuv","wb");
//    if(fp == nullptr){
//        ZY_LOG("open file eror!\n");
//        return ;
//    }
//
//    fwrite(pData,1,cfg_frame_size,fp);
//    fclose(fp);
//
//
//   // memcpy(pData,yuv_frame.data,cfg_frame_size);
//
//    for (int frame_id = 0; frame_id < 20; frame_id++){

        MppMeta meta = NULL;
        MppFrame frame = NULL;
        MppPacket packet = NULL;
        //void *buf = mpp_buffer_get_ptr(frm_buf);
        RK_S32 cam_frm_idx = -1;
        MppBuffer cam_buf = NULL;
        RK_U32 eoi = 1;



        ret = mpp_frame_init(&frame);
        if (ret)
        {
            ZY_LOG_ASSERT("mpp_frame_init failed\n");
        }

        mpp_frame_set_width(frame, cfg_frame_width);
        mpp_frame_set_height(frame, cfg_frame_height);
        mpp_frame_set_hor_stride(frame, cfg_hor_stride);
        mpp_frame_set_ver_stride(frame, cfg_ver_stride);
        mpp_frame_set_fmt(frame, cfg_foramt);
        mpp_frame_set_eos(frame, 0);


        ret = mpp_buffer_write(frm_buf, 0, pData, bufferSize);
        if (ret)
        {
            ZY_LOG_ASSERT("could not write data on frame\n", chn);

        }
        mpp_frame_set_buffer(frame, frm_buf);
        meta = mpp_frame_get_meta(frame);
        mpp_packet_init_with_buffer(&packet, pkt_buf);
        /*NOTE: It is important to clear output packet length!! */
        mpp_packet_set_length(packet, 0);
        mpp_meta_set_packet(meta, KEY_OUTPUT_PACKET, packet);

        ret = mpi->encode_put_frame(ctx, frame);
        if (ret)
        {
            ZY_LOG_ASSERT("chn %d encode put frame failed\n", chn);
            mpp_frame_deinit(&frame);
        }
        mpp_frame_deinit(&frame);

        do {
            ret = mpi->encode_get_packet(ctx, &packet);
            if (ret)
            {
                ZY_LOG_ASSERT("chn %d encode get packet failed\n", chn);
            }

//            mpp_assert(packet);
            if (packet)
            {
                // write packet to file here
                void *ptr = mpp_packet_get_pos(packet);
                size_t len = mpp_packet_get_length(packet);
                fwrite(ptr, 1, len, fp_output);
//                std::cout<<"write packet to file here.len:"<<len<<"\n";
                if (mpp_packet_is_partition(packet))
                {
                    eoi = mpp_packet_is_eoi(packet);
                }
            }
            else{
                ZY_LOG("there is no packet\n");
            }
            mpp_packet_deinit(&packet);
        } while (!eoi);
    }

    if (ctx)
    {
        mpp_destroy(ctx);
        ctx = NULL;
    }

    if (cfg)
    {
        mpp_enc_cfg_deinit(cfg);
        cfg = NULL;
    }

    if (frm_buf)
    {
        mpp_buffer_put(frm_buf);
        frm_buf = NULL;
    }

    if (pkt_buf)
    {
        mpp_buffer_put(pkt_buf);
        pkt_buf = NULL;
    }

    if (buf_grp)
    {
        mpp_buffer_group_put(buf_grp);
        buf_grp = NULL;
    }
    fclose(fp_output);
    //uint8_t* test;
    //mpp_buffer_write(frm_buf,0,test,10);
    ZY_LOG( "runing succeessfully %s\n",nameOut.c_str());
//    return 0;
}


namespace MPP {
    MppEncoder264::MppEncoder264(int width, int height, int fps, int bitRate )
            : mWidth(width), mHeight(height), mFps(fps), mBitRate(bitRate) {
       // test();
    }

    MppEncoder264::~MppEncoder264() {

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
        int cfg_frame_num = 0;	//max encoding frame number

        int cfg_gop_mode = 0;
        int cfg_gop_len = 0;
        int cfg_vi_len = 0;
        int cfg_fps_in_flex = 0;
        int cfg_fps_in_den = 1;
        int cfg_fps_in_num = mFps;	// input fps
        int cfg_fps_out_flex = 0;
        int cfg_fps_out_den = 1;
        int cfg_fps_out_num = mFps;
        int cfg_frame_size = 0;
        int cfg_header_size = 0;
        int cfg_quiet = 0;

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

        RK_U32 gop_mode = cfg_gop_mode;


        RK_S32 chn = 1;
        RK_U32 cap_num = 0;
//    DataCrc checkcrc;
        ret = MPP_OK;
//    memset(&checkcrc, 0, sizeof(checkcrc));
//    checkcrc.sum = mpp_malloc(RK_ULONG, 512);

//        mFp_out = fopen("/storage/emulated/0/Android/data/com.example.rk_mpp_test/cache/2.h264", "wb");
//        if(mFp_out == NULL){
//            ZY_LOG("fopen file error!");
//            return false;
//        }

        if (cfg_type == MPP_VIDEO_CodingAVC || cfg_type == MPP_VIDEO_CodingHEVC)
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

            ret = mMpi->control(mCtx, MPP_ENC_GET_HDR_SYNC, packet);
            if (ret)
            {
                ZY_LOG_ASSERT("mpi control enc get extra info failed\n");
            }
            else
            { /*get and write sps/pps for H.264 */

                void *ptr = mpp_packet_get_pos(packet);
                size_t len = mpp_packet_get_length(packet);

                std::string fileName =
                        std::string("/storage/emulated/0/Android/data/com.example.rk_mpp_test/cache/2.h264");
                FILE*fp =fopen(fileName.c_str(),"ab+");
                if(fp == nullptr){
                    ZY_LOG(" fopen %s error!",fileName.c_str());
                    return false;
                }

                fwrite(ptr,1,len,fp);

                //if (p->fp_output)
//                fwrite(ptr, 1, len, mFp_out);
            }

            mpp_packet_deinit(&packet);
        }


        mIsInit = true;

        return true;

#if 0
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
        mpp_enc_cfg_set_s32(mMppEncCfg, "h264:profile", 100);
        /*
         *H.264 level_idc parameter
         *10 / 11 / 12 / 13    - qcif@15fps / cif@7.5fps / cif@15fps / cif@30fps
         *20 / 21 / 22         - cif@30fps / half-D1@@25fps / D1@12.5fps
         *30 / 31 / 32         - D1@25fps / 720p@30fps / 720p@60fps
         *40 / 41 / 42         - 1080p@30fps / 1080p@30fps / 1080p@60fps
         *50 / 51 / 52         - 4K@30fps
         */
        mpp_enc_cfg_set_s32(mMppEncCfg, "h264:level", 40);
        mpp_enc_cfg_set_s32(mMppEncCfg, "h264:cabac_en", 1);
        mpp_enc_cfg_set_s32(mMppEncCfg, "h264:cabac_idc", 0);
        mpp_enc_cfg_set_s32(mMppEncCfg, "h264:trans8x8", 1);

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

        ret = mpp_buffer_get(mMppBufferGroup,&mPktBuffer,mFrameSize);
        if( MPP_OK != ret){
            ZY_LOG("failed to get buffer for input frame ret %d\n", ret);
            return false;
        }

        mIsInit = true;
        return true;
#endif
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


#if 0
        ZYTime timer;
        auto ret = mpp_buffer_write(mMapBuffer,0,pData,mFrameSize);
        if(MPP_OK != ret){
            ZY_LOG("could not write data on frame\n");
            return;
        }

        mpp_frame_set_buffer(mFrameBufer,mMapBuffer);
        mpp_frame_set_eos(mFrameBufer, 0);


        auto meta = mpp_frame_get_meta(mFrameBufer);

        MppPacket  packet = nullptr;
        mpp_packet_init_with_buffer(&packet,mPktBuffer);
        mpp_packet_set_length(packet,0);
        mpp_meta_set_packet(meta,KEY_OUTPUT_PACKET,packet);

        ret = mMpi->encode_put_frame(mCtx,mFrameBufer);
        if(MPP_OK != ret){
            ZY_LOG("Failed to encode put frame,error:%d",ret);
            return;
        }

        RK_U32 eoi = 1;
        do{
            ret = mMpi->encode_get_packet(mCtx,&packet);
            if(MPP_OK != ret){
                ZY_LOG("encode_get_packet error!,Error:%d",ret);
                return;
            }

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

                if(mpp_packet_is_partition(packet)){
                    eoi = mpp_packet_is_eoi(packet);
                }
            }
        }while(!eoi);



        float differ = timer.GetTimeOfDuration();
        ZY_LOG(":%f",differ);
#endif

    }
}