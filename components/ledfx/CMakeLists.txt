idf_component_register(
    SRCS 
        "effect.c" 
        "effects_registry.c" 
        "test_effect.c"
        "organic_breathe_v1.c"
        "effect_math.c"
        "individual_test_effect.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        "apa102"

)
