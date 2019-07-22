//
//  ConstantConfig.cpp
//  FFTest
//
//  Created by 李沛然 on 2019/7/19.
//  Copyright © 2019 李沛然. All rights reserved.
//

#include <stdio.h>
#include "ConstantConfig.h"

const int green[4] = {0, 1, 20,255}; // 绿幕

const bool audioB = true;
const bool maskB = true;
bool debug = false;

// 如果需要设置视频尺寸，只需要输入的第一张图片尺寸
float dst_screen_width = 0; // 目标视频宽度
float dst_screen_height = 0; // 目标视频高度
int src_totalCount = 0; // 资源总数
int config_src_totalCount = 0; // 配置资源总数
int src_index = 0; // 初始化资源下标
const int src_init_slice = 2; // 初始化资源数量，现在仅支持两个初始化frame
const int src_add_slice = 1; // 增量资源数量，现在仅支持增量一个frame
int fps = 25; // FPS，缺省 25

const enum AVPixelFormat enc_pix_fmt = AV_PIX_FMT_YUV420P; // 目标视频文件的视频编码格式
const enum AVPixelFormat exe_pix_fmt = AV_PIX_FMT_RGBA;//AV_PIX_FMT_RGB24; // 处理过程视频文件的视频编码格式
