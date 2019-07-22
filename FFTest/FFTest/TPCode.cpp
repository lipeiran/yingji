//
//  Test_CPlus.cpp
//  FFTest
//
//  Created by 李沛然 on 2019/7/11.
//  Copyright © 2019 李沛然. All rights reserved.
//

#include <stdio.h>
#include "Parse_AE.h"
#include "BasicTools.hpp"
#include "DataTools.hpp"
#include "TPLog.hpp"
#include "TPError.hpp"

using namespace std;
using namespace cv;

#ifdef  __cplusplus // 此处是ffmpeg 一处错误，c实现转c++实现，error log的时候报错

static const std::string av_make_error_string(int errnum)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
    return (std::string)errbuf;
}

#undef av_err2str
#define av_err2str(errnum) av_make_error_string(errnum).c_str()

#endif // __cplusplus

AEConfigEntity configEntity;


// 视频转码（主要用于解码花瓣视频）
struct SwsContext *img_filter_convert_ctx;
unsigned char *out_filter_buffer = NULL;
AVFrame *src_filter_frame_yuv = NULL;

// 视频转码（主要用于解码花瓣mask视频）
struct SwsContext *img_filter_mask_convert_ctx;
unsigned char *out_filter_mask_buffer = NULL;
AVFrame *src_filter_mask_frame_yuv = NULL;
AVFrame *src_filter_mask_frame_rgba = NULL;

// 图片素材 context
static AVFormatContext *ifmt_ctx;
static AVFormatContext *ifmt_two_ctx;

// 花瓣素材 context
static AVFormatContext *ifmt_filter_ctx; // 原视频
static AVFormatContext *ifmt_filter_two_ctx; // alpha 视频

// 输出源 context
static AVFormatContext *ofmt_ctx;

// 图片流 Context
typedef struct StreamContext
{
    AVCodecContext *dec_ctx;
    AVCodecContext *enc_ctx;
} StreamContext;
static StreamContext *src_stream_ctx;

// 花瓣视频流 Context
typedef struct StreamFilterContext
{
    AVCodecContext *filter_dec_ctx;
    AVCodecContext *filter_enc_ctx;
} StreamFilterContext;
static StreamFilterContext *src_filter_stream_ctx;

// 花瓣视频音频流 Context
typedef struct StreamAudioFilterContext
{
    AVCodecContext *audio_filter_dec_ctx;
    AVCodecContext *audio_filter_enc_ctx;
} StreamAudioFilterContext;
static StreamAudioFilterContext *src_audio_filter_stream_ctx;

#pragma mark ------------ 打开资源 ------------

// 打开图片输入文件资源
static int open_input_file(const char *filename, int index)
{
    int ret;
    if (!src_stream_ctx)
    {
        src_stream_ctx = (StreamContext *)av_mallocz_array(2, sizeof(*src_stream_ctx));
    }
    if (index == 0)
    {
        ifmt_ctx = NULL;
        ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL);
        ret = avformat_find_stream_info(ifmt_ctx, NULL);
        AVStream *stream = ifmt_ctx->streams[0];
        AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
        AVCodecContext *codec_ctx;
        codec_ctx = avcodec_alloc_context3(dec);
        ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
        codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
        ret = avcodec_open2(codec_ctx, dec, NULL);
        src_stream_ctx[0].dec_ctx = codec_ctx;
        if (debug)
        {
            av_dump_format(ifmt_ctx, 0, filename, 0);
        }
    }
    else
    {
        ifmt_two_ctx = NULL;
        ret = avformat_open_input(&ifmt_two_ctx, filename, NULL, NULL);
        ret = avformat_find_stream_info(ifmt_two_ctx, NULL);
        
        AVStream *stream = ifmt_two_ctx->streams[0];
        AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
        AVCodecContext *codec_ctx;
        codec_ctx = avcodec_alloc_context3(dec);
        ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
        codec_ctx->framerate = av_guess_frame_rate(ifmt_two_ctx, stream, NULL);
        ret = avcodec_open2(codec_ctx, dec, NULL);
        src_stream_ctx[1].dec_ctx = codec_ctx;
        if (debug)
        {
            av_dump_format(ifmt_two_ctx, 0, filename, 0);
        }
    }
    return 0;
}

// 打开花瓣视频输入文件资源
static int open_filter_input_file(const char *filename, int index)
{
    int ret = 0;
    if (!src_filter_stream_ctx)
    {
        src_filter_stream_ctx = (StreamFilterContext *)av_mallocz_array(2, sizeof(*src_filter_stream_ctx));
        if (!src_filter_stream_ctx)
        {
            return AVERROR(ENOMEM);
        }
    }
    if (!src_audio_filter_stream_ctx)
    {
        src_audio_filter_stream_ctx = (StreamAudioFilterContext *)av_mallocz_array(2, sizeof(*src_audio_filter_stream_ctx));
        if (!src_audio_filter_stream_ctx)
        {
            return AVERROR(ENOMEM);
        }
    }
    
    // 暂定为 0 为 花瓣视频， 1 为 mask视频
    if (index == 0)
    {
        ifmt_filter_ctx = NULL;
        ret = avformat_open_input(&ifmt_filter_ctx, filename, NULL, NULL);
        ret = avformat_find_stream_info(ifmt_filter_ctx, NULL);
        
        int i;
        for ( i = 0; i < (audioB?ifmt_filter_ctx->nb_streams:1); ++i)
        {
            AVStream *stream = ifmt_filter_ctx->streams[i];
            AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
            AVCodecContext *codec_ctx;
            codec_ctx = avcodec_alloc_context3(dec);
            ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
            if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                codec_ctx->framerate = av_guess_frame_rate(ifmt_filter_ctx, stream, NULL);
            }
            ret = avcodec_open2(codec_ctx, dec, NULL);
            if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                src_filter_stream_ctx[0].filter_dec_ctx = codec_ctx;
            }
            else if (codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                src_audio_filter_stream_ctx[0].audio_filter_dec_ctx = codec_ctx;
            }
            if (debug)
            {
                av_dump_format(ifmt_filter_ctx, 0, filename, 0);
            }
        }
    }
    else
    {
        ifmt_filter_two_ctx = NULL;
        ret = avformat_open_input(&ifmt_filter_two_ctx, filename, NULL, NULL);
        ret = avformat_find_stream_info(ifmt_filter_two_ctx, NULL);
        int i;
        for ( i = 0; i < (audioB?ifmt_filter_two_ctx->nb_streams:1); ++i)
        {
            AVStream *stream = ifmt_filter_two_ctx->streams[i];
            AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
            AVCodecContext *codec_ctx;
            codec_ctx = avcodec_alloc_context3(dec);
            ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
            if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                codec_ctx->framerate = av_guess_frame_rate(ifmt_filter_two_ctx, stream, NULL);
            }
            ret = avcodec_open2(codec_ctx, dec, NULL);
            if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                src_filter_stream_ctx[1].filter_dec_ctx = codec_ctx;
            }
            else if (codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO)
            {
                src_audio_filter_stream_ctx[1].audio_filter_dec_ctx = codec_ctx;
            }
            if (debug)
            {
                av_dump_format(ifmt_filter_two_ctx, 0, filename, 0);
            }
        }
    }
    return ret;
}

