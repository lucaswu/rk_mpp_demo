//
// Created by lenovo on 2024/6/28.
//

#pragma  once

#include <android/log.h>


void zhiyuan_printf(const char *file,const char *func,int line,const char *fmt,...);
void zhiyuan_printf_assert(const char *file,const char *func,int line,const char *fmt,...);

#define ZY_LOG(format,...) zhiyuan_printf(__FILE__,__func__,__LINE__,format,##__VA_ARGS__ )
#define ZY_LOG_ASSERT(format,...) zhiyuan_printf_assert(__FILE__,__func__,__LINE__,format,##__VA_ARGS__ )
