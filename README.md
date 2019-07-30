# yingji
服务器影集项目，可以在Mac上测试，具体可以参考Xcode command Line 项目执行。

此项目中文件也可以放到 Linux 中。
以下是我在服务器的执行命令参考：

GCC：

gcc -O3 -lm -lstdc++ code.c cJSON.c -o code -I /root/ffmpeg_build/include/ -I /usr/include/ -I /usr/local/src/lipeiran/code_source/ -L /root/ffmpeg_build/lib -lavformat -lavcodec -lswresample -lavutil -lswscale -L /usr/lib -lz -lpthread -lx264 -ldl -lvpx -liconv -lx265 -lfreetype -lbz2 -lfdk-aac -lmp3lame -lopus -llzma -lyuv

G++:

g++ -O3 -lm -lstdc++ 1.cpp cJSON.c -o code123 -I /root/ffmpeg_build/include/ -I /usr/include/ -I /usr/local/src/lipeiran/code_source/ -L /root/ffmpeg_build/lib -lavformat -lavcodec -lswresample -lavutil -lswscale -L /usr/lib -lz -lpthread -lx264 -ldl -lvpx -liconv -lx265 -lfreetype -lbz2 -lfdk-aac -lmp3lame -lopus -llzma -lyuv

G++ opencv:

g++ -O3 -lm -std=c++11 -lstdc++ 2_opencv.cpp cJSON.c -o code_2_opencv -I /root/ffmpeg_build/include/ -I /usr/local/include/ -I /usr/local/include/opencv4 -I /usr/include/ -I /usr/local/src/lipeiran/code_source/ -L /root/ffmpeg_build/lib -lavformat -lavcodec -lswresample -lavutil -lswscale -L /usr/lib -lz -lpthread -lx264 -ldl -lvpx -liconv -lx265 -lfreetype -lbz2 -lfdk-aac -lmp3lame -lopus -llzma -lyuv -L /usr/local/lib64 -lopencv_stitching -lopencv_objdetect -lopencv_calib3d -lopencv_features2d -lopencv_highgui -lopencv_videoio -lopencv_imgcodecs -lopencv_video -lopencv_photo -lopencv_ml -lopencv_imgproc -lopencv_flann -lopencv_core

执行：
./code --bgimage=/usr/local/src/lipeiran/image_source/bg.jpg --config=/usr/local/src/lipeiran/mask_config_source/151/tp.json --mask=\"/usr/local/src/lipeiran/mask_config_source/151/tp_fg.mp4^/usr/local/src/lipeiran/mask_config_source/151/tp_mask.mp4\" --images=\"/usr/local/src/lipeiran/mask_config_source/151/images/img_0.png^/usr/local/src/lipeiran/mask_config_source/151/images/img_1.png^/usr/local/src/lipeiran/mask_config_source/151/images/img_2.png^/usr/local/src/lipeiran/mask_config_source/151/images/img_3.png\" --out=/usr/local/src/lipeiran/mask_config_source/151/result.mp4 --debug=1
