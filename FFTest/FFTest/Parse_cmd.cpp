//
//  Parse_cmd.cpp
//  FFTest
//
//  Created by 李沛然 on 2019/7/19.
//  Copyright © 2019 李沛然. All rights reserved.
//

#include "Parse_cmd.hpp"

void ParseCMD::parse_cmd(int argc, char *argv[], char *debug_c, bool & debug, char *dst_output_file_path, char *src_filter_flower_file, char *src_filter_mask_file , char **org_src_img_path_list, int &src_totalCount, int &config_src_totalCount, char *bgimage_path, char *src_config_file_path)
{
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
                    strcpy(src_filter_flower_file, tmp_list[0]);
                    strcpy(src_filter_mask_file, tmp_list[1]);
                }
            }
            else if (tmp_arg[2] == 'i' && tmp_arg[3] == 'm' && tmp_arg[4] == 'a')
            {
                // images：图片集合
                int images_count = 0;
                char *tmp_path = (char *)malloc(sizeof(char) * 2048);
                replace_subStr(tmp_arg, "--images=\"", "", tmp_path);
                replace_subStr(tmp_path, "\"", "", tmp_path);
                char **tmp_org_src_img_path_list = split(tmp_path, '^', &images_count);
                int j = 0;
                for (; j < images_count; j++)
                {
                    org_src_img_path_list[j] = (char *) malloc(sizeof(char) * 100);
                    strcpy(org_src_img_path_list[j], tmp_org_src_img_path_list[j]);
                }
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
}
