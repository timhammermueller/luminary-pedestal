#include <stdlib.h>
#include <math.h>
#include <effect.h>

void reset_cooldown(float *timer, float duration){
    if (!timer || duration <= 0.0f) return;

    if (*timer < 0.0f) {
        float overshoot = -(*timer);
        float overshoot_mod = fmodf(overshoot, duration);
        *timer = (overshoot_mod == 0.0f) ? 0.0f : (duration - overshoot_mod);
    }
}