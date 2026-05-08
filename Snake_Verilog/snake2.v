// snake第二版，五档速度调节
module VGA_test
(
  input clk,
  input rst_n,
  input BUT4,
  input BUT3,
  input BUT2,
  input BUT1,
  input sw5,
  input sw6,
  input sw7,
  input sw8,
  input sw9,

  output [4:0] VGA_B,
  output [4:0] VGA_G,
  output VGA_HS,
  output [4:0] VGA_R,
  output VGA_VS,
  output buzzer
);

// ================= Pixel clock =================
reg pixel_clk;
always @(posedge clk or negedge rst_n) begin
    if (!rst_n) pixel_clk <= 1'b0;
    else pixel_clk <= ~pixel_clk;
end

// ================= VGA 640x480 =================
parameter H_VISIBLE = 640;
parameter H_FRONT   = 16;
parameter H_SYNC    = 96;
parameter H_BACK    = 48;
parameter H_TOTAL   = 800;
parameter V_VISIBLE = 480;
parameter V_FRONT   = 10;
parameter V_SYNC    = 2;
parameter V_BACK    = 33;
parameter V_TOTAL   = 525;

reg [9:0] h_cnt;
reg [9:0] v_cnt;

always @(posedge pixel_clk or negedge rst_n) begin
    if (!rst_n) h_cnt <= 10'd0;
    else if (h_cnt == H_TOTAL - 1) h_cnt <= 10'd0;
    else h_cnt <= h_cnt + 10'd1;
end

always @(posedge pixel_clk or negedge rst_n) begin
    if (!rst_n) v_cnt <= 10'd0;
    else if (h_cnt == H_TOTAL - 1) begin
        if (v_cnt == V_TOTAL - 1) v_cnt <= 10'd0;
        else v_cnt <= v_cnt + 10'd1;
    end
end

assign VGA_HS = ~((h_cnt >= H_VISIBLE + H_FRONT) &&
                  (h_cnt <  H_VISIBLE + H_FRONT + H_SYNC));
assign VGA_VS = ~((v_cnt >= V_VISIBLE + V_FRONT) &&
                  (v_cnt <  V_VISIBLE + V_FRONT + V_SYNC));

wire active = (h_cnt < H_VISIBLE) && (v_cnt < V_VISIBLE);

// ================= Grid =================
parameter GRID_W  = 40;
parameter GRID_H  = 30;
parameter MAX_LEN = 16;

wire [5:0] grid_x = h_cnt[9:4];
wire [4:0] grid_y = v_cnt[8:4];
wire grid_line = (h_cnt[3:0] == 4'd0) || (v_cnt[3:0] == 4'd0);

// ================= Game tick =================
reg [23:0] game_cnt;
reg [23:0] game_tick_limit;
wire game_tick = (game_cnt >= game_tick_limit);

always @(*) begin
    if (sw9) game_tick_limit = 24'd1_500_000;
    else if (sw8) game_tick_limit = 24'd2_500_000;
    else if (sw7) game_tick_limit = 24'd3_500_000;
    else if (sw6) game_tick_limit = 24'd5_000_000;
    else if (sw5) game_tick_limit = 24'd7_000_000;
    else game_tick_limit = 24'd5_000_000;
end

always @(posedge clk or negedge rst_n) begin
    if (!rst_n) game_cnt <= 24'd0;
    else if (game_tick) game_cnt <= 24'd0;
    else game_cnt <= game_cnt + 24'd1;
end

// ================= Key debounce =================
reg [3:0] key_sync0;
reg [3:0] key_sync1;
reg [3:0] key_state;
reg [3:0] key_state_d;
reg [15:0] key_cnt [0:3];
integer ki;

wire [3:0] key_raw = {BUT4, BUT3, BUT2, BUT1};

