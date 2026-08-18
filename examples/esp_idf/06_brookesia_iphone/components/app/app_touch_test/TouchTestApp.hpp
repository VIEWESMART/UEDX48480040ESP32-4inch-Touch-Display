#pragma once

#include "esp_brookesia.hpp"

class TouchTestApp : public ESP_Brookesia_PhoneApp {
public:
  TouchTestApp();
  ~TouchTestApp() override = default;

  bool run(void) override;
  bool back(void) override;
  bool close(void) override;

private:
  lv_obj_t *_canvas = nullptr;
  lv_obj_t *_coord_label = nullptr;

  void updateCoordLabel(int x, int y);

  static void canvas_event_cb(lv_event_t *e);
};
