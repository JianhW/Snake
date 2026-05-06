module VGA_test
(
  input clk,
  input rst_n,
  input BUT4,
  input BUT3,
  input BUT2,
  input BUT1,

  output [4:0] VGA_B,
  output [4:0] VGA_G,
  output VGA_HS,
  output [4:0] VGA_R,
  output VGA_VS
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
wire game_tick = (game_cnt == 24'd5_000_000);

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

        if (game_over) begin
            if (key_up_pulse || key_down_pulse || key_left_pulse || key_right_pulse) begin
                len <= 5'd4;
                dir <= DIR_RIGHT;
                next_dir <= DIR_RIGHT;
                dir_pending <= 1'b0;
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

// ================= Color =================
reg [4:0] r;
reg [4:0] g;
reg [4:0] b;

always @(*) begin
    if (!active) begin
        r = 5'd0;
        g = 5'd0;
        b = 5'd0;
    end else if (game_over) begin
        if (is_snake) begin
            r = 5'd31;
            g = 5'd4;
            b = 5'd4;
        end else begin
            r = 5'd10;
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
