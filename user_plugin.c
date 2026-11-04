#include <math.h>
#line 1
float process(float sample)
{
    // soft clip
    float x = sample * 5.0f;
    return x / (1.0f + fabsf(x));
}