// 打开输出文件流
static int open_dst_output_file(const char *filename)
{
    AVStream *out_stream;
    AVCodecContext *dec_ctx, *enc_ctx;
    AVCodec *encoder;
    int ret;
    ofmt_ctx = NULL;
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
    int i;
    for ( i = 0; i < (audioB?2:1); i++) // 花瓣视频流两个 stream 分解出两个流，分别写入目标视频
    {
        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (i == 0) // 视频流
        {
            encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
            enc_ctx = avcodec_alloc_context3(encoder);
            enc_ctx->height = dst_screen_height;
            enc_ctx->width = dst_screen_width;
            enc_ctx->pix_fmt = enc_pix_fmt;
            enc_ctx->time_base = (AVRational){ 1, fps};
            enc_ctx->sample_aspect_ratio = src_filter_stream_ctx[0].filter_dec_ctx->sample_aspect_ratio;
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            {
                enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }
            ret = avcodec_open2(enc_ctx, encoder, NULL);
            ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
            out_stream->time_base = enc_ctx->time_base;
            
            src_stream_ctx[0].enc_ctx = enc_ctx;
        }
        else if (i == 1) // 音频流
        {
            dec_ctx = src_audio_filter_stream_ctx[0].audio_filter_dec_ctx;
            encoder = avcodec_find_encoder(dec_ctx->codec_id);
            enc_ctx = avcodec_alloc_context3(encoder);
            enc_ctx->sample_rate = dec_ctx->sample_rate;
            enc_ctx->channel_layout = dec_ctx->channel_layout;
            enc_ctx->channels = av_get_channel_layout_nb_channels(enc_ctx->channel_layout);
            enc_ctx->sample_fmt = encoder->sample_fmts[0];
            enc_ctx->time_base = (AVRational){1, enc_ctx->sample_rate};
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            {
                enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }
            ret = avcodec_open2(enc_ctx, encoder, NULL);
            ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
            out_stream->time_base = enc_ctx->time_base;
            src_audio_filter_stream_ctx[0].audio_filter_enc_ctx = enc_ctx;
        }
    }
    if (debug)
    {
        av_dump_format(ofmt_ctx, 0, filename, 1);
    }
    
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
    }
    ret = avformat_write_header(ofmt_ctx, NULL); // 写文件头，对于某些没有文件头的封装格式，不需要此函数，比如MPEG2TS
    return ret;
}

#pragma mark ------------ 初始化输入资源 ------------

static int init_src_files(char *src_img_path_list[])
{
    int ret = 0;
    int i;
    for ( i = 0; i < src_init_slice; i++)
    {
        int tmp_now_index = src_index+i;
        if (tmp_now_index >= src_totalCount)
        {
            break;
        }
        src_index = tmp_now_index;
        char *tmp_src_file = src_img_path_list[tmp_now_index];
        ret = open_input_file(tmp_src_file, src_index);
    }
    return ret;
}

static int init_add_files(char *src_img_path_list[])
{
    int ret = 0;
    int i;
    for ( i = 0; i < src_add_slice; i++)
    {
        char *tmp_src_file = src_img_path_list[src_index];
        ret = open_input_file(tmp_src_file, i);
    }
    return ret;
}

static int init_add_mask_png_files(char *mask_img_path)
{
    int ret = 0;
    ret = open_input_file(mask_img_path, 0);
    return ret;
}

static int init_src_filter_files(char *src_filter_path_list[])
{
    int ret = 0;
    int i;
    for ( i = 0; i < 2; i++)
    {
        char *tmp_src_file = src_filter_path_list[i];
        ret = open_filter_input_file(tmp_src_file, i);
    }
    return ret;
}

#pragma mark ------------ 读取 frame 数据 ------------

static AVFrame *read_frame_from_formatContext(int i)
{
    int ret;
    AVFrame *frame = NULL;
    AVFormatContext *formatContext = NULL;
    if (i == 0)
    {
        formatContext = ifmt_ctx;
    }
    else if (i == 1)
    {
        formatContext = ifmt_two_ctx;
    }
    AVPacket packet;
    if ((ret = av_read_frame(formatContext, &packet)) < 0)
    {
        goto fail;
    }
    av_packet_rescale_ts(&packet, formatContext->streams[0]->time_base, src_stream_ctx[0].dec_ctx->time_base);
    
    ret = avcodec_send_packet(src_stream_ctx[i].dec_ctx, &packet);
    if (ret < 0)
    {
        if (debug)
        {
            fprintf(stderr, "Error during decoding. Error code: %s\n", av_err2str(ret));
        }
        goto fail;
    }
    do{
        if (!(frame = av_frame_alloc()))
        {
            goto fail;
        }
        ret = avcodec_receive_frame(src_stream_ctx[i].dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            return frame;
        }
        else if (ret < 0)
        {
            if (debug)
            {
                fprintf(stderr, "Error while decoding. Error code: %s\n", av_err2str(ret));
            }
            goto fail;
        }
    }while ((ret >= 0) && (frame->width == 0));
fail:
    av_packet_unref(&packet);
    if (ret < 0)
    {
        return NULL;
    }
    return frame;
}

static AVFrame *read_filter_frame_from_formatContext(int i)
{
    int ret;
    AVFrame *frame = NULL;
    enum AVMediaType type;
    AVFormatContext *formatContext = NULL;
    if (i == 0)
    {
        formatContext = ifmt_filter_ctx;
    }
    else if (i == 1)
    {
        formatContext = ifmt_filter_two_ctx;
    }
    AVPacket packet;
    
    do{
        //        av_packet_unref(&packet);
        ret = av_read_frame(formatContext, &packet);
        type = ifmt_filter_ctx->streams[packet.stream_index]->codecpar->codec_type;
        
        if (audioB && type == AVMEDIA_TYPE_AUDIO && i == 0) // i = 0 表示花瓣视频，i = 1 表示mask视频
        {
            av_packet_rescale_ts(&packet,ifmt_filter_ctx->streams[1]->time_base,ofmt_ctx->streams[1]->time_base); // ofmt
            ret = av_interleaved_write_frame(ofmt_ctx, &packet);
        }
        if (ret < 0)
        {
            if (debug)
            {
                printf("input_file stream is error:-%s-\n",av_err2str(ret));
            }
            
            if (ret == AVERROR_EOF)
            {
                return NULL;
            }
        }
    }while ((type != AVMEDIA_TYPE_VIDEO) && (ret >= 0));
    
    ret = avcodec_send_packet(src_filter_stream_ctx[i].filter_dec_ctx, &packet);
    if (ret < 0)
    {
        if (debug)
        {
            fprintf(stderr, "Error during decoding. Error code: %s\n", av_err2str(ret));
        }
        goto fail;
    }
    if (!(frame = av_frame_alloc()))
    {
        goto fail;
    }
    ret = avcodec_receive_frame(src_filter_stream_ctx[i].filter_dec_ctx, frame);
    
    if (ret == AVERROR(EAGAIN))
    {
        return frame;
    }
    else if (ret == AVERROR_EOF)
    {
        return NULL;
    }
    else if (ret < 0)
    {
        if (debug)
        {
            fprintf(stderr, "Error while decoding. Error code: %s\n", av_err2str(ret));
        }
        goto fail;
    }
fail:
    av_packet_unref(&packet);
    if (ret < 0)
    {
        return frame;
    }
    return frame;
}