always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
        key_sync0 <= 4'b0000;
        key_sync1 <= 4'b0000;
        key_state <= 4'b0000;
        key_state_d <= 4'b0000;
        for (ki = 0; ki < 4; ki = ki + 1) key_cnt[ki] <= 16'd0;
    end else begin
        key_sync0 <= key_raw;
        key_sync1 <= key_sync0;
        key_state_d <= key_state;

        for (ki = 0; ki < 4; ki = ki + 1) begin
            if (key_sync1[ki] == key_state[ki]) begin
                key_cnt[ki] <= 16'd0;
            end else if (key_cnt[ki] == 16'hffff) begin
                key_state[ki] <= key_sync1[ki];
                key_cnt[ki] <= 16'd0;
            end else begin
                key_cnt[ki] <= key_cnt[ki] + 16'd1;
            end
        end
    end
end

wire key_up_pulse    = key_state[0] & ~key_state_d[0]; // BUT1
wire key_down_pulse  = key_state[1] & ~key_state_d[1]; // BUT2
wire key_left_pulse  = key_state[2] & ~key_state_d[2]; // BUT3
wire key_right_pulse = key_state[3] & ~key_state_d[3]; // BUT4
wire any_key_pulse = key_up_pulse || key_down_pulse || key_left_pulse || key_right_pulse;

// ================= Snake state =================
localparam DIR_RIGHT = 2'd0;
localparam DIR_DOWN  = 2'd1;
localparam DIR_LEFT  = 2'd2;
localparam DIR_UP    = 2'd3;

reg [5:0] snake_x [0:15];
reg [4:0] snake_y [0:15];
reg [4:0] len;
reg [1:0] dir;
reg [1:0] next_dir;
reg dir_pending;
reg game_started;
reg game_over;

reg [5:0] food_x;
reg [4:0] food_y;
reg [15:0] lfsr;

reg [5:0] next_x;
reg [4:0] next_y;
reg hit_wall;
reg hit_self;
reg will_eat;
reg food_on_snake;
reg [5:0] food_x_candidate;
reg [4:0] food_y_candidate;
reg [5:0] food_x_alt;
reg [4:0] food_y_alt;
integer i;

// ================= Buzzer =================
parameter BUZZ_HALF_PERIOD = 16'd25_000;
parameter BUZZ_TIME        = 24'd6_000_000;

reg buzzer_en;
reg buzzer_wave;
reg [15:0] buzzer_div;
reg [23:0] buzzer_cnt;

wire start_key_event = !game_started && any_key_pulse;
wire restart_key_event = game_over && any_key_pulse;
wire crash_event = game_started && !game_over && game_tick && (hit_wall || hit_self);
wire buzzer_trig = start_key_event || restart_key_event || crash_event;

always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
        buzzer_en <= 1'b0;
        buzzer_wave <= 1'b0;
        buzzer_div <= 16'd0;
        buzzer_cnt <= 24'd0;
    end else if (buzzer_trig) begin
        buzzer_en <= 1'b1;
        buzzer_wave <= 1'b0;
        buzzer_div <= 16'd0;
        buzzer_cnt <= BUZZ_TIME;
    end else if (buzzer_en) begin
        if (buzzer_cnt == 24'd0) begin
            buzzer_en <= 1'b0;
            buzzer_wave <= 1'b0;
        end else begin
            buzzer_cnt <= buzzer_cnt - 24'd1;
            if (buzzer_div == BUZZ_HALF_PERIOD) begin
                buzzer_div <= 16'd0;
                buzzer_wave <= ~buzzer_wave;
            end else begin
                buzzer_div <= buzzer_div + 16'd1;
            end
        end
    end
end

assign buzzer = buzzer_en ? buzzer_wave : 1'b0;

always @(*) begin
    next_x = snake_x[0];
    next_y = snake_y[0];
    hit_wall = 1'b0;

    case (next_dir)
        DIR_RIGHT: begin
            if (snake_x[0] == GRID_W - 1) hit_wall = 1'b1;
            else next_x = snake_x[0] + 6'd1;
        end
        DIR_DOWN: begin
            if (snake_y[0] == GRID_H - 1) hit_wall = 1'b1;
            else next_y = snake_y[0] + 5'd1;
        end
        DIR_LEFT: begin
            if (snake_x[0] == 6'd0) hit_wall = 1'b1;
            else next_x = snake_x[0] - 6'd1;
        end
        DIR_UP: begin
            if (snake_y[0] == 5'd0) hit_wall = 1'b1;
            else next_y = snake_y[0] - 5'd1;
        end
    endcase

    will_eat = (next_x == food_x) && (next_y == food_y);

    hit_self = 1'b0;
    for (i = 1; i < MAX_LEN; i = i + 1) begin
        if (will_eat) begin
            if (i < len && next_x == snake_x[i] && next_y == snake_y[i]) hit_self = 1'b1;
        end else begin
            if (i < len - 1 && next_x == snake_x[i] && next_y == snake_y[i]) hit_self = 1'b1;
        end
    end

    food_x_candidate = (lfsr[5:0] >= GRID_W) ? (lfsr[5:0] - GRID_W) : lfsr[5:0];
    food_y_candidate = (lfsr[12:8] >= GRID_H) ? (lfsr[12:8] - GRID_H) : lfsr[12:8];
    food_x_alt = (food_x_candidate >= 6'd27) ? (food_x_candidate - 6'd27) : (food_x_candidate + 6'd13);
    food_y_alt = (food_y_candidate >= 5'd19) ? (food_y_candidate - 5'd19) : (food_y_candidate + 5'd11);

    food_on_snake = (food_x_candidate == next_x) && (food_y_candidate == next_y);
    for (i = 0; i < MAX_LEN; i = i + 1) begin
        if (i < len && food_x_candidate == snake_x[i] && food_y_candidate == snake_y[i]) food_on_snake = 1'b1;
    end
end

always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
        len <= 5'd4;
        dir <= DIR_RIGHT;
        next_dir <= DIR_RIGHT;
        dir_pending <= 1'b0;
        game_started <= 1'b0;
        game_over <= 1'b0;
        snake_x[0] <= 6'd10; snake_y[0] <= 5'd10;
        snake_x[1] <= 6'd9;  snake_y[1] <= 5'd10;
        snake_x[2] <= 6'd8;  snake_y[2] <= 5'd10;
        snake_x[3] <= 6'd7;  snake_y[3] <= 5'd10;
        for (i = 4; i < MAX_LEN; i = i + 1) begin
            snake_x[i] <= 6'd0;
            snake_y[i] <= 5'd0;
        end
        food_x <= 6'd25;
        food_y <= 5'd12;
        lfsr <= 16'hACE1;
    end else begin
        lfsr <= {lfsr[14:0], lfsr[15] ^ lfsr[13] ^ lfsr[12] ^ lfsr[10]};

        if (!game_started) begin
            if (any_key_pulse) begin
                game_started <= 1'b1;
            end
        end else if (game_over) begin
            if (any_key_pulse) begin
                len <= 5'd4;
                dir <= DIR_RIGHT;
                next_dir <= DIR_RIGHT;
                dir_pending <= 1'b0;
                game_started <= 1'b1;
                game_over <= 1'b0;
                snake_x[0] <= 6'd10; snake_y[0] <= 5'd10;
                snake_x[1] <= 6'd9;  snake_y[1] <= 5'd10;
                snake_x[2] <= 6'd8;  snake_y[2] <= 5'd10;
                snake_x[3] <= 6'd7;  snake_y[3] <= 5'd10;
                for (i = 4; i < MAX_LEN; i = i + 1) begin
                    snake_x[i] <= 6'd0;
                    snake_y[i] <= 5'd0;
                end
                food_x <= 6'd25;
                food_y <= 5'd12;
            end
        end else begin
            if (!dir_pending) begin
                if (key_up_pulse && dir != DIR_DOWN) begin
                    next_dir <= DIR_UP;
                    dir_pending <= 1'b1;
                end else if (key_down_pulse && dir != DIR_UP) begin
                    next_dir <= DIR_DOWN;
                    dir_pending <= 1'b1;
                end else if (key_left_pulse && dir != DIR_RIGHT) begin
                    next_dir <= DIR_LEFT;
                    dir_pending <= 1'b1;
                end else if (key_right_pulse && dir != DIR_LEFT) begin
                    next_dir <= DIR_RIGHT;
                    dir_pending <= 1'b1;
                end
            end

            if (game_tick) begin
                dir <= next_dir;
                dir_pending <= 1'b0;

                if (hit_wall || hit_self) begin
                    game_over <= 1'b1;
                end else begin
                    for (i = MAX_LEN - 1; i > 0; i = i - 1) begin
                        if ((will_eat && i <= len) || (!will_eat && i < len)) begin
                            snake_x[i] <= snake_x[i - 1];
                            snake_y[i] <= snake_y[i - 1];
                        end
                    end

                    snake_x[0] <= next_x;
                    snake_y[0] <= next_y;

                    if (will_eat) begin
                        if (len < MAX_LEN) len <= len + 5'd1;

                        if (food_on_snake) begin
                            food_x <= food_x_alt;
                            food_y <= food_y_alt;
                        end else begin
                            food_x <= food_x_candidate;
                            food_y <= food_y_candidate;
                        end
                    end
                end
            end
        end
    end
end

// ================= Pixel test =================
reg is_snake;
integer j;

always @(*) begin
    is_snake = 1'b0;
    for (j = 0; j < MAX_LEN; j = j + 1) begin
        if (j < len && grid_x == snake_x[j] && grid_y == snake_y[j]) is_snake = 1'b1;
    end
end

wire is_food = (grid_x == food_x) && (grid_y == food_y);

// ================= Start / game-over text =================
parameter TEXT_SCALE = 4;
parameter TEXT_CELL_W = 24;
parameter TEXT_W_START = 240;
parameter TEXT_W_OVER = 216;
parameter TEXT_H = 28;
parameter TEXT_START_X = 200;
parameter TEXT_OVER_X = 212;
parameter TEXT_Y = 226;

function [4:0] font_row;
    input [7:0] ch;
    input [2:0] y;
    begin
        font_row = 5'b00000;
        case (ch)
            "G": begin
                case (y)
                    3'd0: font_row = 5'b01110;
                    3'd1: font_row = 5'b10000;
                    3'd2: font_row = 5'b10000;
                    3'd3: font_row = 5'b10111;
                    3'd4: font_row = 5'b10001;
                    3'd5: font_row = 5'b10001;
                    3'd6: font_row = 5'b01110;
                endcase
            end
            "S": begin
                case (y)
                    3'd0: font_row = 5'b01111;
                    3'd1: font_row = 5'b10000;
                    3'd2: font_row = 5'b10000;
                    3'd3: font_row = 5'b01110;
                    3'd4: font_row = 5'b00001;
                    3'd5: font_row = 5'b00001;
                    3'd6: font_row = 5'b11110;
                endcase
            end
            "s": begin
                case (y)
                    3'd0: font_row = 5'b00000;
                    3'd1: font_row = 5'b00000;
                    3'd2: font_row = 5'b01111;
                    3'd3: font_row = 5'b10000;
                    3'd4: font_row = 5'b01110;
                    3'd5: font_row = 5'b00001;
                    3'd6: font_row = 5'b11110;
                endcase
            end
            "O": begin
                case (y)
                    3'd0: font_row = 5'b01110;
                    3'd1: font_row = 5'b10001;
                    3'd2: font_row = 5'b10001;
                    3'd3: font_row = 5'b10001;
                    3'd4: font_row = 5'b10001;
                    3'd5: font_row = 5'b10001;
                    3'd6: font_row = 5'b01110;
                endcase
            end
            "a": begin
                case (y)
                    3'd0: font_row = 5'b00000;
                    3'd1: font_row = 5'b00000;
                    3'd2: font_row = 5'b01110;
                    3'd3: font_row = 5'b00001;
                    3'd4: font_row = 5'b01111;
                    3'd5: font_row = 5'b10001;
                    3'd6: font_row = 5'b01111;
                endcase
            end
            "e": begin
                case (y)
                    3'd0: font_row = 5'b00000;
                    3'd1: font_row = 5'b00000;
                    3'd2: font_row = 5'b01110;
                    3'd3: font_row = 5'b10001;
                    3'd4: font_row = 5'b11111;
                    3'd5: font_row = 5'b10000;
                    3'd6: font_row = 5'b01110;
                endcase
            end
            "m": begin
                case (y)
                    3'd0: font_row = 5'b00000;
                    3'd1: font_row = 5'b00000;
                    3'd2: font_row = 5'b11010;
                    3'd3: font_row = 5'b10101;
                    3'd4: font_row = 5'b10101;
                    3'd5: font_row = 5'b10101;
                    3'd6: font_row = 5'b10101;
                endcase
            end
            "r": begin
                case (y)
                    3'd0: font_row = 5'b00000;
                    3'd1: font_row = 5'b00000;
                    3'd2: font_row = 5'b10110;
                    3'd3: font_row = 5'b11001;
                    3'd4: font_row = 5'b10000;
                    3'd5: font_row = 5'b10000;
                    3'd6: font_row = 5'b10000;
                endcase
            end
            "t": begin
                case (y)
                    3'd0: font_row = 5'b00100;
                    3'd1: font_row = 5'b00100;
                    3'd2: font_row = 5'b11111;
                    3'd3: font_row = 5'b00100;
                    3'd4: font_row = 5'b00100;
                    3'd5: font_row = 5'b00101;
                    3'd6: font_row = 5'b00010;
                endcase
            end
            "v": begin
                case (y)
                    3'd0: font_row = 5'b00000;
                    3'd1: font_row = 5'b00000;
                    3'd2: font_row = 5'b10001;
                    3'd3: font_row = 5'b10001;
                    3'd4: font_row = 5'b10001;
                    3'd5: font_row = 5'b01010;
                    3'd6: font_row = 5'b00100;
                endcase
            end
        endcase
    end
endfunction

function font_pixel;
    input [7:0] ch;
    input [2:0] x;
    input [2:0] y;
    reg [4:0] row_bits;
    begin
        row_bits = font_row(ch, y);
        if (x < 5) font_pixel = row_bits[4 - x];
        else font_pixel = 1'b0;
    end
endfunction

wire start_text_area = (h_cnt >= TEXT_START_X) && (h_cnt < TEXT_START_X + TEXT_W_START) &&
                       (v_cnt >= TEXT_Y) && (v_cnt < TEXT_Y + TEXT_H);
wire over_text_area = (h_cnt >= TEXT_OVER_X) && (h_cnt < TEXT_OVER_X + TEXT_W_OVER) &&
                      (v_cnt >= TEXT_Y) && (v_cnt < TEXT_Y + TEXT_H);

wire [9:0] start_text_x = h_cnt - TEXT_START_X;
wire [9:0] over_text_x = h_cnt - TEXT_OVER_X;
wire [9:0] text_y_rel = v_cnt - TEXT_Y;

wire [3:0] start_char_idx = start_text_x / TEXT_CELL_W;
wire [3:0] over_char_idx = over_text_x / TEXT_CELL_W;
wire [2:0] start_font_x = (start_text_x % TEXT_CELL_W) / TEXT_SCALE;
wire [2:0] over_font_x = (over_text_x % TEXT_CELL_W) / TEXT_SCALE;
wire [2:0] font_y = text_y_rel / TEXT_SCALE;

reg [7:0] start_char;
reg [7:0] over_char;

always @(*) begin
    case (start_char_idx)
        4'd0: start_char = "G";
        4'd1: start_char = "a";
        4'd2: start_char = "m";
        4'd3: start_char = "e";
        4'd4: start_char = " ";
        4'd5: start_char = "s";
        4'd6: start_char = "t";
        4'd7: start_char = "a";
        4'd8: start_char = "r";
        4'd9: start_char = "t";
        default: start_char = " ";
    endcase
end

always @(*) begin
    case (over_char_idx)
        4'd0: over_char = "G";
        4'd1: over_char = "a";
        4'd2: over_char = "m";
        4'd3: over_char = "e";
        4'd4: over_char = " ";
        4'd5: over_char = "O";
        4'd6: over_char = "v";
        4'd7: over_char = "e";
        4'd8: over_char = "r";
        default: over_char = " ";
    endcase
end

wire start_text_pixel = start_text_area && font_pixel(start_char, start_font_x, font_y);
wire over_text_pixel = over_text_area && font_pixel(over_char, over_font_x, font_y);

// ================= Color =================
reg [4:0] r;
reg [4:0] g;
reg [4:0] b;

always @(*) begin
    if (!active) begin
        r = 5'd0;
        g = 5'd0;
        b = 5'd0;
    end else if (!game_started) begin
        if (start_text_pixel) begin
            r = 5'd31;
            g = 5'd31;
            b = 5'd31;
        end else begin
            r = 5'd0;
            g = 5'd0;
            b = 5'd0;
        end
    end else if (game_over) begin
        if (over_text_pixel) begin
            r = 5'd31;
            g = 5'd31;
            b = 5'd31;
        end else begin
            r = 5'd0;
            g = 5'd0;
            b = 5'd0;
        end
    end else if (is_snake) begin
        r = 5'd0;
        g = 5'd31;
        b = 5'd6;
    end else if (is_food) begin
        r = 5'd31;
        g = 5'd4;
        b = 5'd4;
    end else if (grid_line) begin
        r = 5'd0;
        g = 5'd3;
        b = 5'd0;
    end else begin
        r = 5'd0;
        g = 5'd0;
        b = 5'd0;
    end
end

assign VGA_R = r;
assign VGA_G = g;
assign VGA_B = b;

endmodule
