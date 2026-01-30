#define _GNU_SOURCE
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

/**
 * Takes an integer value in the range "from" and maps it onto the range "to".
 *
 * @param value Value that is being mapped.
 * @param from_min Lower bound of the input interval.
 * @param from_max Higher bound of the input interval.
 * @param to_min Lower bound of the output interval.
 * @param to_max Higher bound of the output interval.
 * @return Mapped value.
 */
int scale_i(int value, int from_min, int from_max, int to_min, int to_max)
{
    return ((value - from_min) * (to_max - to_min)) / (from_max - from_min) + to_min;
}

/**
 * Takes a double precision floating point value in the range "from" and maps
 * it onto the range "to".
 *
 * @param value Value that is being mapped.
 * @param from_min Lower bound of the input interval.
 * @param from_max Higher bound of the input interval.
 * @param to_min Lower bound of the output interval.
 * @param to_max Higher bound of the output interval.
 * @return Mapped value.
 */
double scale_d(double value, double from_min, double from_max, double to_min, double to_max)
{
    return ((value - from_min) * (to_max - to_min)) / (from_max - from_min) + to_min;
}

/**
 * Writes a header of the plain pgm format consisting of 4 lines along with
 * descriptive comments:
 *
 * 1. The magic number specifying the image to be grayscale and written in
 * plain text
 *
 * 2. Dimensions of the image
 *
 * 3. Maximum value of the numbers that follow the header, corresponding to
 * white
 *
 * 4. A comment stating the end of the header (not required)
 *
 * After calling this function, write `width`*`height` integers to `f`. They
 * need to be in the range [0,255] and separated by spaces.
 * See: https://netpbm.sourceforge.net/doc/pgm.html
 *
 * @param f Stream to which the header is being written to. Needs to be at the
 * start of the file.
 * @param width Width of the image. Also the number of values after which the
 * next line of pixels starts.
 * @param height Height of the image.
 */
void pgm_header(FILE* f, int width, int height)
{
    fprintf(f, "P2 # magic number\n");
    fprintf(f, "%d %d # width height\n", width, height);
    fprintf(f, "255 # maximum value\n");
    fprintf(f, "# This is the end of the header\n");  // comment for clarity
}
/**
 * Transforms a value from the interval [`min`, `max`], so that the values are
 * closer to `max` more quickly, e.g. resulting in a brighter image.
 *
 * @param x Value to be transformed.
 * @param min Lower bound of the input/output interval.
 * @param max Higher bound of the input/output interval.
 * @return Transformed value.
 */
double brighten(double x, double min, double max)
{
    double v = scale_d(x, min, max, 0, 1);
    v = sqrt(v);
    return scale_d(v, 0, 1, min, max);
}
