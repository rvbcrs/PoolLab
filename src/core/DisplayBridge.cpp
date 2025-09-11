#include "DisplayBridge.h"

namespace core {

DisplayBridge::DisplayBridge(Arduino_GFX* gfx)
  : _gfx(gfx), _buf1(nullptr) {
}

void DisplayBridge::initLvgl(uint16_t bufHeight) {
  lv_init();
  uint16_t hor = _gfx->width();
  _buf1 = (lv_color_t*)malloc(hor * bufHeight * sizeof(lv_color_t));
  lv_disp_draw_buf_init(&_drawBuf, _buf1, NULL, hor * bufHeight);
}

lv_disp_t* DisplayBridge::registerDisplay() {
  lv_disp_drv_init(&_dispDrv);
  _dispDrv.hor_res = _gfx->width();
  _dispDrv.ver_res = _gfx->height();
  _dispDrv.draw_buf = &_drawBuf;
  _dispDrv.flush_cb = [](lv_disp_drv_t *d, const lv_area_t *a, lv_color_t *p){
    Arduino_GFX* gfx = (Arduino_GFX*)d->user_data;
    int32_t w = (a->x2 - a->x1 + 1);
    int32_t h = (a->y2 - a->y1 + 1);
#if (LV_COLOR_16_SWAP != 0)
    gfx->draw16bitBeRGBBitmap(a->x1, a->y1, (uint16_t*)p, w, h);
#else
    gfx->draw16bitRGBBitmap(a->x1, a->y1, (uint16_t*)p, w, h);
#endif
    lv_disp_flush_ready(d);
  };
  _dispDrv.user_data = _gfx;
  return lv_disp_drv_register(&_dispDrv);
}

} // namespace core


