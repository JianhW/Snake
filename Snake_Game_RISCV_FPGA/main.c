#include <stdint.h>
#include "bsp.h"
#include "gpio.h"

#define FB_BASE 0xF8100000
#define FB_WIDTH 64
#define FB_HEIGHT 64

#define CELL_SIZE 2
#define GRID_W (FB_WIDTH / CELL_SIZE)
#define GRID_H (FB_HEIGHT / CELL_SIZE)

#define MAX_SNAKE_LENGTH 160

#define GPIO_BASE SYSTEM_GPIO_0_IO_CTRL
#define SWITCH_REG_ADDR (FB_BASE + 8192u)
#define BUTTON_UP_MASK    (1u << 0)
#define BUTTON_DOWN_MASK  (1u << 1)
#define BUTTON_LEFT_MASK  (1u << 2)
#define BUTTON_RIGHT_MASK (1u << 3)
#define BUTTON_MASK_ALL   (BUTTON_RIGHT_MASK | BUTTON_LEFT_MASK | BUTTON_UP_MASK | BUTTON_DOWN_MASK)

#define SWITCH_SPEED0_MASK (1u << 0)
#define SWITCH_SPEED1_MASK (1u << 1)
#define SWITCH_WRAP_MASK   (1u << 2)
#define SWITCH_PAUSE_MASK  (1u << 3)
#define SWITCH_THEME_MASK  (1u << 4)

#define BUZZER_MASK (1u << 3)

#define INPUT_ACTIVE_HIGH 1

#define COLOR_BLACK       0x0000
#define COLOR_WHITE       0xFFFF
#define COLOR_NAVY        0x08A6
#define COLOR_TEAL        0x0451
#define COLOR_SKY         0x869F
#define COLOR_MINT        0x6671
#define COLOR_LIME        0x5FE0
#define COLOR_LEAF        0x2E26
#define COLOR_GOLD        0xFEA0
#define COLOR_ORANGE      0xFC60
#define COLOR_RED         0xF800
#define COLOR_SHADOW      0x18C3
#define COLOR_CYAN        0x07FF
#define COLOR_BLUE        0x041F
#define COLOR_PINK        0xF81F
#define COLOR_SILVER      0xC638
typedef enum
{
    GAME_STATE_READY = 0,
    GAME_STATE_PLAYING = 1,
    GAME_STATE_OVER = 2,
    GAME_STATE_PAUSED = 3
} game_state_t;

typedef struct
{
    int x;
    int y;
} point_t;

typedef struct
{
    int dx;
    int dy;
} direction_t;

static volatile uint16_t *const fb = (volatile uint16_t *)FB_BASE;
static volatile uint32_t *const switch_reg = (volatile uint32_t *)SWITCH_REG_ADDR;

static point_t snake[MAX_SNAKE_LENGTH];
static int snake_length;
static direction_t current_dir;
static direction_t next_dir;
static point_t food;
static game_state_t game_state;
static uint32_t rng_state = 0x13579BDFu;
static uint32_t score;
static uint32_t best_score;
static uint32_t prev_buttons;
static uint32_t switch_state;
static uint32_t prev_switch_state;
static uint32_t move_delay_loops;
static uint32_t buzzer_ticks;

