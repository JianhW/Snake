
#include <stdint.h>
#include "bsp.h"

/////////////////////////////////////////////////////
// framebuffer
/////////////////////////////////////////////////////

#define FB_BASE 0xF8100000

volatile uint16_t *fb = (volatile uint16_t *)FB_BASE;

/////////////////////////////////////////////////////
// screen
/////////////////////////////////////////////////////

#define FB_WIDTH   64
#define FB_HEIGHT  64

/////////////////////////////////////////////////////
// game grid
/////////////////////////////////////////////////////

#define CELL_SIZE  2

#define GRID_W     32
#define GRID_H     32

/////////////////////////////////////////////////////
// snake
/////////////////////////////////////////////////////

#define MAX_SNAKE_LENGTH 128

int snake_x[MAX_SNAKE_LENGTH];
int snake_y[MAX_SNAKE_LENGTH];

int snake_length = 3;

/////////////////////////////////////////////////////
// direction
/////////////////////////////////////////////////////

int dir_x = 1;
int dir_y = 0;

/////////////////////////////////////////////////////
// colors
/////////////////////////////////////////////////////

#define BLACK   0x0000
#define GREEN   0x07E0
#define BLUE    0x001F
#define RED     0xF800
#define WHITE   0xFFFF

/////////////////////////////////////////////////////
// draw pixel
/////////////////////////////////////////////////////

void draw_pixel(int x, int y, uint16_t color)
{
    if(x < 0 || x >= FB_WIDTH)
        return;

    if(y < 0 || y >= FB_HEIGHT)
        return;

    fb[y * FB_WIDTH + x] = color;
}

/////////////////////////////////////////////////////
// draw rect
/////////////////////////////////////////////////////

void draw_rect(int x0, int y0, int w, int h, uint16_t color)
{
    int x;
    int y;

    for(y = y0; y < y0 + h; y++)
    {
        for(x = x0; x < x0 + w; x++)
        {
            draw_pixel(x, y, color);
        }
    }
}

/////////////////////////////////////////////////////
// clear screen
/////////////////////////////////////////////////////

void clear_screen(uint16_t color)
{
    int x;
    int y;

    for(y = 0; y < FB_HEIGHT; y++)
    {
        for(x = 0; x < FB_WIDTH; x++)
        {
            draw_pixel(x, y, color);
        }
    }
}

/////////////////////////////////////////////////////
// draw map border
/////////////////////////////////////////////////////

void draw_map()
{
    int x;
    int y;

    // top and bottom
    for(x = 0; x < GRID_W; x++)
    {
        draw_rect(
            x * CELL_SIZE,
            0,
            CELL_SIZE,
            CELL_SIZE,
            BLUE
        );

        draw_rect(
            x * CELL_SIZE,
            (GRID_H - 1) * CELL_SIZE,
            CELL_SIZE,
            CELL_SIZE,
            BLUE
        );
    }

    // left and right
    for(y = 0; y < GRID_H; y++)
    {
        draw_rect(
            0,
            y * CELL_SIZE,
            CELL_SIZE,
            CELL_SIZE,
            BLUE
        );

        draw_rect(
            (GRID_W - 1) * CELL_SIZE,
            y * CELL_SIZE,
            CELL_SIZE,
            CELL_SIZE,
            BLUE
        );
    }
}

/////////////////////////////////////////////////////
// draw snake
/////////////////////////////////////////////////////

void draw_snake()
{
    int i;

    for(i = 0; i < snake_length; i++)
    {
        draw_rect(
            snake_x[i] * CELL_SIZE,
            snake_y[i] * CELL_SIZE,
            CELL_SIZE,
            CELL_SIZE,
            GREEN
        );
    }
}

/////////////////////////////////////////////////////
// erase tail
/////////////////////////////////////////////////////

void erase_tail(int x, int y)
{
    draw_rect(
        x * CELL_SIZE,
        y * CELL_SIZE,
        CELL_SIZE,
        CELL_SIZE,
        BLACK
    );
}

/////////////////////////////////////////////////////
// move snake
/////////////////////////////////////////////////////

void move_snake()
{
    int i;

    int tail_x;
    int tail_y;

    // record old tail
    tail_x = snake_x[snake_length - 1];
    tail_y = snake_y[snake_length - 1];

    // erase old tail
    erase_tail(tail_x, tail_y);

    // body follow
    for(i = snake_length - 1; i > 0; i--)
    {
        snake_x[i] = snake_x[i - 1];
        snake_y[i] = snake_y[i - 1];
    }

    // head move
    snake_x[0] += dir_x;
    snake_y[0] += dir_y;

    // inside border loop
    if(snake_x[0] >= GRID_W - 1)
        snake_x[0] = 1;

    if(snake_x[0] <= 0)
        snake_x[0] = GRID_W - 2;

    if(snake_y[0] >= GRID_H - 1)
        snake_y[0] = 1;

    if(snake_y[0] <= 0)
        snake_y[0] = GRID_H - 2;
}

/////////////////////////////////////////////////////
// init snake
/////////////////////////////////////////////////////

void init_snake()
{
    snake_x[0] = 10;
    snake_y[0] = 10;

    snake_x[1] = 9;
    snake_y[1] = 10;

    snake_x[2] = 8;
    snake_y[2] = 10;
}

/////////////////////////////////////////////////////
// delay
/////////////////////////////////////////////////////

void delay()
{
    volatile int i;

    for(i = 0; i < 300000; i++)
    {
    }
}

/////////////////////////////////////////////////////
// trap
/////////////////////////////////////////////////////

void trap()
{
    while(1);
}

/////////////////////////////////////////////////////
// main
/////////////////////////////////////////////////////

void main()
{
    bsp_init();

    clear_screen(BLACK);

    draw_map();

    init_snake();

    draw_snake();

    while(1)
    {
        move_snake();

        draw_snake();

        delay();
    }
}
