//
//  DataTools.cpp
//  FFTest
//
//  Created by 李沛然 on 2019/7/19.
//  Copyright © 2019 李沛然. All rights reserved.
//

#include "DataTools.hpp"

// FFMPEG AVFrame to OPENCV Mat，此处默认数据都是pix_fmt:RGBA
cv::Mat DataTools::frame_data_to_cvmat(int height, int width, uint8_t * src_data, int step)
{
    cv::Mat m;
    m = cv::Mat(height, width, CV_8UC4, src_data, step);
    return m;
}

// 复制 frame data
uint8_t * DataTools::copy_frame_data(uint8_t *src, int linesize, int height)
{
    uint8_t *dst = NULL;
    dst = (uint8_t *)malloc(sizeof(uint8_t) * linesize * height);
    memcpy(dst, src, linesize * height);
    return dst;
}

// YUV 转 RGBA
bool DataTools::yuv_to_rgba_libyuv(uint8_t *p_yuv[], unsigned char* p_rgba[], int yuv_linesize[], int rgba_linesize[],int width,int height)
{
    int ret = 0;
    if (width < 1 || height < 1 || p_yuv == NULL || p_rgba == NULL)
    {
        return false;
    }
    libyuv::I420ToABGR(p_yuv[0],
                       yuv_linesize[0],
                       p_yuv[1],
                       yuv_linesize[1],
                       p_yuv[2],
                       yuv_linesize[2],
                       p_rgba[0],
                       rgba_linesize[0],
                       width,
                       height);
    return ret;
}

// RGBA 转 YUV
bool DataTools::rgba_to_yuv_libyuv(unsigned char* p_rgba, uint8_t *p_yuv[], int rgba_linesize[], int yuv_linesize[],int width,int height)
{
    int ret = 0;
    yuv_linesize[0] = width;
    yuv_linesize[1] = width / 2;
    yuv_linesize[2] = width / 2;
    
    p_yuv[0] = (uint8_t *)malloc(sizeof(uint8_t) * yuv_linesize[0] * height);
    p_yuv[1] = (uint8_t *)malloc(sizeof(uint8_t) * yuv_linesize[1] * height / 2);
    p_yuv[2] = (uint8_t *)malloc(sizeof(uint8_t) * yuv_linesize[2] * height / 2);
    
    // ABGR little endian (rgba in memory) to I420.
    // 所以在这里应该是 ABGRTOI420 而不是 RGBATOI420
    libyuv::ABGRToI420(p_rgba,
                       rgba_linesize[0],
                       p_yuv[0],
                       yuv_linesize[0],
                       p_yuv[1],
                       yuv_linesize[1],
                       p_yuv[2],
                       yuv_linesize[2],
                       width,
                       height);
    
    return ret;
}

// 双线性插值，origin_point_x，origin_point_y之所以用float，是为了后面双线性插值的精确运算，不然每一帧都不会很顺滑的连续
void DataTools::bilinear_rgb_interpolation_float(float x_ratio, float y_ratio, float origin_point_x, float origin_point_y, int step_width, float *x_decimal, float *y_decimal, long *ori1, long * ori2, long *ori3, long *ori4)
{
    int rgb_step = 4;
    // 对于目标图像(x, y),实际映射到原图的坐标为(fx * x, fy * y),但是若为小数，原图并不存在该点，因此近似为(i, j)
    int x_int = x_ratio * origin_point_x;
    int y_int = y_ratio * origin_point_y;
    // 找到四个领域的下标
    *ori1 = y_int * step_width + x_int*rgb_step;
    *ori2 = y_int * step_width + (x_int + 1)*rgb_step;
    *ori3 = (y_int + 1) * step_width + x_int*rgb_step;
    *ori4 = (y_int + 1) * step_width + (x_int + 1)*rgb_step;
    
    *x_decimal = origin_point_x * 1.0 * x_ratio - x_int;
    *y_decimal = origin_point_y * 1.0 * y_ratio - y_int;
}