static const uint8_t glyph_space[7] = {0, 0, 0, 0, 0, 0, 0};
static const uint8_t glyph_a[7] = {14, 17, 17, 31, 17, 17, 17};
static const uint8_t glyph_b[7] = {30, 17, 17, 30, 17, 17, 30};
static const uint8_t glyph_c[7] = {14, 17, 16, 16, 16, 17, 14};
static const uint8_t glyph_e[7] = {31, 16, 16, 30, 16, 16, 31};
static const uint8_t glyph_g[7] = {14, 17, 16, 23, 17, 17, 15};
static const uint8_t glyph_k[7] = {17, 18, 20, 24, 20, 18, 17};
static const uint8_t glyph_m[7] = {17, 27, 21, 17, 17, 17, 17};
static const uint8_t glyph_n[7] = {17, 25, 21, 19, 17, 17, 17};
static const uint8_t glyph_o[7] = {14, 17, 17, 17, 17, 17, 14};
static const uint8_t glyph_p[7] = {30, 17, 17, 30, 16, 16, 16};
static const uint8_t glyph_r[7] = {30, 17, 17, 30, 20, 18, 17};
static const uint8_t glyph_s[7] = {15, 16, 16, 14, 1, 1, 30};
static const uint8_t glyph_t[7] = {31, 4, 4, 4, 4, 4, 4};
static const uint8_t glyph_u[7] = {17, 17, 17, 17, 17, 17, 14};
static const uint8_t glyph_v[7] = {17, 17, 17, 17, 17, 10, 4};
static const uint8_t glyph_y[7] = {17, 17, 10, 4, 4, 4, 4};

static const uint8_t digit_0[7] = {14, 17, 19, 21, 25, 17, 14};
static const uint8_t digit_1[7] = {4, 12, 4, 4, 4, 4, 14};
static const uint8_t digit_2[7] = {14, 17, 1, 2, 4, 8, 31};
static const uint8_t digit_3[7] = {30, 1, 1, 14, 1, 1, 30};
static const uint8_t digit_4[7] = {2, 6, 10, 18, 31, 2, 2};
static const uint8_t digit_5[7] = {31, 16, 16, 30, 1, 1, 30};
static const uint8_t digit_6[7] = {14, 16, 16, 30, 17, 17, 14};
static const uint8_t digit_7[7] = {31, 1, 2, 4, 8, 8, 8};
static const uint8_t digit_8[7] = {14, 17, 17, 14, 17, 17, 14};
static const uint8_t digit_9[7] = {14, 17, 17, 15, 1, 1, 14};

static void draw_pixel(int x, int y, uint16_t color)
{
    if(x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT)
        return;

    fb[y * FB_WIDTH + x] = color;
}

static void fill_rect(int x0, int y0, int w, int h, uint16_t color)
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

static void draw_rect_outline(int x0, int y0, int w, int h, uint16_t color)
{
    fill_rect(x0, y0, w, 1, color);
    fill_rect(x0, y0 + h - 1, w, 1, color);
    fill_rect(x0, y0, 1, h, color);
    fill_rect(x0 + w - 1, y0, 1, h, color);
}

static void clear_screen(uint16_t color)
{
    fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, color);
}

static uint16_t read_buttons_raw(void)
{
    uint32_t value = gpio_getInput(GPIO_BASE);

#if INPUT_ACTIVE_HIGH
    return (uint16_t)(value & BUTTON_MASK_ALL);
#else
    return (uint16_t)((~value) & BUTTON_MASK_ALL);
#endif
}

static uint16_t read_buttons_pressed(void)
{
    uint16_t current = read_buttons_raw();
    uint16_t edge = (uint16_t)(current & (uint16_t)(~prev_buttons));

    prev_buttons = current;
    return edge;
}

static uint32_t read_switches(void)
{
    return (*switch_reg) & 0x1Fu;
}

static void buzzer_on(void)
{
    gpio_setOutputEnable(GPIO_BASE, BUZZER_MASK);
    gpio_setOutput(GPIO_BASE, BUZZER_MASK);
}

static void buzzer_off(void)
{
    gpio_setOutputEnable(GPIO_BASE, BUZZER_MASK);
    gpio_setOutput(GPIO_BASE, 0u);
}

static void trigger_buzzer(uint32_t ticks)
{
    buzzer_ticks = ticks;
    buzzer_on();
}

static void service_buzzer(void)
{
    if(buzzer_ticks > 0u)
    {
        buzzer_ticks--;
        if(buzzer_ticks == 0u)
            buzzer_off();
    }
}

