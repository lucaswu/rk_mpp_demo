#pragma once
#include <stdint.h>
#include <string>

#include "../../utils/ZYLog.h"
class ZYGlobal{
public:
    static ZYGlobal& GetInstance(){
        static ZYGlobal gloabl;
        return gloabl;
    }

    void setAppDirectoru(std::string url){
        mAppDirectory = url;
    }

    std::string getAppDirectory(){
        return mAppDirectory;
    }
private:
    ZYGlobal(){}

    std::string mAppDirectory;
};


#define  USE_OPENCV4