// 带有父子层级的图片的 缩放&位移等操作
void DataTools::scale_rotate_drift_parent_action(int hor, int ver, float anchor_x,float anchor_y,float rotate_angle, float scale_ratio_x, float scale_ratio_y, float x_offset, float y_offset, float anchor_x_parent,float anchor_y_parent,float rotate_angle_parent, float scale_ratio_x_parent, float scale_ratio_y_parent, float x_offset_parent, float y_offset_parent, float *target_hor, float *target_ver)
{
    // 先逆向父层位移
    float drift_hor = hor-x_offset_parent;
    float drift_ver = ver-y_offset_parent;
    // 再逆向父层旋转
    float rotate_hor = 0;
    float rotate_ver = 0;
    if (rotate_angle_parent == 0)
    {
        rotate_hor = drift_hor;
        rotate_ver = drift_ver;
    }
    else
    {
        float tmp_r_x = drift_hor - anchor_x_parent;
        float tmp_r_y = drift_ver - anchor_y_parent;
        float tmp_r_xy_x = tmp_r_x * cos(rotate_angle_parent) + tmp_r_y * sin(rotate_angle_parent);
        float tmp_r_xy_y = tmp_r_y * cos(rotate_angle_parent) - tmp_r_x * sin(rotate_angle_parent);
        rotate_hor = tmp_r_xy_x + anchor_x_parent;
        rotate_ver = tmp_r_xy_y + anchor_y_parent;
    }
    // 再逆向父层缩放
    float scale_hor = 0;
    float scale_ver = 0;
    if (scale_ratio_x_parent == 1.0 && scale_ratio_y_parent == 1.0)
    {
        scale_hor = rotate_hor;
        scale_ver = rotate_ver;
    }
    else
    {
        float tmp_s_x_1 = rotate_hor - anchor_x_parent;
        float tmp_s_y_1 = rotate_ver - anchor_y_parent;
        float tmp_s_xy_x = tmp_s_x_1 / scale_ratio_x_parent;
        float tmp_s_xy_y = tmp_s_y_1 / scale_ratio_y_parent;
        scale_hor = tmp_s_xy_x + anchor_x_parent;
        scale_ver = tmp_s_xy_y + anchor_y_parent;
    }
    float target_hor_1 = scale_hor;
    float target_ver_1 = scale_ver;
    
    // 先逆向位移
    float drift_hor_1 = target_hor_1-x_offset;
    float drift_ver_1 = target_ver_1-y_offset;
    // 再逆向旋转
    float rotate_hor_1 = 0;
    float rotate_ver_1 = 0;
    if (rotate_angle == 0)
    {
        rotate_hor_1 = drift_hor_1;
        rotate_ver_1 = drift_ver_1;
    }
    else
    {
        float tmp_r_x_1 = drift_hor_1 - anchor_x;
        float tmp_r_y_1 = drift_ver_1 - anchor_y;
        float tmp_r_xy_x_1 = tmp_r_x_1 * cos(rotate_angle) + tmp_r_y_1 * sin(rotate_angle);
        float tmp_r_xy_y_1 = tmp_r_y_1 * cos(rotate_angle) - tmp_r_x_1 * sin(rotate_angle);
        rotate_hor_1 = tmp_r_xy_x_1 + anchor_x;
        rotate_ver_1 = tmp_r_xy_y_1 + anchor_y;
    }
    // 再逆向缩放
    float scale_hor_1 = 0;
    float scale_ver_1 = 0;
    if (scale_ratio_x == 1.0 && scale_ratio_y == 1.0)
    {
        scale_hor_1 = rotate_hor_1;
        scale_ver_1 = rotate_ver_1;
    }
    else
    {
        float tmp_s_x_1_1 = rotate_hor_1 - anchor_x;
        float tmp_s_y_1_1 = rotate_ver_1 - anchor_y;
        float tmp_s_xy_x_1 = tmp_s_x_1_1 / scale_ratio_x;
        float tmp_s_xy_y_1 = tmp_s_y_1_1 / scale_ratio_y;
        scale_hor_1 = tmp_s_xy_x_1 + anchor_x;
        scale_ver_1 = tmp_s_xy_y_1 + anchor_y;
    }
    *target_hor = scale_hor_1;
    *target_ver = scale_ver_1;
}

// 带有父级的旋转缩放位移，首先进行逆旋转父级，然后进行逆旋转子级，缩放&旋转&位移 RGB24，此处实现逻辑顺序：位移。缩放，旋转
void DataTools::scale_rotate_drift_rgb_parent(float anchor_x,float anchor_y,float rotate_angle, float scale_ratio_x, float scale_ratio_y, float x_offset, float y_offset, uint8_t *cp_next_target_frame_rgb, int rgb_linesize,int asset_width, int asset_height, uint8_t *cp_out_target_frame_rgb,int out_rgb_linesize, int frame_width, int frame_height, float anchor_x_parent,float anchor_y_parent,float rotate_angle_parent, float scale_ratio_x_parent, float scale_ratio_y_parent, float x_offset_parent, float y_offset_parent)
{
    float x_decimal = 0.0,y_decimal = 0.0;
    long ori1 = 0, ori2 = 0, ori3 = 0, ori4 = 0;
    int r_offset = 0;
    int g_offset = 1;
    int b_offset = 2;
    int a_offset = 3;
    int rgba_step = 4;
    long rotate_angle_I = rotate_angle * 1000000;
    rotate_angle = rotate_angle_I / 1000000.0;
    int ver;
    for ( ver = 0; ver < frame_height; ver++)
    {
        int hor;
        for ( hor = 0; hor < frame_width; hor++)
        {
            float target_hor = 0;
            float target_ver = 0;
            
            scale_rotate_drift_parent_action( hor, ver, anchor_x, anchor_y, rotate_angle, scale_ratio_x, scale_ratio_y, x_offset, y_offset, anchor_x_parent, anchor_y_parent, rotate_angle_parent, scale_ratio_x_parent, scale_ratio_y_parent, x_offset_parent, y_offset_parent, &target_hor, &target_ver);
            
            if (target_hor >= (asset_width-1) || target_hor < 0 || target_ver >= (asset_height - 1) || target_ver < 0)
            {
                cp_out_target_frame_rgb[ver*out_rgb_linesize+hor*rgba_step+r_offset] = green[0];
                cp_out_target_frame_rgb[ver*out_rgb_linesize+hor*rgba_step+g_offset] = green[1];
                cp_out_target_frame_rgb[ver*out_rgb_linesize+hor*rgba_step+b_offset] = green[2];
                cp_out_target_frame_rgb[ver*out_rgb_linesize+hor*rgba_step+a_offset] = green[3];
            }
            else // hor,ver 在屏幕区间范围内
            {
                // 此处依然采用双线性插值
                bilinear_rgb_interpolation_float(1.0, 1.0, target_hor, target_ver, rgb_linesize, &x_decimal, &y_decimal, &ori1, &ori2, &ori3, &ori4);
                cp_out_target_frame_rgb[ver*out_rgb_linesize+hor*rgba_step+r_offset] = (cp_next_target_frame_rgb[ori1+r_offset] * (1 - y_decimal) * (1 - x_decimal) + cp_next_target_frame_rgb[ori2+r_offset] * (1 - y_decimal) * x_decimal + cp_next_target_frame_rgb[ori3+r_offset] * y_decimal * (1 - x_decimal) + cp_next_target_frame_rgb[ori4+r_offset] * y_decimal * x_decimal);
                cp_out_target_frame_rgb[ver*out_rgb_linesize+hor*rgba_step+g_offset] = (cp_next_target_frame_rgb[ori1+g_offset] * (1 - y_decimal) * (1 - x_decimal) + cp_next_target_frame_rgb[ori2+g_offset] * (1 - y_decimal) * x_decimal + cp_next_target_frame_rgb[ori3+g_offset] * y_decimal * (1 - x_decimal) + cp_next_target_frame_rgb[ori4+g_offset] * y_decimal * x_decimal);
                cp_out_target_frame_rgb[ver*out_rgb_linesize+hor*rgba_step+b_offset] = (cp_next_target_frame_rgb[ori1+b_offset] * (1 - y_decimal) * (1 - x_decimal) + cp_next_target_frame_rgb[ori2+b_offset] * (1 - y_decimal) * x_decimal + cp_next_target_frame_rgb[ori3+b_offset] * y_decimal * (1 - x_decimal) + cp_next_target_frame_rgb[ori4+b_offset] * y_decimal * x_decimal);
                cp_out_target_frame_rgb[ver*out_rgb_linesize+hor*rgba_step+a_offset] = (cp_next_target_frame_rgb[ori1+a_offset] * (1 - y_decimal) * (1 - x_decimal) + cp_next_target_frame_rgb[ori2+a_offset] * (1 - y_decimal) * x_decimal + cp_next_target_frame_rgb[ori3+a_offset] * y_decimal * (1 - x_decimal) + cp_next_target_frame_rgb[ori4+a_offset] * y_decimal * x_decimal);
            }
        }
    }
}