static uint16_t border_primary_color(void)
{
    return (switch_state & SWITCH_THEME_MASK) ? COLOR_CYAN : COLOR_GOLD;
}

static uint16_t border_secondary_color(void)
{
    return (switch_state & SWITCH_THEME_MASK) ? COLOR_BLUE : COLOR_ORANGE;
}

static uint16_t snake_head_color(void)
{
    return (switch_state & SWITCH_THEME_MASK) ? COLOR_PINK : COLOR_LIME;
}

static uint16_t snake_body_color(void)
{
    return (switch_state & SWITCH_THEME_MASK) ? COLOR_CYAN : COLOR_LEAF;
}

static uint16_t snake_shine_color(void)
{
    return (switch_state & SWITCH_THEME_MASK) ? COLOR_SILVER : COLOR_WHITE;
}

static uint16_t food_main_color(void)
{
    return (switch_state & SWITCH_THEME_MASK) ? COLOR_BLUE : COLOR_RED;
}

static uint16_t food_highlight_color(void)
{
    return (switch_state & SWITCH_THEME_MASK) ? COLOR_CYAN : COLOR_WHITE;
}

static uint16_t score_accent_color(void)
{
    if((switch_state & (SWITCH_SPEED0_MASK | SWITCH_SPEED1_MASK)) == SWITCH_SPEED0_MASK)
        return COLOR_MINT;
    else if((switch_state & (SWITCH_SPEED0_MASK | SWITCH_SPEED1_MASK)) == SWITCH_SPEED1_MASK)
        return COLOR_SKY;
    else if((switch_state & (SWITCH_SPEED0_MASK | SWITCH_SPEED1_MASK)) == (SWITCH_SPEED0_MASK | SWITCH_SPEED1_MASK))
        return COLOR_RED;

    return (switch_state & SWITCH_THEME_MASK) ? COLOR_CYAN : COLOR_GOLD;
}

static const uint8_t *get_glyph(char c)
{
    switch(c)
    {
        case 'A': return glyph_a;
        case 'B': return glyph_b;
        case 'C': return glyph_c;
        case 'E': return glyph_e;
        case 'G': return glyph_g;
        case 'K': return glyph_k;
        case 'M': return glyph_m;
        case 'N': return glyph_n;
        case 'O': return glyph_o;
        case 'P': return glyph_p;
        case 'R': return glyph_r;
        case 'S': return glyph_s;
        case 'T': return glyph_t;
        case 'U': return glyph_u;
        case 'V': return glyph_v;
        case 'Y': return glyph_y;
        case '0': return digit_0;
        case '1': return digit_1;
        case '2': return digit_2;
        case '3': return digit_3;
        case '4': return digit_4;
        case '5': return digit_5;
        case '6': return digit_6;
        case '7': return digit_7;
        case '8': return digit_8;
        case '9': return digit_9;
        case ' ': return glyph_space;
        default:  return glyph_space;
    }
}

