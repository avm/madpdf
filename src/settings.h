#ifndef MADPDF_SETTINGS_H
#define MADPDF_SETTINGS_H

typedef struct 
{
    // The minimum amount (%) of overlap between panning positions
    int pan_overlap;

    int hpan;
    int vpan;
    int ltrimpad;
    int rtrimpad;
    int zoominc;
} progsettings;

void save_settings(const char *filename);
progsettings *get_settings();
void load_settings(const char *filename);
void free_settings();

/* vim:set sw=4 ts=4 et: */
#endif
