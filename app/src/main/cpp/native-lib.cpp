#include <jni.h>
#include <string>
#include <memory>
#include "mpp/MppDecoder264.h"
#include "mpp/MppEncoder264.h"
#include <fstream>
#include "utils/ZYLog.h"

extern "C" {
#include <libavcodec/version.h>
#include <libavcodec/avcodec.h>
#include <libavformat/version.h>
#include <libavutil/version.h>
#include <libavfilter/version.h>
#include <libswresample/version.h>
#include <libswscale/version.h>
};


std::shared_ptr<MPP::MppDecoder264> gMppDecoder264;
std::shared_ptr<MPP::MppEncoder264> gMppEncoder264;

void test(){
    char strBuffer[1024 * 4] = {0};
    strcat(strBuffer, "libavcodec : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVCODEC_VERSION));
    strcat(strBuffer, "\nlibavformat : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVFORMAT_VERSION));
    strcat(strBuffer, "\nlibavutil : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVUTIL_VERSION));
    strcat(strBuffer, "\nlibavfilter : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVFILTER_VERSION));
    strcat(strBuffer, "\nlibswresample : ");
    strcat(strBuffer, AV_STRINGIFY(LIBSWRESAMPLE_VERSION));
    strcat(strBuffer, "\nlibswscale : ");
    strcat(strBuffer, AV_STRINGIFY(LIBSWSCALE_VERSION));
    strcat(strBuffer, "\navcodec_configure : \n");
    strcat(strBuffer, avcodec_configuration());
    strcat(strBuffer, "\navcodec_license : ");
    strcat(strBuffer, avcodec_license());

    ZY_LOG("avcodec_configure :%s\n",avcodec_configuration());

}



extern "C"
JNIEXPORT void JNICALL
Java_com_example_rk_1mpp_1test_MainActivity_testDecoder(JNIEnv *env, jobject thiz) {

    test();
    // TODO: implement testDecoder()
    gMppDecoder264 = std::make_shared<MPP::MppDecoder264>();

    std::string file_name = "/storage/emulated/0/Android/data/com.example.rk_mpp_test/cache/2.h264";

    std::ifstream fin(file_name, std::ios::binary);
    if (!fin) {
        ZY_LOG("failed to open input file %s.\n", file_name.c_str());
        return ;
    }

    int bufferSize = 1024*1024*5;
    uint8_t *pData = new uint8_t [bufferSize];
    while(!fin.eof()){
        fin.read((char*)pData,bufferSize);
        auto len = fin.gcount();
        gMppDecoder264->decode(pData,len,len!=bufferSize);
    }
}


extern "C"
JNIEXPORT void JNICALL
Java_com_example_rk_1mpp_1test_MainActivity_testEncoder(JNIEnv *env, jobject thiz) {
    gMppEncoder264 = std::make_shared<MPP::MppEncoder264>(1920,1080,30,400000*2);
    std::string file_name = "/storage/emulated/0/Android/data/com.example.rk_mpp_test/cache/1920x1080.yuv";

    std::ifstream fin(file_name, std::ios::binary);
    if (!fin) {
        ZY_LOG("failed to open input file %s.\n", file_name.c_str());
        return ;
    }

    int bufferSize = 1920*1080*3/2;
    uint8_t *pData = new uint8_t [bufferSize];
    while(!fin.eof()){
        fin.read((char*)pData,bufferSize);
        auto len = fin.gcount();
        gMppEncoder264->encoder(pData);
    }
}