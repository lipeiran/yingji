//
//  DataTools.hpp
//  FFTest
//
//  Created by 李沛然 on 2019/7/19.
//  Copyright © 2019 李沛然. All rights reserved.
//

#ifndef DataTools_hpp
#define DataTools_hpp

#include <stdio.h>
#include <opencv2/opencv.hpp>
#include "ConstantConfig.h"
#include "AE_struct.h"
#include <iostream>
#include "Parse_AE.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <stdbool.h>
#include <float.h>
#include <string>
#include <stdlib.h>
#include "cJSON.h"
#include <unistd.h>
#include <libyuv.h>
};

class DataTools{
    
public:
    // 复制数据
    static uint8_t * copy_frame_data(uint8_t *src, int linesize, int height);
    
    // FFMPEG AVFrame to OPENCV Mat，此处默认数据都是pix_fmt:RGBA
    static cv::Mat frame_data_to_cvmat(int height, int width, uint8_t * src_data, int step);
    
    // YUV 转 RGBA
    static bool yuv_to_rgba_libyuv(uint8_t *p_yuv[], unsigned char* p_rgba[], int yuv_linesize[], int rgba_linesize[],int width,int height);
    
    // RGBA 转 YUV
    static bool rgba_to_yuv_libyuv(unsigned char* p_rgba, uint8_t *p_yuv[], int rgba_linesize[], int yuv_linesize[],int width,int height);

    // 双线性插值，origin_point_x，origin_point_y之所以用float，是为了后面双线性插值的精确运算，不然每一帧都不会很顺滑的连续
    static void bilinear_rgb_interpolation_float(float x_ratio, float y_ratio, float origin_point_x, float origin_point_y, int step_width, float *x_decimal, float *y_decimal, long *ori1, long * ori2, long *ori3, long *ori4);
    
    // 带有父子层级的图片的 缩放&位移等操作
    static void scale_rotate_drift_parent_action(int hor, int ver, float anchor_x,float anchor_y,float rotate_angle, float scale_ratio_x, float scale_ratio_y, float x_offset, float y_offset, float anchor_x_parent,float anchor_y_parent,float rotate_angle_parent, float scale_ratio_x_parent, float scale_ratio_y_parent, float x_offset_parent, float y_offset_parent, float *target_hor, float *target_ver);
    
    // 带有父级的旋转缩放位移，首先进行逆旋转父级，然后进行逆旋转子级，缩放&旋转&位移 RGB24，此处实现逻辑顺序：位移。缩放，旋转
    static void scale_rotate_drift_rgb_parent(float anchor_x,float anchor_y,float rotate_angle, float scale_ratio_x, float scale_ratio_y, float x_offset, float y_offset, uint8_t *cp_next_target_frame_rgb, int rgb_linesize,int asset_width, int asset_height, uint8_t *cp_out_target_frame_rgb,int out_rgb_linesize, int frame_width, int frame_height, float anchor_x_parent,float anchor_y_parent,float rotate_angle_parent, float scale_ratio_x_parent, float scale_ratio_y_parent, float x_offset_parent, float y_offset_parent);
    
    // 不带父子层级的图片的 缩放&位移等操作
    static void scale_rotate_drift_action(int hor, int ver, float anchor_x,float anchor_y,float rotate_angle, float scale_ratio_x, float scale_ratio_y, float x_offset, float y_offset, float *target_hor, float *target_ver);
    
    // 缩放&旋转&位移 RGB24，此处实现逻辑顺序：位移。缩放，旋转
    static void scale_rotate_drift_rgb(float anchor_x,float anchor_y,float rotate_angle, float scale_ratio_x, float scale_ratio_y, float x_offset, float y_offset, uint8_t *cp_next_target_frame_rgb, int rgb_linesize,int asset_width, int asset_height, uint8_t *cp_out_target_frame_rgb,int out_rgb_linesize, int frame_width, int frame_height);
    
    // 将素材原始帧转换为目标帧，用于各种效果处理
    static AVFrame *sws_origin_from_frame_to_sws_frame(AVFrame *frame, int stream_index, float target_width, float target_height, AVPixelFormat target_pix_fmt, float src_width, float src_height, AVPixelFormat src_pix_fmt);
    
    // 合并mask和原素材
    static uint8_t * combine_mask_data_rgba(int width, int height, int linesize, uint8_t *src_rgba_data, int mask_linesize, uint8_t *mask_rgba_data);
    
    // 转场结束以后的 cp_target_data_yuv 用于接下来静态的显示以及下一次转场
    static void reset_cp_target_data_rgb(int dst_frame_width, int dst_frame_height, int src_width, int src_height, int src_rgb_linesize, uint8_t *cp_src_frame_rgb,int target_rgb_linesize, uint8_t *cp_target_frame_rgb);
    
    // 高斯模糊 RGB24
    static void gaussian_blur_rgba(int blur_radius, float weight_ratio, uint8_t *cp_next_target_frame_rgba, int rgba_linesize, int frame_width, int frame_height);
    
    // 获取每一帧浮层集合的配置
    static void get_layers_config(int i, int all_layer_num, AEConfigEntity configEntity, float reset_next[], int &reset_next_num);
private:

};


#endif /* DataTools_hpp */






