//
// Created by lenovo on 2024/7/1.
//
#pragma  once
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
class ZYTensor {
public:
    ZYTensor(size_t size){
        mSize = size;
        mPData = (void*)malloc(size);
    }

    ~ZYTensor()
    {
        if(mPData != nullptr){
            free(mPData);
            mPData = nullptr;
        }
        mSize =  0;
    }

    void* getPtr()
    {
        return mPData;
    }

    size_t  getSize()
    {
        return mSize;
    }

    bool copyData2Tensor(void*pData,size_t size){
        auto copySize = size > mSize?mSize:size;
        if(pData == nullptr || mPData == nullptr)
            return false;

        memcpy(mPData,pData,copySize);
        return true;
    }

    bool copyDataFromTensor(void*pData,size_t size){
        auto copySize = size > mSize?mSize:size;
        if(pData == nullptr || mPData == nullptr)
            return false;

        memcpy(pData,mPData,copySize);
        return true;
    }

    int mWidth;
    int mHeight;
    int mChannel;

private:
    void *mPData;
    size_t mSize;


};