#pragma mark ------------ 编码音频&视频 ------------

// 编码经过处理后的帧
static int encode_write_frame(AVFrame *filt_frame, int *got_frame)
{
    int ret;
    int got_frame_local;
    AVPacket *enc_pkt;
    enc_pkt = av_packet_alloc();
    if (!got_frame)
    {
        got_frame = &got_frame_local;
    }
    enc_pkt->data = NULL;
    enc_pkt->size = 0;
    av_init_packet(enc_pkt);
    
    if ((ret = avcodec_send_frame(src_stream_ctx[0].enc_ctx, filt_frame)) < 0)
    {
        goto end;
    }
    while (1)
    {
        ret = avcodec_receive_packet(src_stream_ctx[0].enc_ctx, enc_pkt);
        if (ret)
        {
            break;
        }
        av_packet_rescale_ts(enc_pkt,
                             src_stream_ctx[0].enc_ctx->time_base,
                             ofmt_ctx->streams[0]->time_base);
        ret = av_interleaved_write_frame(ofmt_ctx, enc_pkt);
        if (ret < 0)
        {
            return -1;
        }
    }
    av_packet_unref(enc_pkt);
end:
    if (ret == AVERROR_EOF)
    {
        *got_frame = 0;
        return 0;
    }
    ret = ((ret == AVERROR(EAGAIN)) ? 0:-1);
    return ret;
}

// 输入的像素数据读取完成后调用此函数，用于输出编码器中剩余的AVPacket
static int flush_encoder(unsigned int stream_index)
{
    int ret;
    int got_frame;
    if (!(src_stream_ctx[0].enc_ctx->codec->capabilities & AV_CODEC_CAP_DELAY))
    {
        return 0;
    }
    while (1)
    {
        if (debug)
        {
            av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", stream_index);
        }
        ret = encode_write_frame(NULL, &got_frame);
        if (ret < 0)
        {
            break;
        }
        if (!got_frame)
        {
            return 0;
        }
    }
    return ret;
}

#pragma mark ------------ main 方法 ------------

