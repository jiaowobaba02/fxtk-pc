# fxtk 库组件: 被 demo (根工程 main) 和 examples/* 共同引用
idf_component_register(
    SRCS "fxtk.c" "fxtk_draw.c" "fxtk_font.c" "fxtk_widgets.c"
         "fxtk_st6201.c" "fxtk_touch.c" "cn_gray.c" "ascii_gray16.c"
    INCLUDE_DIRS "."
    REQUIRES driver)
