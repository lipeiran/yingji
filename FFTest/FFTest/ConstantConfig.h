//
//  ConstantConfig.h
//  FFTest
//
//  Created by 李沛然 on 2019/7/19.
//  Copyright © 2019 李沛然. All rights reserved.
//

#ifndef ConstantConfig_h
#define ConstantConfig_h

extern "C"
{
#include <libavformat/avformat.h>
};

extern const int green[];   // 绿幕

extern const bool audioB;   // 是否需要声音
extern const bool maskB;    // 是否需要mask
extern bool debug;    // 是否debug模式

// 如果需要设置视频尺寸，只需要输入的第一张图片尺寸
extern float dst_screen_width; // 目标视频宽度
extern float dst_screen_height; // 目标视频高度
extern int src_totalCount; // 资源总数
extern int config_src_totalCount; // 配置资源总数
extern int src_index; // 初始化资源下标
extern const int src_init_slice; // 初始化资源数量，现在仅支持两个初始化frame
extern const int src_add_slice; // 增量资源数量，现在仅支持增量一个frame
extern int fps; // FPS，缺省 25

extern const enum AVPixelFormat enc_pix_fmt; // 目标视频文件的视频编码格式
extern const enum AVPixelFormat exe_pix_fmt; // 处理过程视频文件的视频编码格式


#endif /* ConstantConfig_h */