static void draw_glyph(int x, int y, char c, uint16_t color, int scale)
{
    int row;
    int col;
    const uint8_t *glyph = get_glyph(c);

    for(row = 0; row < 7; row++)
    {
        for(col = 0; col < 5; col++)
        {
            if(glyph[row] & (1u << (4 - col)))
            {
                fill_rect(x + col * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

static void draw_text(int x, int y, const char *text, uint16_t color, int scale)
{
    int cursor = 0;

    while(text[cursor] != '\0')
    {
        draw_glyph(x + cursor * (6 * scale), y, text[cursor], color, scale);
        cursor++;
    }
}

static int text_width(const char *text, int scale)
{
    int len = 0;

    while(text[len] != '\0')
    {
        len++;
    }

    if(len == 0)
        return 0;

    return len * 6 * scale - scale;
}

static void draw_center_text(int y, const char *text, uint16_t color, int scale)
{
    int width = text_width(text, scale);
    int x = (FB_WIDTH - width) / 2;

    draw_text(x, y, text, color, scale);
}

static uint16_t mix565(uint16_t a, uint16_t b)
{
    uint16_t r = (uint16_t)((((a >> 11) & 0x1F) + ((b >> 11) & 0x1F)) >> 1);
    uint16_t g = (uint16_t)((((a >> 5) & 0x3F) + ((b >> 5) & 0x3F)) >> 1);
    uint16_t bl = (uint16_t)((((a >> 0) & 0x1F) + ((b >> 0) & 0x1F)) >> 1);

    return (uint16_t)((r << 11) | (g << 5) | bl);
}

static void format_u32(uint32_t value, char *out)
{
    char buffer[10];
    int count = 0;
    int i;

    do
    {
        buffer[count++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    while(value != 0u);

    for(i = 0; i < count; i++)
    {
        out[i] = buffer[count - 1 - i];
    }

    out[count] = '\0';
}

static uint16_t board_cell_color(int grid_x, int grid_y)
{
    if(grid_x == 0 || grid_y == 0 || grid_x == GRID_W - 1 || grid_y == GRID_H - 1)
    {
        return (((grid_x + grid_y) & 1) == 0) ? border_primary_color() : border_secondary_color();
    }

    return COLOR_BLACK;
}

static void draw_arena(void)
{
    int x;
    int y;

    fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, COLOR_BLACK);

    for(y = 0; y < GRID_H; y++)
    {
        for(x = 0; x < GRID_W; x++)
        {
            int px = x * CELL_SIZE;
            int py = y * CELL_SIZE;
            uint16_t cell_color = board_cell_color(x, y);

            fill_rect(px, py, CELL_SIZE, CELL_SIZE, cell_color);
        }
    }

    draw_rect_outline(0, 0, FB_WIDTH, FB_HEIGHT, COLOR_WHITE);
}

static void redraw_board_cell(int grid_x, int grid_y)
{
    fill_rect(grid_x * CELL_SIZE, grid_y * CELL_SIZE, CELL_SIZE, CELL_SIZE, board_cell_color(grid_x, grid_y));
}

static void draw_food(void)
{
    int px = food.x * CELL_SIZE;
    int py = food.y * CELL_SIZE;
    uint16_t food_color = food_main_color();

    fill_rect(px, py, CELL_SIZE, CELL_SIZE, food_color);
    fill_rect(px, py, 1, 1, food_highlight_color());
}

static void draw_snake_segment(point_t segment, int is_head)
{
    int px = segment.x * CELL_SIZE;
    int py = segment.y * CELL_SIZE;
    uint16_t body_color = is_head ? snake_head_color() : snake_body_color();

    fill_rect(px, py, CELL_SIZE, CELL_SIZE, body_color);

    if(is_head)
    {
        draw_pixel(px, py, snake_shine_color());
    }
    else
    {
        draw_pixel(px, py, mix565(body_color, snake_shine_color()));
    }
}

static void draw_snake(void)
{
    int i;

    for(i = snake_length - 1; i >= 0; i--)
    {
        draw_snake_segment(snake[i], i == 0);
    }
}

static void draw_score_banner(void)
{
    char score_text[10];
    uint16_t score_color;

    format_u32(score, score_text);
    score_color = score_accent_color();

    fill_rect(2, 2, 32, 8, COLOR_SHADOW);
    draw_text(4, 3, "S", COLOR_WHITE, 1);
    draw_text(10, 3, score_text, score_color, 1);
}

static void render_game(void)
{
    draw_arena();
    draw_food();
    draw_snake();
    draw_score_banner();
}

static uint32_t next_random(void)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}

static int point_on_snake(int x, int y)
{
    int i;

    for(i = 0; i < snake_length; i++)
    {
        if(snake[i].x == x && snake[i].y == y)
            return 1;
    }

    return 0;
}

static void spawn_food(void)
{
    int x;
    int y;

    do
    {
        x = 1 + (int)(next_random() % (uint32_t)(GRID_W - 2));
        y = 1 + (int)(next_random() % (uint32_t)(GRID_H - 2));
    }
    while(point_on_snake(x, y));

    food.x = x;
    food.y = y;
}

static void update_mode_from_switches(void)
{
    uint32_t speed_sel;

    switch_state = read_switches();
    speed_sel = switch_state & (SWITCH_SPEED0_MASK | SWITCH_SPEED1_MASK);

    switch(speed_sel)
    {
        case 0u:
            move_delay_loops = 240000u;
            break;
        case SWITCH_SPEED0_MASK:
            move_delay_loops = 180000u;
            break;
        case SWITCH_SPEED1_MASK:
            move_delay_loops = 130000u;
            break;
        default:
            move_delay_loops = 90000u;
            break;
    }
}

static int decode_button_direction(uint16_t pressed, direction_t *direction)
{
    if(pressed & BUTTON_RIGHT_MASK)
    {
        direction->dx = 1;
        direction->dy = 0;
        return 1;
    }

    if(pressed & BUTTON_LEFT_MASK)
    {
        direction->dx = -1;
        direction->dy = 0;
        return 1;
    }

    if(pressed & BUTTON_UP_MASK)
    {
        direction->dx = 0;
        direction->dy = -1;
        return 1;
    }

    if(pressed & BUTTON_DOWN_MASK)
    {
        direction->dx = 0;
        direction->dy = 1;
        return 1;
    }

    return 0;
}

static void reset_snake(direction_t initial_dir)
{
    int i;

    snake_length = 4;

    snake[0].x = GRID_W / 2;
    snake[0].y = GRID_H / 2;

    for(i = 1; i < snake_length; i++)
    {
        snake[i].x = snake[0].x - initial_dir.dx * i;
        snake[i].y = snake[0].y - initial_dir.dy * i;
    }

    current_dir = initial_dir;
    next_dir = current_dir;
    score = 0;
}

static void start_new_game(direction_t initial_dir)
{
    reset_snake(initial_dir);
    spawn_food();
    game_state = GAME_STATE_PLAYING;
    buzzer_off();
    render_game();
}

static int direction_is_opposite(direction_t a, direction_t b)
{
    return (a.dx == -b.dx) && (a.dy == -b.dy);
}

static void update_direction_from_buttons(uint16_t pressed)
{
    direction_t requested;

    if(decode_button_direction(pressed, &requested) && !direction_is_opposite(requested, current_dir))
        next_dir = requested;
}

static int snake_hits_body(int x, int y, int ignore_tail)
{
    int i;
    int limit = snake_length - (ignore_tail ? 1 : 0);

    for(i = 0; i < limit; i++)
    {
        if(snake[i].x == x && snake[i].y == y)
            return 1;
    }

    return 0;
}

static void show_ready_screen(void)
{
    clear_screen(COLOR_BLACK);
    draw_center_text(14, "SNAKE", COLOR_LIME, 2);
    draw_center_text(30, "PRESS", COLOR_WHITE, 1);
    draw_center_text(38, "ANY", COLOR_GOLD, 1);
    draw_center_text(46, "KEY", COLOR_GOLD, 1);
}

static void show_game_over_screen(void)
{
    char best_text[10];

    clear_screen(COLOR_BLACK);
    draw_center_text(16, "GAME", COLOR_RED, 2);
    draw_center_text(32, "OVER", COLOR_WHITE, 2);
    draw_center_text(48, "BEST", COLOR_GOLD, 1);

    format_u32(best_score, best_text);
    draw_center_text(56, best_text, COLOR_SKY, 1);
}

static void show_pause_screen(void)
{
    clear_screen(COLOR_BLACK);
    draw_center_text(16, "PAUSE", border_primary_color(), 2);
    draw_center_text(40, "SW9", COLOR_WHITE, 1);
    draw_center_text(48, "RUN", score_accent_color(), 1);
}

static void finish_game(void)
{
    if(score > best_score)
        best_score = score;

    game_state = GAME_STATE_OVER;
    show_game_over_screen();
}

static void step_game(void)
{
    point_t new_head;
    point_t old_head;
    point_t old_tail;
    int grow = 0;
    int i;

    current_dir = next_dir;
    old_head = snake[0];
    old_tail = snake[snake_length - 1];

    new_head.x = snake[0].x + current_dir.dx;
    new_head.y = snake[0].y + current_dir.dy;

    if((switch_state & SWITCH_WRAP_MASK) == 0u)
    {
        if(new_head.x <= 0 || new_head.x >= GRID_W - 1 || new_head.y <= 0 || new_head.y >= GRID_H - 1)
        {
            finish_game();
            return;
        }
    }
    else
    {
        if(new_head.x <= 0)
            new_head.x = GRID_W - 2;
        else if(new_head.x >= GRID_W - 1)
            new_head.x = 1;

        if(new_head.y <= 0)
            new_head.y = GRID_H - 2;
        else if(new_head.y >= GRID_H - 1)
            new_head.y = 1;
    }

    if(new_head.x == food.x && new_head.y == food.y)
        grow = 1;

    if(snake_hits_body(new_head.x, new_head.y, !grow))
    {
        finish_game();
        return;
    }

    if(grow && snake_length < MAX_SNAKE_LENGTH)
        snake_length++;

    for(i = snake_length - 1; i > 0; i--)
    {
        snake[i] = snake[i - 1];
    }

    snake[0] = new_head;

    if(grow)
    {
        score++;
        spawn_food();
        draw_food();
        draw_score_banner();
        trigger_buzzer(200u);
    }

    if(!grow)
        redraw_board_cell(old_tail.x, old_tail.y);

    draw_snake_segment(old_head, 0);
    draw_snake_segment(new_head, 1);
}

static void game_delay(uint32_t loops)
{
    volatile uint32_t i;

    for(i = 0; i < loops; i++)
    {
        if((i & 2047u) == 0u)
            service_buzzer();
    }
}

void trap(void)
{
    while(1)
    {
    }
}

void main(void)
{
    direction_t start_dir;

    bsp_init();
    gpio_setOutputEnable(GPIO_BASE, BUZZER_MASK);
    buzzer_off();
    update_mode_from_switches();
    prev_switch_state = switch_state;

    prev_buttons = read_buttons_raw();
    game_state = GAME_STATE_READY;
    show_ready_screen();

    while(1)
    {
        uint16_t pressed = read_buttons_pressed();
        update_mode_from_switches();
        service_buzzer();

        if(game_state == GAME_STATE_PLAYING && switch_state != prev_switch_state)
        {
            if(switch_state & SWITCH_PAUSE_MASK)
            {
                game_state = GAME_STATE_PAUSED;
                show_pause_screen();
            }
            else
            {
                game_state = GAME_STATE_PLAYING;
                render_game();
            }
        }
        prev_switch_state = switch_state;

        if(game_state == GAME_STATE_READY)
        {
            if(decode_button_direction(pressed, &start_dir))
            {
                start_new_game(start_dir);
            }

            game_delay(90000);
            continue;
        }

        if(game_state == GAME_STATE_OVER)
        {
            if(decode_button_direction(pressed, &start_dir))
            {
                start_new_game(start_dir);
            }

            game_delay(90000);
            continue;
        }

        if(game_state == GAME_STATE_PAUSED)
        {
            if((switch_state & SWITCH_PAUSE_MASK) == 0u)
            {
                game_state = GAME_STATE_PLAYING;
                render_game();
            }

            game_delay(90000);
            continue;
        }

        if(switch_state & SWITCH_PAUSE_MASK)
        {
            game_state = GAME_STATE_PAUSED;
            show_pause_screen();
            game_delay(90000);
            continue;
        }

        update_direction_from_buttons(pressed);
        step_game();
        game_delay(move_delay_loops);
    }
}