void DataTools::scale_rotate_drift_action(int hor, int ver, float anchor_x,float anchor_y,float rotate_angle, float scale_ratio_x, float scale_ratio_y, float x_offset, float y_offset, float *target_hor, float *target_ver)
{
    // 先逆向位移
    float drift_hor = hor-x_offset;
    float drift_ver = ver-y_offset;
    // 再逆向旋转
    float rotate_hor = 0;
    float rotate_ver = 0;
    if (rotate_angle == 0)
    {
        rotate_hor = drift_hor;
        rotate_ver = drift_ver;
    }
    else
    {
        float tmp_r_x = drift_hor - anchor_x;
        float tmp_r_y = drift_ver - anchor_y;
        float tmp_r_xy_x = tmp_r_x * cos(rotate_angle) + tmp_r_y * sin(rotate_angle);
        float tmp_r_xy_y = tmp_r_y * cos(rotate_angle) - tmp_r_x * sin(rotate_angle);
        rotate_hor = tmp_r_xy_x + anchor_x;
        rotate_ver = tmp_r_xy_y + anchor_y;
    }
    // 再逆向缩放
    float scale_hor = 0;
    float scale_ver = 0;
    if (scale_ratio_x == 1.0 && scale_ratio_y == 1.0)
    {
        scale_hor = rotate_hor;
        scale_ver = rotate_ver;
    }
    else
    {
        float tmp_s_x_1 = rotate_hor - anchor_x;
        float tmp_s_y_1 = rotate_ver - anchor_y;
        float tmp_s_xy_x = tmp_s_x_1 / scale_ratio_x;
        float tmp_s_xy_y = tmp_s_y_1 / scale_ratio_y;
        scale_hor = tmp_s_xy_x + anchor_x;
        scale_ver = tmp_s_xy_y + anchor_y;
    }
    *target_hor = scale_hor;
    *target_ver = scale_ver;
}

