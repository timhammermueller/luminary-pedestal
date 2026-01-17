#include <effect.h>
#include <stdint.h>
#include <string.h>

extern effect_t g_test_effect;
extern effect_t g_organic_breathe_v1_effect;
extern effect_t g_individual_test_effect;


effect_t *g_effects[] = {
    &g_test_effect,
    &g_organic_breathe_v1_effect,
    &g_individual_test_effect,
};

size_t g_effects_count = sizeof(g_effects) / sizeof(g_effects[0]);

effect_t *effect_find_by_name(const char *name)
{
    if (!name) return NULL;

    for (size_t i = 0; i < g_effects_count; i++) {
        if (strcmp(g_effects[i]->name, name) == 0) {
            return g_effects[i];
        }
    }
    return NULL;
}