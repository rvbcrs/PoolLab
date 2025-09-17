## Arduino uses its own build; below is only for ESP-IDF envs.

idf_component_register(
  SRCS
    "/Users/rvbcrs/GitHub/JC3248W535EN/src/DEMO_LVGL.c"
    "/Users/rvbcrs/GitHub/JC3248W535EN/src/esp_bsp.c"
    "/Users/rvbcrs/GitHub/JC3248W535EN/src/esp_lcd_axs15231b.c"
    "/Users/rvbcrs/GitHub/JC3248W535EN/src/esp_lcd_touch.c"
    "/Users/rvbcrs/GitHub/JC3248W535EN/src/lv_port.c"
    "/Users/rvbcrs/GitHub/JC3248W535EN/libraries/lvgl/src/demos/widgets/lv_demo_widgets.c"
    "/Users/rvbcrs/GitHub/JC3248W535EN/libraries/lvgl/src/demos/widgets/assets/img_demo_widgets_avatar.c"
    "/Users/rvbcrs/GitHub/JC3248W535EN/libraries/lvgl/src/demos/widgets/assets/img_lvgl_logo.c"
    "/Users/rvbcrs/GitHub/JC3248W535EN/libraries/lvgl/src/demos/widgets/assets/img_clothes.c"
  INCLUDE_DIRS
    "/Users/rvbcrs/GitHub/JC3248W535EN/src"
    "/Users/rvbcrs/GitHub/JC3248W535EN/libraries/lvgl/src"
  REQUIRES
    driver
    esp_lcd
)

# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

# Remove auto glob; we register sources explicitly above for IDF demo
