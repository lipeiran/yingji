//
//  TPError.hpp
//  FFTest
//
//  Created by 李沛然 on 2019/7/19.
//  Copyright © 2019 李沛然. All rights reserved.
//

#ifndef TPError_hpp
#define TPError_hpp

#include <stdio.h>

class TPError{
public:
    enum Error_ret{
        Error_src_img = 1, // 初始资源图片错误
        Error_add_img,     // 增加资源图片错误
        Error_src_filter,  // 初始mask视频错误
        Error_open_dst,    // 初始目标文件错误
        Error_diff_src_config,  // 资源图片数量不符合要求
    };
    
private:
    
};

#endif /* TPError_hpp */
