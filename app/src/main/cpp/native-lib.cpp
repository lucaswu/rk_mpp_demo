#include <jni.h>
#include <string>
#include <memory>
#include "mpp/MppDecoder264.h"
#include <fstream>
#include "utils/ZYLog.h"

std::shared_ptr<MPP::MppDecoder264> gMppDecoder264;

extern "C"
JNIEXPORT void JNICALL
Java_com_example_rk_1mpp_1test_MainActivity_testDecoder(JNIEnv *env, jobject thiz) {
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