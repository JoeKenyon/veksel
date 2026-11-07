float buffer[44100];
int write_ptr = 0;

veksel_main
{
    int d = (int)(44100);
    if (d < 1) d = 1;
    int rp = write_ptr - d;
    if (rp < 0) rp += 44100;
    float delayed = buffer[rp];
    buffer[write_ptr] = input + delayed * 0.5;
    write_ptr = (write_ptr + 1) % 44100;
    return input + delayed * 0.5;
}
