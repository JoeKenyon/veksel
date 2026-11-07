// Delay effect
// KNOB_1 = delay time  (0..1 -> 0..1 second)
// KNOB_2 = feedback    (0..1 -> 0..90%)
// KNOB_3 = dry/wet mix (0..1 -> 0%..100% wet)
//
// Buffer is sized for 2 seconds at 48 kHz, so it works at any common
// sample rate (44.1, 48, 88.2, 96). Statics reset when you save — that's
// expected.

#define MAX_DELAY 96000

float buffer[MAX_DELAY];
int write_ptr = 0;

veksel_main {
    // how many samples the delay knob is asking for
    int max_samples = (int)SAMPLE_RATE;
    if (max_samples > MAX_DELAY - 1) max_samples = MAX_DELAY - 1;

    int delay_samples = (int)(KNOB_1 * max_samples) + 1;
    float feedback = KNOB_2 * 0.9f;
    float mix = KNOB_3;

    // read position is delay_samples behind the write head, wrapped
    int read_ptr = write_ptr - delay_samples;
    if (read_ptr < 0) read_ptr += MAX_DELAY;

    // read the delayed sample, write input + feedback into the buffer
    float delayed = buffer[read_ptr];
    buffer[write_ptr] = input + delayed * feedback;

    // advance the write head
    write_ptr++;
    if (write_ptr >= MAX_DELAY) write_ptr = 0;

    // mix dry and wet
    return input * (1.0f - mix) + delayed * mix;
}
