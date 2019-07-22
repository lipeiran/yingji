//
//  TPLog.cpp
//  FFTest
//
//  Created by 李沛然 on 2019/7/19.
//  Copyright © 2019 李沛然. All rights reserved.
//

#include "TPLog.hpp"
#include <string>
#include <sys/time.h>

struct timeval tv;

void Log::print_ms(const char *text = "")
{
    gettimeofday(&tv, NULL);
    printf("time is:%s:%ld\n",text,tv.tv_sec*1000 + tv.tv_usec/1000);
}

