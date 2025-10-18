#include "fx_segments.h"

segment_t seg_from_params(const aled_channel_t* ch, const effect_params_t* p){
  segment_t s = {
    .start = p ? p->seg_start : 0u,
    .len = (p && p->seg_len) ? p->seg_len : ch->n_pixels
  };

  if (!ch || ch->n_pixels == 0){
    s.start = 0;
    s.len = 0;
    return s;
  }

  if (s.start >= ch->n_pixels){
    s.start = 0;
    s.len = ch->n_pixels;
    return s;
  }

  if (s.start + s.len > ch->n_pixels){
    s.len = ch->n_pixels - s.start;
  }

  return s;
}

bool seg_is_full(const aled_channel_t* ch, segment_t s){
  if (!ch){
    return false;
  }
  return s.start == 0 && s.len == ch->n_pixels;
}
