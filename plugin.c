#define MAX_DELAY 96000

float buffer[MAX_DELAY];
int write_ptr = 0;

veksel_main 
{
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
