//
// Created by lucas on 2023/7/24.
//

#include<stdarg.h>
#include<memory.h>
#include <stdio.h>
#include <string>
#include <assert.h>
#include "ZYLog.h"

#define LOG_TAG "lucas"

#define LOGD(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, fmt, ##args)
#define LOGI(fmt, args...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##args)
#define LOGW(fmt, args...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, fmt, ##args)
#define LOGE(fmt, args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##args)

void zhiyuan_printf(const char *file,const char *func,int line,const char *fmt,...)
{
#define BUF_LEN 500
    char buf[BUF_LEN];
    va_list args;
    memset(buf, 0, BUF_LEN);
    snprintf(buf, BUF_LEN, "[%s:%d]:", file, line);
    va_start(args, fmt);
    vsnprintf(buf+strlen(buf), BUF_LEN-strlen(buf), fmt, args);
    va_end(args);

    LOGD("%s\n",buf);

}

void zhiyuan_printf_assert(const char *file,const char *func,int line,const char *fmt,...)
{
    zhiyuan_printf(file,func,line,fmt);
    assert(0);
}