// 缩放&旋转&位移 RGB24，此处实现逻辑顺序：位移。缩放，旋转
void DataTools::scale_rotate_drift_rgb(float anchor_x,float anchor_y,float rotate_angle, float scale_ratio_x, float scale_ratio_y, float x_offset, float y_offset, uint8_t *cp_next_target_frame_rgb, int rgb_linesize,int asset_width, int asset_height, uint8_t *cp_out_target_frame_rgb,int out_rgb_linesize, int frame_width, int frame_height)
{
    float x_decimal = 0.0,y_decimal = 0.0;
    long ori1 = 0, ori2 = 0, ori3 = 0, ori4 = 0;
    int r_offset = 0;
    int g_offset = 1;
    int b_offset = 2;
    int a_offset = 3;
    int rgba_step = 4;
    long rotate_angle_I = rotate_angle * 1000000;
    rotate_angle = rotate_angle_I / 1000000.0;
    int ver;
    for ( ver = 0; ver < frame_height; ver++)
    {
        int hor;
        for ( hor = 0; hor < frame_width; hor++)
        {
            float target_hor = 0;
            float target_ver = 0;
            scale_rotate_drift_action(hor, ver, anchor_x, anchor_y, rotate_angle, scale_ratio_x, scale_ratio_y, x_offset, y_offset, &target_hor, &target_ver);
            if (target_hor >= (asset_width-1) || target_hor < 0 || target_ver >= (asset_height - 1) || target_ver < 0)
            {
                cp_out_target_frame_rgb[ver*out_rgb_linesize+hor*rgba_step+r_offset] = green[0];
                cp_out_target_frame_rgb[ver*out_rgb_linesize+hor*rgba_step+g_offset] = green[1];
                cp_out_target_frame_rgb[ver*out_rgb_linesize+hor*rgba_step+b_offset] = green[2];
                cp_out_target_frame_rgb[ver*out_rgb_linesize+hor*rgba_step+a_offset] = green[3];
            }
            else // hor,ver 在屏幕区间范围内
            {
                // 此处依然采用双线性插值
                bilinear_rgb_interpolation_float(1.0, 1.0, target_hor, target_ver, rgb_linesize, &x_decimal, &y_decimal, &ori1, &ori2, &ori3, &ori4);
                cp_out_target_frame_rgb[ver*out_rgb_linesize+hor*rgba_step+r_offset] = (cp_next_target_frame_rgb[ori1+r_offset] * (1 - y_decimal) * (1 - x_decimal) + cp_next_target_frame_rgb[ori2+r_offset] * (1 - y_decimal) * x_decimal + cp_next_target_frame_rgb[ori3+r_offset] * y_decimal * (1 - x_decimal) + cp_next_target_frame_rgb[ori4+r_offset] * y_decimal * x_decimal);
                cp_out_target_frame_rgb[ver*out_rgb_linesize+hor*rgba_step+g_offset] = (cp_next_target_frame_rgb[ori1+g_offset] * (1 - y_decimal) * (1 - x_decimal) + cp_next_target_frame_rgb[ori2+g_offset] * (1 - y_decimal) * x_decimal + cp_next_target_frame_rgb[ori3+g_offset] * y_decimal * (1 - x_decimal) + cp_next_target_frame_rgb[ori4+g_offset] * y_decimal * x_decimal);
                cp_out_target_frame_rgb[ver*out_rgb_linesize+hor*rgba_step+b_offset] = (cp_next_target_frame_rgb[ori1+b_offset] * (1 - y_decimal) * (1 - x_decimal) + cp_next_target_frame_rgb[ori2+b_offset] * (1 - y_decimal) * x_decimal + cp_next_target_frame_rgb[ori3+b_offset] * y_decimal * (1 - x_decimal) + cp_next_target_frame_rgb[ori4+b_offset] * y_decimal * x_decimal);
                cp_out_target_frame_rgb[ver*out_rgb_linesize+hor*rgba_step+a_offset] = (cp_next_target_frame_rgb[ori1+a_offset] * (1 - y_decimal) * (1 - x_decimal) + cp_next_target_frame_rgb[ori2+a_offset] * (1 - y_decimal) * x_decimal + cp_next_target_frame_rgb[ori3+a_offset] * y_decimal * (1 - x_decimal) + cp_next_target_frame_rgb[ori4+a_offset] * y_decimal * x_decimal);
            }
        }
    }
}

// 将素材原始帧转换为目标帧，用于各种效果处理
AVFrame * DataTools::sws_origin_from_frame_to_sws_frame(AVFrame *frame, int stream_index, float target_width, float target_height, AVPixelFormat target_pix_fmt, float src_width, float src_height, AVPixelFormat src_pix_fmt)
{
    AVFrame *src_frame_yuv = av_frame_alloc();
    unsigned char *out_buffer = (unsigned char *)av_malloc(av_image_get_buffer_size(exe_pix_fmt, target_width, target_height, 1));
    av_image_fill_arrays(src_frame_yuv->data, src_frame_yuv->linesize, out_buffer, exe_pix_fmt, target_width, target_height, 1);
    struct SwsContext *img_convert_ctx = sws_getContext(src_width, src_height, src_pix_fmt,target_width, target_height, target_pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);
    sws_scale(img_convert_ctx, (const unsigned char* const*)frame->data, frame->linesize, 0, src_height,
              src_frame_yuv->data, src_frame_yuv->linesize);
    src_frame_yuv->width = target_width;
    src_frame_yuv->height = target_height;
    src_frame_yuv->format = exe_pix_fmt;
    sws_freeContext(img_convert_ctx);
    return src_frame_yuv;
}