int main(int argc, char *argv[])
{
    Log log;
    log.print_ms("start");
    
    if (argc < 5)
    {
        printf("usage: %s --bgimage'' --images='' --mask='' --out=''\n"
               "\n", argv[0]);
        return 1;
    }
    char *src_config_file_path = (char *)malloc(sizeof(char) * 1024);
    char *dst_output_file_path = (char *)malloc(sizeof(char) * 1024);
    char *src_filter_flower_file = (char *)malloc(sizeof(char) * 1024);
    char *src_filter_mask_file = (char *)malloc(sizeof(char) * 1024);
    char *bgimage_path = (char *)malloc(sizeof(char) * 1024);
    char **org_src_img_path_list = NULL;
    char *debug_c = (char *)malloc(sizeof(char) * 10);
    int i;
    for ( i = 0; i < argc; i++)
    {
        char *tmp_arg = argv[i];
        if (tmp_arg[0] == '-' && tmp_arg[1] == '-')
        {
            if (tmp_arg[2] == 'd' && tmp_arg[3] == 'e' && tmp_arg[4] == 'b')
            {
                // out：视频输出路径
                replace_subStr(tmp_arg, "--debug=", "", debug_c);
                if (debug_c[0] == '1')
                {
                    debug = true;
                }
            }
            else if (tmp_arg[2] == 'o' && tmp_arg[3] == 'u' && tmp_arg[4] == 't')
            {
                // out：视频输出路径
                replace_subStr(tmp_arg, "--out=", "", dst_output_file_path);
            }
            else if (tmp_arg[2] == 'm' && tmp_arg[3] == 'a' && tmp_arg[4] == 's')
            {
                // mask：视频mask
                char *tmp_path = (char *)malloc(sizeof(char) * 1024);;
                replace_subStr(tmp_arg, "--mask=\"", "", tmp_path);
                replace_subStr(tmp_path, "\"", "", tmp_path);
                int filter_count;
                char **tmp_list = split(tmp_path, '^', &filter_count);
                if (filter_count == 2)
                {
                    src_filter_flower_file = tmp_list[0];
                    src_filter_mask_file = tmp_list[1];
                }
            }
            else if (tmp_arg[2] == 'i' && tmp_arg[3] == 'm' && tmp_arg[4] == 'a')
            {
                // images：图片集合
                int images_count = 0;
                char *tmp_path = (char *)malloc(sizeof(char) * 2048);;
                replace_subStr(tmp_arg, "--images=\"", "", tmp_path);
                replace_subStr(tmp_path, "\"", "", tmp_path);
                org_src_img_path_list = split(tmp_path, '^', &images_count);
                src_totalCount += images_count;
            }
            else if (tmp_arg[2] == 'b' && tmp_arg[3] == 'g' && tmp_arg[4] == 'i')
            {
                // bgimage：背景图片，同时控制输出分辨率
                src_totalCount += 1;
                config_src_totalCount += 1;
                replace_subStr(tmp_arg, "--bgimage=", "", bgimage_path);
            }
            else if (tmp_arg[2] == 'c' && tmp_arg[3] == 'o' && tmp_arg[4] == 'n')
            {
                // AE配置文件地址
                replace_subStr(tmp_arg, "--config=", "", src_config_file_path);
            }
        }
    }
    ParseAE parseAE;
    av_log_set_level(debug?AV_LOG_INFO:AV_LOG_ERROR);
    parseAE.dofile(src_config_file_path, configEntity);
    config_src_totalCount += configEntity.assets_num;
    fps = configEntity.fr;
    
    if (config_src_totalCount != src_totalCount)
    {
        printf("资源总数与配置文件不等");
        return TPError::Error_diff_src_config;
    }
    
    DataTools dataTools;
    
    char **src_img_path_list;
    src_img_path_list = (char **) malloc(sizeof(char *) * src_totalCount);
    int j;
    for ( j = 0; j < src_totalCount; j++)
    {
        src_img_path_list[j] = (char *) malloc(sizeof(char) * 100);
        if (j == 0)
        {
            src_img_path_list[0] = bgimage_path;
        }
        else
        {
            strcpy (src_img_path_list[j],org_src_img_path_list[j-1]);
        }
    }
    
    // 初始化局部变量
    int ret;
    int tmp_pts = 0;
    AVFrame *frame = NULL;
    AVFrame **two_frame = (AVFrame **)av_mallocz_array(2, sizeof(AVFrame *));
    AVFrame **two_dst_frame = (AVFrame **)av_mallocz_array(configEntity.assets_num, sizeof(AVFrame *));
    
    int mask_count = 0;
    char **mask_img_path = NULL;
    char **mask_ind_path = NULL;
    char **mask_ref_id_path = NULL;
    int tmp_layer_i = 0;
    
    char *now_pwd = (char *)malloc(sizeof(char) * 1024);
    now_pwd[0] = '\0';
    get_pwd(src_config_file_path, now_pwd);
    char *mask_img_path_str = NULL;
    char *mask_img_ind_str = NULL;
    char *mask_img_ref_id_str = NULL;
    for (tmp_layer_i = 0; tmp_layer_i < configEntity.layers_num; ++tmp_layer_i)
    {
        AELayerEntity tmpEntity = configEntity.layers[tmp_layer_i];
        if (tmpEntity.hasMask)
        {
            const char *firstName = "tp_mask_";
            int midName = tmpEntity.ind;
            const char *lastName = ".png";
            
            char *name = (char *) malloc(strlen(now_pwd) + strlen(firstName) + length_int(midName) + strlen(lastName));
            sprintf(name, "%s%s%d%s",now_pwd, firstName, midName, lastName);
            if (mask_count > 0)
            {
                sprintf(mask_img_path_str, "%s%s",mask_img_path_str, "^");
                sprintf(mask_img_ind_str, "%s%s",mask_img_ind_str, "^");
                sprintf(mask_img_ref_id_str, "%s%s",mask_img_ref_id_str, "^");
            }
            else
            {
                mask_img_path_str = (char *)malloc(sizeof(char)*1024);
                mask_img_path_str[0] = '\0';
                mask_img_ind_str = (char *)malloc(sizeof(char)*1024);
                mask_img_ind_str[0] = '\0';
                mask_img_ref_id_str = (char *)malloc(sizeof(char)*1024);
                mask_img_ref_id_str[0] = '\0';
            }
            sprintf(mask_img_path_str, "%s%s",mask_img_path_str, name);
            sprintf(mask_img_ind_str, "%s%d",mask_img_ind_str, tmpEntity.ind);
            int tmp_count = 0;
            char **ref_id = NULL;
            ref_id = split(tmpEntity.refId, '_', &tmp_count);
            
            sprintf(mask_img_ref_id_str, "%s%s",mask_img_ref_id_str, ref_id[tmp_count-1]);
            mask_count++;
        }
    }
    AVFrame **mask_dst_frame = NULL;
    AVFrame **mask_two_dst_frame = NULL;
    if (mask_img_path_str)
    {
        mask_dst_frame = (AVFrame **)av_mallocz_array(mask_count, sizeof(AVFrame *));
        mask_two_dst_frame = (AVFrame **)av_mallocz_array(mask_count, sizeof(AVFrame *));
        mask_img_path = split(mask_img_path_str, '^', &mask_count);
        mask_ind_path = split(mask_img_ind_str, '^', &mask_count);
        mask_ref_id_path = split(mask_img_ref_id_str, '^', &mask_count);
    }
    
    char *src_filter_path_list[] = {src_filter_flower_file, src_filter_mask_file};
    AVFrame *dst_frame = NULL;
    // 初始化图片资源文件，每次只初始化 src_init_slice 数量的资源，后面每处理完最后一个资源，就新增 src_add_slice 资源初始化
    if ((ret = init_src_files(src_img_path_list)) < 0)
    {
        goto end;
    }
    frame = read_frame_from_formatContext(0);
    dst_screen_width = frame->width * 1.0;
    dst_screen_height = frame->height * 1.0;
    // 初始化filter资源文件，包括花瓣视频&花瓣mask视频
    if ((ret = init_src_filter_files(src_filter_path_list)) < 0)
    {
        goto end;
    }
    
    // 初始化写入目标文件，赋值
    if ((ret = open_dst_output_file(dst_output_file_path)) < 0)
    {
        goto end;
    }
    {
        // init 解码资源文件，根据背景图片，初始化导出视频大小
        dst_frame = dataTools.sws_origin_from_frame_to_sws_frame(frame, 0, dst_screen_width, dst_screen_height, exe_pix_fmt, src_stream_ctx[0].dec_ctx->width, src_stream_ctx[0].dec_ctx->height, src_stream_ctx[0].dec_ctx->pix_fmt);
        
        // 读取本地图片素材，two_dst_frame 即 swscale 以后的frame 集合
        for ( i = 1; i <= configEntity.assets_num; i++)
        {
            int tmp_i = i-1;
            if (tmp_i == 0)
            {
                two_frame[tmp_i] = read_frame_from_formatContext(1);
                two_dst_frame[tmp_i] = dataTools.sws_origin_from_frame_to_sws_frame(two_frame[tmp_i], 1, configEntity.assets[tmp_i].w, configEntity.assets[tmp_i].h, exe_pix_fmt, src_stream_ctx[1].dec_ctx->width, src_stream_ctx[1].dec_ctx->height, src_stream_ctx[1].dec_ctx->pix_fmt);
            }
            else
            {
                src_index++;
                if ((ret = init_add_files(src_img_path_list)) < 0)
                {
                    goto end;
                }
                two_frame[tmp_i] = read_frame_from_formatContext(0);
                two_dst_frame[tmp_i] = dataTools.sws_origin_from_frame_to_sws_frame(two_frame[tmp_i], 0, configEntity.assets[tmp_i].w, configEntity.assets[tmp_i].h, exe_pix_fmt, src_stream_ctx[0].dec_ctx->width, src_stream_ctx[0].dec_ctx->height, src_stream_ctx[0].dec_ctx->pix_fmt);
            }
        }
        int total_frame = configEntity.op-configEntity.ip+1;
        int all_assets_num = configEntity.assets_num;
        int all_layer_num = configEntity.layers_num;
        uint8_t ***cp_layer_target_frame = (uint8_t ***)av_mallocz_array(all_assets_num, sizeof(uint8_t **));
        int rgba_full_linesize = dst_screen_width*4; // 480*640 linesize[0] 480*4=1920
        int rgb_linesize_a[1024] = {0};
        // 预先加载全部 layer 到内存中，以后可以优化，只有在需要的时候再加载到内存，不需要的时候释放掉
        for ( i = 0; i < configEntity.assets_num; i++)
        {
            rgb_linesize_a[i] = two_dst_frame[i]->linesize[0];
            cp_layer_target_frame[i] = (uint8_t **)av_mallocz_array(1, sizeof(uint8_t *));
            cp_layer_target_frame[i][0] = two_dst_frame[i]->data[0];
        }
        uint8_t ***cp_mask_layer_target_frame = (uint8_t ***)av_mallocz_array(mask_count, sizeof(uint8_t **));
        for (i = 0; i < mask_count; i++)
        {
            if (access(mask_img_path[i], 0))
            {
                mask_count = 0;
                break;
            }
            if ((ret = init_add_mask_png_files(mask_img_path[i])) < 0)
            {
                goto end;
            }
            uint8_t *tmp_result_data = NULL;
            int m = atoi(mask_ref_id_path[i]);
            
            mask_dst_frame[i] = read_frame_from_formatContext(0);
            mask_two_dst_frame[i] = dataTools.sws_origin_from_frame_to_sws_frame(mask_dst_frame[i], 0, configEntity.assets[m].w, configEntity.assets[m].h, exe_pix_fmt, src_stream_ctx[0].dec_ctx->width, src_stream_ctx[0].dec_ctx->height, src_stream_ctx[0].dec_ctx->pix_fmt);
            uint8_t *yuvData[3] = {NULL};
            int yuvStride[3] = {0};
            
            dataTools.rgba_to_yuv_libyuv(mask_two_dst_frame[i]->data[0], yuvData, mask_two_dst_frame[i]->linesize, yuvStride, mask_two_dst_frame[i]->width, mask_two_dst_frame[i]->height);
            
            tmp_result_data = dataTools.combine_mask_data_rgba(mask_two_dst_frame[i]->width, mask_two_dst_frame[i]->height, rgb_linesize_a[m], cp_layer_target_frame[m][0], yuvStride[0],yuvData[0]);
            
            cp_mask_layer_target_frame[i] = (uint8_t **)av_mallocz_array(1, sizeof(uint8_t *));
            cp_mask_layer_target_frame[i][0] = tmp_result_data;
        }
        uint8_t *result_rgbData[1] = {NULL};
        uint8_t *rgb_Data[1] = {NULL};
        if (configEntity.assets_num == 0)
        {
            rgb_Data[0] = (uint8_t *)malloc(sizeof(uint8_t)*0);
            result_rgbData[0] = (uint8_t *)malloc(sizeof(uint8_t) * 0);
        }
        else
        {
            rgb_Data[0] =  (uint8_t *)malloc(sizeof(uint8_t) * rgba_full_linesize * dst_frame->height);
            dataTools.reset_cp_target_data_rgb(dst_frame->width, dst_frame->height, configEntity.assets[0].w, configEntity.assets[0].h, rgb_linesize_a[0],cp_layer_target_frame[0][0],rgba_full_linesize,rgb_Data[0]);
            result_rgbData[0] = dataTools.copy_frame_data(rgb_Data[0], rgba_full_linesize, dst_frame->height);
        }
        // 用于判断ae前后两帧的异同
        float reset_pre[1024] = {0};
        float reset_next[1024] = {0};
        int reset_pre_num = 0;
        int reset_next_num = 0;
        // 是否需要重新创建帧
        bool reset_b = false;
        uint8_t *filter_rgba_Data[1];
        filter_rgba_Data[0] = (uint8_t *)malloc(sizeof(uint8_t) * dst_frame->width * dst_frame->height * 4);
        uint8_t *filter_mask_y_Data[1];
        filter_mask_y_Data[0] = (uint8_t *)malloc(sizeof(uint8_t) * dst_frame->width * dst_frame->height);
        int mask_y_linesize = 0;
        uint8_t **loop_data = NULL;
        loop_data = (uint8_t **)malloc(sizeof(uint8_t *) * configEntity.layers_num);
        int loop_data_index = 0;
        for (loop_data_index = 0; loop_data_index < configEntity.layers_num; loop_data_index++)
        {
            loop_data[loop_data_index] = NULL;
        }
        // 遍历总时长，即遍历每一帧
        for ( i = 0; i < total_frame; i++)
        {
            dst_frame->pts = tmp_pts+i;
            dst_frame->pict_type = AV_PICTURE_TYPE_NONE;
            reset_next_num = 0;
            reset_b = false;
            // 获取当前帧的活动层数据
            for ( j = 0; j < all_layer_num; j++)
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
                            hor = dst_frame->width;
                            ver = 0;
                        }
                        else if (square_index == 2)
                        {
                            hor = 0;
                            ver = dst_frame->height;
                        }
                        else if (square_index == 3)
                        {
                            hor = dst_frame->width;
                            ver = dst_frame->height;
                        }
                        
                        float tmp_target_hor = 0;
                        float tmp_target_ver = 0;
                        
                        if (parent_ind == -1)
                        {
                            dataTools.scale_rotate_drift_action( hor, ver, anchor_x_f, anchor_y_f, angle_f, scale_f_x, scale_f_y, x_f, y_f, &tmp_target_hor, &tmp_target_ver);
                        }
                        else
                        {
                            int parent_ind_index = parseAE.layer_index_ind(parent_ind, configEntity);
                            AELayerEntity tmpLayerEntity_parent = configEntity.layers[parent_ind_index];
                            parseAE.get_ae_params( i, tmpLayerEntity_parent, &parent_angle_f, &parent_scale_f_x, &parent_scale_f_y, &parent_x_f, &parent_y_f, &parent_anchor_x_f, &parent_anchor_y_f, &parent_alpha_f, &parent_blur_radius);
                            dataTools.scale_rotate_drift_parent_action( hor, ver, anchor_x_f, anchor_y_f, angle_f, scale_f_x, scale_f_y, x_f, y_f,  parent_anchor_x_f, parent_anchor_y_f, parent_angle_f, parent_scale_f_x, parent_scale_f_y, parent_x_f, parent_y_f, &tmp_target_hor, &tmp_target_ver);
                        }
                        
                        tmp_s_x[square_index] = tmp_target_hor;
                        tmp_s_y[square_index] = tmp_target_ver;
                        
                        if (tmp_target_hor >= (dst_frame->width-1) || tmp_target_hor <= 0 || tmp_target_ver >= (dst_frame->height - 1) || tmp_target_ver <= 0 || alpha_f < 1.0)
                        {
                            break_b = false;
                        }
                    }
                    // 边界外 60 像素之外不绘制
                    int delta = 100;
                    float left_delta = 0-delta;
                    float up_delta = 0-delta;
                    float right_delta = dst_frame->width-1+delta;
                    float down_delta = dst_frame->height-1+delta;
                    if ((tmp_s_x[0] <= left_delta && tmp_s_x[1] <=left_delta  && tmp_s_x[2] <=left_delta  && tmp_s_x[3] <=left_delta) || (tmp_s_x[0] >= (dst_frame->width-1) && tmp_s_x[1] >= right_delta  && tmp_s_x[2] >= right_delta && tmp_s_x[3] >= right_delta) || (tmp_s_y[0] <= up_delta && tmp_s_y[1] <= up_delta  && tmp_s_y[2] <= up_delta  && tmp_s_y[3] <= up_delta) || (tmp_s_y[0] >= down_delta && tmp_s_y[1] >= down_delta  && tmp_s_y[2] >= down_delta  && tmp_s_y[3] >= down_delta))
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
            // 根据当前帧活动层与前一帧活动层数据是否一致，来判断是否重新刷新yuv数据
            if (reset_pre_num == reset_next_num)
            {
                int reset_i = 0;
                for (reset_i = 0; reset_i < reset_pre_num; reset_i++)
                {
                    if (reset_pre[reset_i] != reset_next[reset_i])
                    {
                        reset_b = true;
                        break;
                    }
                }
            }
            else
            {
                reset_b = true;
            }
            if (reset_b)
            {
                if (rgb_Data[0])
                {
                    av_free(rgb_Data[0]);
                    rgb_Data[0] = NULL;
                }
                bool first_suit_layer = false;
                rgb_Data[0] = NULL;
                memset(result_rgbData[0], 0, dst_frame->height * rgba_full_linesize);
                int tmp_index = reset_next_num/18;
                int tmp_pre_index = reset_pre_num/18;
                for ( j = 0; j < tmp_index; j++)
                {
                    bool reset_now_index = true;
                    float angle_f = reset_next[(tmp_index-1-j)*18+0];
                    float scale_f_x = reset_next[(tmp_index-1-j)*18+1];
                    float x_f = reset_next[(tmp_index-1-j)*18+2];
                    float y_f = reset_next[(tmp_index-1-j)*18+3];
                    float anchor_x_f = reset_next[(tmp_index-1-j)*18+4];
                    float anchor_y_f = reset_next[(tmp_index-1-j)*18+5];
                    float alpha_f = reset_next[(tmp_index-1-j)*18+6];
                    int exe_index = reset_next[(tmp_index-1-j)*18+7];
                    int blur_radius = reset_next[(tmp_index-1-j)*18+8];
                    int ind_index = reset_next[(tmp_index-1-j)*18+9];
                    float scale_f_y = reset_next[(tmp_index-1-j)*18+10];
                    int parent_ind = reset_next[(tmp_index-1-j)*18+11];
                    // parent params
                    float parent_angle_f = reset_next[(tmp_index-1-j)*18+12];
                    float parent_scale_f_x = reset_next[(tmp_index-1-j)*18+13];
                    float parent_scale_f_y = reset_next[(tmp_index-1-j)*18+14];
                    float parent_x_f = reset_next[(tmp_index-1-j)*18+15];
                    float parent_y_f = reset_next[(tmp_index-1-j)*18+16];
                    float parent_alpha_f = reset_next[(tmp_index-1-j)*18+17];
                    
                    int tmp_pre_i = 0;
                    for (tmp_pre_i = 0; tmp_pre_i < tmp_pre_index; tmp_pre_i++)
                    {
                        int pre_exe_index = reset_pre[tmp_pre_i * 19 + 7];
                        if (exe_index == pre_exe_index)
                        {
                            float pre_angle_f = reset_pre[(tmp_index-1-j)*18+0];
                            float pre_scale_f_x = reset_pre[(tmp_index-1-j)*18+1];
                            float pre_x_f = reset_pre[(tmp_index-1-j)*18+2];
                            float pre_y_f = reset_pre[(tmp_index-1-j)*18+3];
                            float pre_anchor_x_f = reset_pre[(tmp_index-1-j)*18+4];
                            float pre_anchor_y_f = reset_pre[(tmp_index-1-j)*18+5];
                            float pre_alpha_f = reset_pre[(tmp_index-1-j)*18+6];
                            int pre_blur_radius = reset_pre[(tmp_index-1-j)*18+8];
                            float pre_scale_f_y = reset_pre[(tmp_index-1-j)*18+10];
                            float pre_parent_ind = reset_pre[(tmp_index-1-j)*18+11];
                            // parent params
                            float pre_parent_angle_f = reset_pre[(tmp_index-1-j)*18+12];
                            float pre_parent_scale_f_x = reset_pre[(tmp_index-1-j)*18+13];
                            float pre_parent_scale_f_y = reset_pre[(tmp_index-1-j)*18+14];
                            float pre_parent_x_f = reset_pre[(tmp_index-1-j)*18+15];
                            float pre_parent_y_f = reset_pre[(tmp_index-1-j)*18+16];
                            float pre_parent_alpha_f = reset_pre[(tmp_index-1-j)*18+17];
                            
                            if (pre_angle_f == angle_f && pre_scale_f_x == scale_f_x && pre_scale_f_y == scale_f_y && pre_x_f == x_f && pre_y_f == y_f && pre_anchor_x_f == anchor_x_f && pre_anchor_y_f == anchor_y_f && pre_alpha_f == alpha_f && pre_blur_radius == blur_radius && pre_parent_ind == parent_ind && parent_angle_f == pre_parent_angle_f && parent_scale_f_x == pre_parent_scale_f_x && parent_scale_f_y == pre_parent_scale_f_y && parent_x_f == pre_parent_x_f && parent_y_f == pre_parent_y_f && parent_alpha_f == pre_parent_alpha_f)
                            {
                                reset_now_index = false;
                            }
                            break;
                        }
                    }
                    uint8_t *loop_rgbData[1];
                    loop_rgbData[0] = NULL;
                    
                    if (reset_now_index)
                    {
                        int tmp_exe_index = parseAE.layer_index_ind(exe_index, configEntity);
                        if (loop_data[tmp_exe_index] == NULL)
                        {
                            loop_data[tmp_exe_index] = (uint8_t *)malloc(sizeof(uint8_t) * rgba_full_linesize * dst_frame->height);
                            memset(loop_data[tmp_exe_index], 0, rgba_full_linesize * dst_frame->height);
                        }
                        
                        loop_rgbData[0] = loop_data[tmp_exe_index];
                        
                        AELayerEntity tmpLayerEntity = configEntity.layers[tmp_exe_index];
                        int asset_index = parseAE.asset_index_refId(tmpLayerEntity.refId, configEntity);
                        // 通过以上数据来实现动画以后的效果
                        float asset_width = configEntity.assets[asset_index].w;
                        float asset_height = configEntity.assets[asset_index].h;
                        int asset_rgb_linesize = rgb_linesize_a[asset_index];
                        
                        if (ind_index != -1 && mask_count > 0)
                        {
                            int tmp_search_i = 0;
                            for (tmp_search_i = 0 ; tmp_search_i < mask_count; tmp_search_i++)
                            {
                                char *tmp_c = mask_ind_path[tmp_search_i];
                                int ind_int = atoi(tmp_c);
                                if (ind_int == ind_index)
                                {
                                    break;
                                }
                            }
                            if (parent_ind == -1)
                            {
                                dataTools.scale_rotate_drift_rgb(anchor_x_f, anchor_y_f,angle_f, scale_f_x, scale_f_y, x_f, y_f, cp_mask_layer_target_frame[tmp_search_i][0], asset_rgb_linesize, asset_width, asset_height, loop_rgbData[0],rgba_full_linesize, dst_frame->width, dst_frame->height);
                            }
                            else
                            {
                                int parent_ind_index = parseAE.layer_index_ind(parent_ind, configEntity);
                                AELayerEntity tmpLayerEntity = configEntity.layers[parent_ind_index];
                                
                                float parent_angle_f = 0;
                                float parent_scale_f_x = 1.0;
                                float parent_scale_f_y = 1.0;
                                float parent_x_f = 0;
                                float parent_y_f = 0;
                                float parent_anchor_x_f = 0;
                                float parent_anchor_y_f = 0;
                                float parent_alpha_f = 1.0;
                                int parent_blur_radius = 0.0;
                                
                                parseAE.get_ae_params(i, tmpLayerEntity, &parent_angle_f, &parent_scale_f_x, &parent_scale_f_y, &parent_x_f, &parent_y_f, &parent_anchor_x_f, &parent_anchor_y_f, &parent_alpha_f, &parent_blur_radius);
                                dataTools.scale_rotate_drift_rgb_parent(anchor_x_f, anchor_y_f,angle_f, scale_f_x, scale_f_y, x_f, y_f, cp_mask_layer_target_frame[tmp_search_i][0], asset_rgb_linesize, asset_width, asset_height, loop_rgbData[0],rgba_full_linesize, dst_frame->width, dst_frame->height, parent_anchor_x_f, parent_anchor_y_f, parent_angle_f, parent_scale_f_x, parent_scale_f_y, parent_x_f, parent_y_f);
                            }
                        }
                        else
                        {
                            if (parent_ind == -1)
                            {
                                dataTools.scale_rotate_drift_rgb(anchor_x_f, anchor_y_f,angle_f, scale_f_x, scale_f_y, x_f, y_f, cp_layer_target_frame[asset_index][0], asset_rgb_linesize, asset_width, asset_height, loop_rgbData[0],rgba_full_linesize, dst_frame->width, dst_frame->height);
                            }
                            else
                            {
                                int parent_ind_index = parseAE.layer_index_ind(parent_ind, configEntity);
                                AELayerEntity tmpLayerEntity = configEntity.layers[parent_ind_index];
                                
                                float parent_angle_f = 0;
                                float parent_scale_f_x = 1.0;
                                float parent_scale_f_y = 1.0;
                                float parent_x_f = 0;
                                float parent_y_f = 0;
                                float parent_anchor_x_f = 0;
                                float parent_anchor_y_f = 0;
                                float parent_alpha_f = 1.0;
                                int parent_blur_radius = 0.0;
                                
                                parseAE.get_ae_params(i, tmpLayerEntity, &parent_angle_f, &parent_scale_f_x, &parent_scale_f_y, &parent_x_f, &parent_y_f, &parent_anchor_x_f, &parent_anchor_y_f, &parent_alpha_f, &parent_blur_radius);
                                dataTools.scale_rotate_drift_rgb_parent(anchor_x_f, anchor_y_f,angle_f, scale_f_x, scale_f_y, x_f, y_f, cp_layer_target_frame[asset_index][0], asset_rgb_linesize, asset_width, asset_height, loop_rgbData[0],rgba_full_linesize, dst_frame->width, dst_frame->height, parent_anchor_x_f, parent_anchor_y_f, parent_angle_f, parent_scale_f_x, parent_scale_f_y, parent_x_f, parent_y_f);
                            }
                        }
                        if (blur_radius > 0)
                        {
                            dataTools.gaussian_blur_rgba(blur_radius, 1.0, loop_rgbData[0], rgba_full_linesize, dst_frame->width, dst_frame->height);
                        }
                        memcpy(loop_data[tmp_exe_index], loop_rgbData[0], rgba_full_linesize * dst_frame->height);
                    }
                    else
                    {
                        int tmp_exe_index = parseAE.layer_index_ind(exe_index, configEntity);
                        loop_rgbData[0] = loop_data[tmp_exe_index];
                    }
                    
                    if (first_suit_layer == false)
                    {
                        first_suit_layer = true;
                        memcpy(result_rgbData[0], loop_rgbData[0], rgba_full_linesize*dst_frame->height);
                    }
                    else
                    {
                        int hor = 0;
                        int ver = 0;
                        int rgb_step = 4;
                        int r_offset = 0;
                        int g_offset = 1;
                        int b_offset = 2;
                        int a_offset = 3;
                        for (ver = 0; ver < dst_frame->height; ver++)
                        {
                            for (hor = 0; hor < dst_frame->width; hor++)
                            {
                                int tmp_loop_r = ver * rgba_full_linesize + hor * rgb_step + r_offset;
                                int tmp_loop_g = ver * rgba_full_linesize + hor * rgb_step + g_offset;
                                int tmp_loop_b = ver * rgba_full_linesize + hor * rgb_step + b_offset;
                                int tmp_loop_a = ver * rgba_full_linesize + hor * rgb_step + a_offset;
                                
                                int r = result_rgbData[0][tmp_loop_r];
                                int g = result_rgbData[0][tmp_loop_g];
                                int b = result_rgbData[0][tmp_loop_b];
                                int a = result_rgbData[0][tmp_loop_a];
                                
                                int loop_r = loop_rgbData[0][tmp_loop_r];
                                int loop_g = loop_rgbData[0][tmp_loop_g];
                                int loop_b = loop_rgbData[0][tmp_loop_b];
                                int loop_a = loop_rgbData[0][tmp_loop_a];
                                
                                if (loop_r != green[0] || loop_g != green[1] || loop_b != green[2] || loop_a != green[3])
                                {
                                    if (alpha_f < 1.0 || loop_a < 255)
                                    {
                                        if (loop_a >= 100)
                                        {
                                            result_rgbData[0][tmp_loop_r] = (loop_r * alpha_f + r * (1-alpha_f))*loop_a/255.0;
                                            result_rgbData[0][tmp_loop_g] = (loop_g * alpha_f + g * (1-alpha_f))*loop_a/255.0;
                                            result_rgbData[0][tmp_loop_b] = (loop_b * alpha_f + b * (1-alpha_f))*loop_a/255.0;
                                            result_rgbData[0][tmp_loop_a] = loop_a * alpha_f + a * (1-alpha_f);
                                        }
                                    }
                                    else
                                    {
                                        result_rgbData[0][tmp_loop_r] = loop_r;
                                        result_rgbData[0][tmp_loop_g] = loop_g;
                                        result_rgbData[0][tmp_loop_b] = loop_b;
                                        result_rgbData[0][tmp_loop_a] = loop_a;
                                    }
                                }
                            }
                        }
                    }
                    
                    loop_rgbData[0] = NULL;
                }
                // rgb 拷贝
                rgb_Data[0] = dataTools.copy_frame_data(result_rgbData[0], rgba_full_linesize, dst_frame->height);
                memmove(reset_pre, reset_next, sizeof(float)*reset_next_num);
                reset_pre_num = reset_next_num;
                
            }
            AVFrame *filter_frame = NULL;
            AVFrame *filter_mask_frame = NULL;
            do{
                filter_frame = read_filter_frame_from_formatContext(0);
            }while (filter_frame && (filter_frame->width == 0));
            do{
                filter_mask_frame = read_filter_frame_from_formatContext(1);
            }while (filter_mask_frame && (filter_mask_frame->width == 0));
            if (filter_frame && filter_mask_frame)
            {
                if (filter_rgba_Data[0] != NULL && filter_mask_y_Data[0] && mask_y_linesize != 0)
                {
                    free(filter_rgba_Data[0]);
                    filter_rgba_Data[0] = NULL;
                    
                    free(filter_mask_y_Data[0]);
                    filter_mask_y_Data[0] = NULL;
                    
                    mask_y_linesize = 0;
                }
                
                filter_rgba_Data[0] = (uint8_t *)malloc(sizeof(uint8_t) * rgba_full_linesize * dst_frame->height);
                // yuv 转 rgb
                dataTools.yuv_to_rgba_libyuv(filter_frame->data, filter_rgba_Data, filter_frame->linesize, &rgba_full_linesize, dst_frame->width, dst_frame->height);
                
                filter_mask_y_Data[0] = dataTools.copy_frame_data(filter_mask_frame->data[0], filter_mask_frame->linesize[0], dst_frame->height);
                mask_y_linesize = filter_mask_frame->linesize[0];
            }
            else
            {
                goto write_end;
            }
            uint8_t *rgba_copy_Data = dataTools.copy_frame_data(rgb_Data[0], rgba_full_linesize, dst_frame->height);
            if (filter_rgba_Data[0] != NULL &&  filter_mask_y_Data[0] != NULL && mask_y_linesize != 0)
            {
                int r_index = 0;
                int g_index = 1;
                int b_index = 2;
                float tmpRatio = 0.0;
                int ver = 0;
                
                cv::Mat mat_rgba_copy_data = dataTools.frame_data_to_cvmat(dst_frame->height, dst_frame->width, rgba_copy_Data, rgba_full_linesize);
                for (ver = 0; ver < mat_rgba_copy_data.rows; ver++)
                {
                    int hor = 0;
                    for ( hor = 0; hor < mat_rgba_copy_data.cols; hor++)
                    {
                        Vec4b &m_c = mat_rgba_copy_data.at<Vec4b>(ver, hor);
                        
                        int tmpY = filter_mask_y_Data[0][ver * mask_y_linesize + hor];
                        
                        if (tmpY > 235)
                        {
                            tmpRatio = 1.0;
                        }
                        else
                        {
                            tmpRatio = tmpY * 1.0/235;
                        }
                        
                        int tmpResult_r = rgb_Data[0][r_index] * (1-tmpRatio) + filter_rgba_Data[0][r_index];
                        int tmpResult_g = rgb_Data[0][g_index] * (1-tmpRatio) + filter_rgba_Data[0][g_index];
                        int tmpResult_b = rgb_Data[0][b_index] * (1-tmpRatio) + filter_rgba_Data[0][b_index];
                        
                        if (tmpResult_r > 255)
                        {
                            tmpResult_r = 255;
                        }
                        if (tmpResult_g > 255)
                        {
                            tmpResult_g = 255;
                        }
                        if (tmpResult_b > 255)
                        {
                            tmpResult_b = 255;
                        }
                        m_c[0] = tmpResult_r;
                        m_c[1] = tmpResult_g;
                        m_c[2] = tmpResult_b;
                        
                        r_index += 4;
                        g_index += 4;
                        b_index += 4;
                    }
                }
            }
            if (i == (total_frame-1) && filter_rgba_Data[0] != NULL && filter_mask_y_Data[0] != NULL && mask_y_linesize != 0)
            {
                free(filter_rgba_Data[0]);
                filter_rgba_Data[0] = NULL;
                
                free(filter_mask_y_Data[0]);
                filter_mask_y_Data[0] = NULL;
            }
            uint8_t *yuvData[3] = {NULL};
            int yuvStride[3] = {0};
            dataTools.rgba_to_yuv_libyuv(rgba_copy_Data, yuvData, &rgba_full_linesize, yuvStride, dst_frame->width, dst_frame->height);
            dst_frame->data[0] = yuvData[0];
            dst_frame->data[1] = yuvData[1];
            dst_frame->data[2] = yuvData[2];
            dst_frame->linesize[0] = yuvStride[0];
            dst_frame->linesize[1] = yuvStride[1];
            dst_frame->linesize[2] = yuvStride[2];
            free(rgba_copy_Data);
            rgba_copy_Data = NULL;
            av_frame_free(&filter_frame);
            av_frame_free(&filter_mask_frame);
            ret = encode_write_frame(dst_frame, NULL);
            free(yuvData[0]);
            yuvData[0] = NULL;
            free(yuvData[1]);
            yuvData[1] = NULL;
            free(yuvData[2]);
            yuvData[2] = NULL;
        }
    }
write_end:
    /* flush encoder */
    ret = flush_encoder(0);
    av_write_trailer(ofmt_ctx); // 写文件尾，对于某些没有文件头的封装格式，不需要此函数，比如MPEG2TS
end:
//    av_frame_free(&dst_frame);
    avformat_close_input(&ifmt_ctx);
    avformat_close_input(&ifmt_two_ctx);
//    avcodec_free_context(&src_stream_ctx[0].enc_ctx);
    
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        avio_closep(&ofmt_ctx->pb);
    }
//    av_free(src_stream_ctx);

    log.print_ms("  end");

    return ret;
}
