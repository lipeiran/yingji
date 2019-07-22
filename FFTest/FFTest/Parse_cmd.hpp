//
//  Parse_cmd.hpp
//  FFTest
//
//  Created by 李沛然 on 2019/7/19.
//  Copyright © 2019 李沛然. All rights reserved.
//

#ifndef Parse_cmd_hpp
#define Parse_cmd_hpp

#include <stdio.h>
#include "BasicTools.hpp"

class ParseCMD{
public:
    void parse_cmd(int argc, char *argv[], char *debug_c, bool & debug, char *dst_output_file_path, char *src_filter_flower_file, char *src_filter_mask_file , char **org_src_img_path_list, int &src_totalCount, int &config_src_totalCount, char *bgimage_path, char *src_config_file_path);

private:
    
};

#endif /* Parse_cmd_hpp */