// 合并mask和原素材
uint8_t * DataTools::combine_mask_data_rgba(int width, int height, int linesize, uint8_t *src_rgba_data, int mask_linesize, uint8_t *mask_rgba_data)
{
    uint8_t *dst_rgba_data = NULL;
    dst_rgba_data = (uint8_t *)malloc(sizeof(uint8_t) * linesize * height);
    dst_rgba_data[0] = '\0';
    int i = 0;
    int j = 0;
    for (i = 0; i < height; i++)
    {
        for (j = 0; j < width; j++)
        {
            int tmp_y = mask_rgba_data[i * mask_linesize + j];
            float tmp_ratio = tmp_y * 1.0 / 255;
            int tmpResult_r = src_rgba_data[i * linesize + j * 4 + 0] * tmp_ratio;
            int tmpResult_g = src_rgba_data[i * linesize + j * 4 + 1] * tmp_ratio;
            int tmpResult_b = src_rgba_data[i * linesize + j * 4 + 2] * tmp_ratio;
            
            if (tmp_y != 0)
            {
                dst_rgba_data[i * linesize + j * 4 + 0] = tmpResult_r * tmp_ratio;
                dst_rgba_data[i * linesize + j * 4 + 1] = tmpResult_g * tmp_ratio;
                dst_rgba_data[i * linesize + j * 4 + 2] = tmpResult_b * tmp_ratio;
                dst_rgba_data[i * linesize + j * 4 + 3] = 255;
            }
            else
            {
                dst_rgba_data[i * linesize + j * 4 + 0] = green[0];
                dst_rgba_data[i * linesize + j * 4 + 1] = green[1];
                dst_rgba_data[i * linesize + j * 4 + 2] = green[2];
                dst_rgba_data[i * linesize + j * 4 + 3] = 0;
            }
        }
    }
    return dst_rgba_data;
}

// 转场结束以后的 cp_target_data_yuv 用于接下来静态的显示以及下一次转场
void DataTools::reset_cp_target_data_rgb(int dst_frame_width, int dst_frame_height, int src_width, int src_height, int src_rgb_linesize, uint8_t *cp_src_frame_rgb,int target_rgb_linesize, uint8_t *cp_target_frame_rgb)
{
    int ver;
    int r_offset = 0;
    int g_offset = 1;
    int b_offset = 2;
    int a_offset = 3;
    int rgb_step = 4;
    for ( ver = 0; ver < dst_frame_height; ver++)
    {
        int hor;
        for ( hor = 0; hor < dst_frame_width; hor++)
        {
            // 先判断 hor & ver 是否在界内（两张图中心相叠）
            if (hor < src_width && ver < src_height)
            {
                cp_target_frame_rgb[ver*target_rgb_linesize+hor * rgb_step + r_offset] = cp_src_frame_rgb[ver * src_rgb_linesize+hor * rgb_step + r_offset];
                cp_target_frame_rgb[ver*target_rgb_linesize+hor * rgb_step + g_offset] = cp_src_frame_rgb[ver * src_rgb_linesize+hor * rgb_step + g_offset];
                cp_target_frame_rgb[ver*target_rgb_linesize+hor * rgb_step + b_offset] = cp_src_frame_rgb[ver * src_rgb_linesize+hor * rgb_step + b_offset];
                cp_target_frame_rgb[ver*target_rgb_linesize+hor * rgb_step + a_offset] = cp_src_frame_rgb[ver * src_rgb_linesize+hor * rgb_step + a_offset];
            }
            else // 不在界内
            {
                cp_target_frame_rgb[ver*target_rgb_linesize+hor * rgb_step + r_offset] = green[0];
                cp_target_frame_rgb[ver*target_rgb_linesize+hor * rgb_step + g_offset] = green[1];
                cp_target_frame_rgb[ver*target_rgb_linesize+hor * rgb_step + b_offset] = green[2];
                cp_target_frame_rgb[ver*target_rgb_linesize+hor * rgb_step + a_offset] = green[3];
            }
        }
    }
}

// 高斯模糊 RGB24
void DataTools::gaussian_blur_rgba(int blur_radius, float weight_ratio, uint8_t *cp_next_target_frame_rgba, int rgba_linesize, int frame_width, int frame_height)
{
    uint8_t *cp_pre_target_frame_rgba = NULL;
    cp_pre_target_frame_rgba = (unsigned char *)malloc(sizeof(uint8_t) * rgba_linesize * frame_height);
    // 高斯模糊半径
    if (blur_radius < 1)
    {
        blur_radius = 1; // 不可为0，只能为整数
    }
    float tmp_ratio_f = pow(1.2,pow(blur_radius-1.0,0.3));
    
    // 高斯模糊次数
    int blur_time = 1;
    // sigma 正态分布单位
    float sigma = 2; // 不可为 0
    // 高斯模糊 step
    int step_size = blur_radius/4+1.0; // 可为0，只能为整数
    if (step_size > 4)
    {
        step_size = 4;
    }
    
    float *standardGaussianWeights = (float *)calloc(blur_radius, sizeof(float));
    float sumOfWeights = 0.0;
    int currentGaussianWeightIndex;
    for ( currentGaussianWeightIndex = 0; currentGaussianWeightIndex < blur_radius; currentGaussianWeightIndex++)
    {
        standardGaussianWeights[currentGaussianWeightIndex] = (1.0 / sqrt(2.0 * M_PI * pow(sigma, 2.0))) * exp(-pow(currentGaussianWeightIndex, 2.0) / (2.0 * pow(sigma, 2.0)));
        
        if (currentGaussianWeightIndex == 0)
        {
            sumOfWeights += standardGaussianWeights[currentGaussianWeightIndex];
        }
        else
        {
            sumOfWeights += 2.0 * standardGaussianWeights[currentGaussianWeightIndex];
        }
    }
    // 让standardGaussianWeights里面的总和相加为1
    for ( currentGaussianWeightIndex = 0; currentGaussianWeightIndex < blur_radius; currentGaussianWeightIndex++)
    {
        standardGaussianWeights[currentGaussianWeightIndex] = standardGaussianWeights[currentGaussianWeightIndex]/sumOfWeights*0.9;
    }
    if (blur_radius == 1)
    {
        standardGaussianWeights[0] += (1-standardGaussianWeights[0])*weight_ratio;
    }
    
    int r_offset = 0;
    int g_offset = 1;
    int b_offset = 2;
    int a_offset = 3;
    int rgba_step = 4;
    int blur_t;
    for ( blur_t = 0; blur_t < blur_time; blur_t++)
    {
        int i;
        for ( i = 0; i < 2; i++)
        {
            memcpy(cp_pre_target_frame_rgba, cp_next_target_frame_rgba, rgba_linesize * frame_height);
            int ver;
            for ( ver = 0; ver < frame_height; ver++)
            {
                int hor;
                for ( hor = 0; hor < frame_width; hor++)
                {
                    if (i == 0) // 水平相加，该点左右各blur_radius
                    {
                        // RGB 横向正态分布
                        int tmp_R = 0;
                        int tmp_G = 0;
                        int tmp_B = 0;
                        int tmp_A = 255;
                        tmp_R = cp_pre_target_frame_rgba[ver*rgba_linesize+hor*rgba_step+r_offset] * standardGaussianWeights[0];
                        tmp_G = cp_pre_target_frame_rgba[ver*rgba_linesize+hor*rgba_step+g_offset] * standardGaussianWeights[0];
                        tmp_B = cp_pre_target_frame_rgba[ver*rgba_linesize+hor*rgba_step+b_offset] * standardGaussianWeights[0];
                        int calcu_blur_t;
                        for ( calcu_blur_t = 1; calcu_blur_t < blur_radius; calcu_blur_t++)
                        {
                            int tmp_add_index = hor + calcu_blur_t * step_size;
                            if (tmp_add_index < frame_width)
                            {
                                tmp_R += cp_pre_target_frame_rgba[ver*rgba_linesize+tmp_add_index*rgba_step+r_offset] * standardGaussianWeights[calcu_blur_t];
                                tmp_G += cp_pre_target_frame_rgba[ver*rgba_linesize+tmp_add_index*rgba_step+g_offset] * standardGaussianWeights[calcu_blur_t];
                                tmp_B += cp_pre_target_frame_rgba[ver*rgba_linesize+tmp_add_index*rgba_step+b_offset] * standardGaussianWeights[calcu_blur_t];
                            }
                            else
                            {
                                tmp_R += cp_pre_target_frame_rgba[ver*rgba_linesize+(frame_width-1)*rgba_step+r_offset] * standardGaussianWeights[calcu_blur_t];
                                tmp_G += cp_pre_target_frame_rgba[ver*rgba_linesize+(frame_width-1)*rgba_step+g_offset] * standardGaussianWeights[calcu_blur_t];
                                tmp_B += cp_pre_target_frame_rgba[ver*rgba_linesize+(frame_width-1)*rgba_step+b_offset] * standardGaussianWeights[calcu_blur_t];
                            }
                            int tmp_sub_index = hor - calcu_blur_t * step_size;
                            if (tmp_sub_index >= 0)
                            {
                                tmp_R += cp_pre_target_frame_rgba[ver*rgba_linesize+tmp_sub_index*rgba_step+r_offset] * standardGaussianWeights[calcu_blur_t];
                                tmp_G += cp_pre_target_frame_rgba[ver*rgba_linesize+tmp_sub_index*rgba_step+g_offset] * standardGaussianWeights[calcu_blur_t];
                                tmp_B += cp_pre_target_frame_rgba[ver*rgba_linesize+tmp_sub_index*rgba_step+b_offset] * standardGaussianWeights[calcu_blur_t];
                            }
                            else
                            {
                                tmp_R += cp_pre_target_frame_rgba[ver*rgba_linesize] * standardGaussianWeights[calcu_blur_t];
                                tmp_G += cp_pre_target_frame_rgba[ver*rgba_linesize] * standardGaussianWeights[calcu_blur_t];
                                tmp_B += cp_pre_target_frame_rgba[ver*rgba_linesize] * standardGaussianWeights[calcu_blur_t];
                            }
                        }
                        cp_next_target_frame_rgba[ver*rgba_linesize+hor*rgba_step+r_offset] = tmp_R;
                        cp_next_target_frame_rgba[ver*rgba_linesize+hor*rgba_step+g_offset] = tmp_G;
                        cp_next_target_frame_rgba[ver*rgba_linesize+hor*rgba_step+b_offset] = tmp_B;
                        cp_next_target_frame_rgba[ver*rgba_linesize+hor*rgba_step+a_offset] = tmp_A;
                    }
                    else // 竖直相加，该点上下相加各blur_radius
                    {
                        // RGB 纵向正态分布
                        int tmp_R = 0;
                        int tmp_G = 0;
                        int tmp_B = 0;
                        int tmp_A = 255;
                        tmp_R = cp_pre_target_frame_rgba[ver*rgba_linesize+hor*rgba_step+r_offset] * standardGaussianWeights[0];
                        tmp_G = cp_pre_target_frame_rgba[ver*rgba_linesize+hor*rgba_step+g_offset] * standardGaussianWeights[0];
                        tmp_B = cp_pre_target_frame_rgba[ver*rgba_linesize+hor*rgba_step+b_offset] * standardGaussianWeights[0];
                        int calcu_blur_t;
                        for ( calcu_blur_t = 1; calcu_blur_t < blur_radius; calcu_blur_t++)
                        {
                            int tmp_add_index = ver + calcu_blur_t * step_size;
                            if (tmp_add_index < frame_height)
                            {
                                tmp_R += cp_pre_target_frame_rgba[tmp_add_index*rgba_linesize+hor*rgba_step+r_offset] * standardGaussianWeights[calcu_blur_t];
                                tmp_G += cp_pre_target_frame_rgba[tmp_add_index*rgba_linesize+hor*rgba_step+g_offset] * standardGaussianWeights[calcu_blur_t];
                                tmp_B += cp_pre_target_frame_rgba[tmp_add_index*rgba_linesize+hor*rgba_step+b_offset] * standardGaussianWeights[calcu_blur_t];
                            }
                            else
                            {
                                tmp_R += cp_pre_target_frame_rgba[(frame_height-1)*rgba_linesize+hor*rgba_step+r_offset] * standardGaussianWeights[calcu_blur_t];
                                tmp_G += cp_pre_target_frame_rgba[(frame_height-1)*rgba_linesize+hor*rgba_step+g_offset] * standardGaussianWeights[calcu_blur_t];
                                tmp_B += cp_pre_target_frame_rgba[(frame_height-1)*rgba_linesize+hor*rgba_step+b_offset] * standardGaussianWeights[calcu_blur_t];
                            }
                            int tmp_sub_index = ver - calcu_blur_t * step_size;
                            if (tmp_sub_index >= 0)
                            {
                                tmp_R += cp_pre_target_frame_rgba[tmp_sub_index*rgba_linesize+hor*rgba_step+r_offset] * standardGaussianWeights[calcu_blur_t];
                                tmp_G += cp_pre_target_frame_rgba[tmp_sub_index*rgba_linesize+hor*rgba_step+g_offset] * standardGaussianWeights[calcu_blur_t];
                                tmp_B += cp_pre_target_frame_rgba[tmp_sub_index*rgba_linesize+hor*rgba_step+b_offset] * standardGaussianWeights[calcu_blur_t];
                            }
                            else
                            {
                                tmp_R += cp_pre_target_frame_rgba[hor*rgba_step+r_offset] * standardGaussianWeights[calcu_blur_t];
                                tmp_G += cp_pre_target_frame_rgba[hor*rgba_step+g_offset] * standardGaussianWeights[calcu_blur_t];
                                tmp_B += cp_pre_target_frame_rgba[hor*rgba_step+b_offset] * standardGaussianWeights[calcu_blur_t];
                            }
                        }
                        
                        tmp_R *= tmp_ratio_f;
                        tmp_G *= tmp_ratio_f;
                        tmp_B *= tmp_ratio_f;
                        
                        if (tmp_R > 255)
                        {
                            tmp_R = 255;
                        }
                        if (tmp_G > 255)
                        {
                            tmp_G = 255;
                        }
                        if (tmp_B > 255)
                        {
                            tmp_B = 255;
                        }
                        cp_next_target_frame_rgba[ver*rgba_linesize+hor*rgba_step+r_offset] = tmp_R;
                        cp_next_target_frame_rgba[ver*rgba_linesize+hor*rgba_step+g_offset] = tmp_G;
                        cp_next_target_frame_rgba[ver*rgba_linesize+hor*rgba_step+b_offset] = tmp_B;
                        cp_next_target_frame_rgba[ver*rgba_linesize+hor*rgba_step+a_offset] = tmp_A;
                    }
                }
            }
        }
    }
    free(cp_pre_target_frame_rgba);
    cp_pre_target_frame_rgba = NULL;
}

void DataTools::get_layers_config(int i, int all_layer_num, AEConfigEntity configEntity, float reset_next[], int &reset_next_num)
{
    ParseAE parseAE;
    int j = 0;
    // 获取当前帧的活动层数据
    for (; j < all_layer_num; j++)
    {
        AELayerEntity tmpLayerEntity = configEntity.layers[j];
        if (i >= tmpLayerEntity.ip && i <= tmpLayerEntity.op)
        {
            float angle_f = 0;
            float scale_f_x = 1.0;
            float scale_f_y = 1.0;
            float x_f = 0;
            float y_f = 0;
            float anchor_x_f = 0;
            float anchor_y_f = 0;
            float alpha_f = 1.0;
            int blur_radius = 0.0;
            int parent_ind = tmpLayerEntity.parent;
            // 父层 params
            float parent_angle_f = 0;
            float parent_scale_f_x = 1.0;
            float parent_scale_f_y = 1.0;
            float parent_x_f = 0;
            float parent_y_f = 0;
            float parent_anchor_x_f = 0;
            float parent_anchor_y_f = 0;
            float parent_alpha_f = 1.0;
            int parent_blur_radius = 0.0;
            
            // 获取该浮层某一帧的属性
            parseAE.get_ae_params(i, tmpLayerEntity, &angle_f, &scale_f_x, &scale_f_y, &x_f, &y_f, &anchor_x_f, &anchor_y_f, &alpha_f, &blur_radius);
            
            if (alpha_f == 0)
            {
                continue;
            }
            int square_index = 0;
            bool break_b = true;
            bool continue_b = false;
            float *tmp_s_x = (float *)malloc(sizeof(float)*4);
            memset(tmp_s_x, 0, 4);
            float *tmp_s_y = (float *)malloc(sizeof(float)*4);
            memset(tmp_s_y, 0, 4);
            
            if (parent_ind > -1)
            {
                int parent_ind_index = parseAE.layer_index_ind(parent_ind, configEntity);
                AELayerEntity tmpLayerEntity_parent = configEntity.layers[parent_ind_index];
                parseAE.get_ae_params(i, tmpLayerEntity_parent, &parent_angle_f, &parent_scale_f_x, &parent_scale_f_y, &parent_x_f, &parent_y_f, &parent_anchor_x_f, &parent_anchor_y_f, &parent_alpha_f, &parent_blur_radius);
            }
            
            // 通过以上数据来实现动画以后的效果
            for ( square_index = 0; square_index < 4; square_index++)
            {
                int hor = 0, ver = 0;
                if (square_index == 0)
                {
                    hor = 0;
                    ver = 0;
                }
                else if (square_index == 1)
                {
                    hor = dst_screen_width;
                    ver = 0;
                }
                else if (square_index == 2)
                {
                    hor = 0;
                    ver = dst_screen_height;
                }
                else if (square_index == 3)
                {
                    hor = dst_screen_width;
                    ver = dst_screen_height;
                }
                
                float tmp_target_hor = 0;
                float tmp_target_ver = 0;
                
                if (parent_ind == -1)
                {
                    scale_rotate_drift_action( hor, ver, anchor_x_f, anchor_y_f, angle_f, scale_f_x, scale_f_y, x_f, y_f, &tmp_target_hor, &tmp_target_ver);
                }
                else
                {
                    int parent_ind_index = parseAE.layer_index_ind(parent_ind, configEntity);
                    AELayerEntity tmpLayerEntity_parent = configEntity.layers[parent_ind_index];
                    parseAE.get_ae_params( i, tmpLayerEntity_parent, &parent_angle_f, &parent_scale_f_x, &parent_scale_f_y, &parent_x_f, &parent_y_f, &parent_anchor_x_f, &parent_anchor_y_f, &parent_alpha_f, &parent_blur_radius);
                    scale_rotate_drift_parent_action( hor, ver, anchor_x_f, anchor_y_f, angle_f, scale_f_x, scale_f_y, x_f, y_f,  parent_anchor_x_f, parent_anchor_y_f, parent_angle_f, parent_scale_f_x, parent_scale_f_y, parent_x_f, parent_y_f, &tmp_target_hor, &tmp_target_ver);
                }
                
                tmp_s_x[square_index] = tmp_target_hor;
                tmp_s_y[square_index] = tmp_target_ver;
                
                if (tmp_target_hor >= (dst_screen_width-1) || tmp_target_hor <= 0 || tmp_target_ver >= (dst_screen_height - 1) || tmp_target_ver <= 0 || alpha_f < 1.0)
                {
                    break_b = false;
                }
            }
            // 边界外 60 像素之外不绘制
            int delta = 100;
            float left_delta = 0-delta;
            float up_delta = 0-delta;
            float right_delta = dst_screen_width-1+delta;
            float down_delta = dst_screen_height-1+delta;
            if ((tmp_s_x[0] <= left_delta && tmp_s_x[1] <=left_delta  && tmp_s_x[2] <=left_delta  && tmp_s_x[3] <=left_delta) || (tmp_s_x[0] >= (dst_screen_width-1) && tmp_s_x[1] >= right_delta  && tmp_s_x[2] >= right_delta && tmp_s_x[3] >= right_delta) || (tmp_s_y[0] <= up_delta && tmp_s_y[1] <= up_delta  && tmp_s_y[2] <= up_delta  && tmp_s_y[3] <= up_delta) || (tmp_s_y[0] >= down_delta && tmp_s_y[1] >= down_delta  && tmp_s_y[2] >= down_delta  && tmp_s_y[3] >= down_delta))
            {
                continue_b = true;
            }
            free(tmp_s_x);
            free(tmp_s_y);
            if (break_b || !continue_b)
            {
                reset_next[reset_next_num+0] = angle_f;
                reset_next[reset_next_num+1] = scale_f_x;
                reset_next[reset_next_num+2] = x_f;
                reset_next[reset_next_num+3] = y_f;
                reset_next[reset_next_num+4] = anchor_x_f;
                reset_next[reset_next_num+5] = anchor_y_f;
                reset_next[reset_next_num+6] = alpha_f;
                reset_next[reset_next_num+7] = tmpLayerEntity.ind;
                reset_next[reset_next_num+8] = blur_radius;
                reset_next[reset_next_num+9] = tmpLayerEntity.hasMask?(tmpLayerEntity.ind):-1;
                reset_next[reset_next_num+10] = scale_f_y;
                reset_next[reset_next_num+11] = parent_ind;
                // parent params
                reset_next[reset_next_num+12] = parent_angle_f;
                reset_next[reset_next_num+13] = parent_scale_f_x;
                reset_next[reset_next_num+14] = parent_scale_f_y;
                reset_next[reset_next_num+15] = parent_x_f;
                reset_next[reset_next_num+16] = parent_y_f;
                reset_next[reset_next_num+17] = parent_alpha_f;
                reset_next_num += 18;
                if (break_b)
                {
                    break;
                }
            }
            else if (continue_b)
            {
                continue;
            }
        }
    }
}


