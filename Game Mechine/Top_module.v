module VGA_test
(
    input clk,
    input rst_n,

    input BUT1,
    input BUT2,
    input BUT3,
    input BUT4,
    
    input sw1,
    input sw2,
    input sw3,
    input sw4,

    input sw5,
    input sw6,
    input sw7,
    input sw8,
    input sw9,

    output [4:0] VGA_R,
    output [5:0] VGA_G,
    output [4:0] VGA_B,
    output VGA_HS,
    output VGA_VS,
    output [6:0] seg,
    output [7:0] sel,
    output [9:0] LED,
    output buzzer,

    (* syn_peri_port = 0 *) input io_jtag_CAPTURE,
    (* syn_peri_port = 0 *) input io_jtag_DRCK,
    (* syn_peri_port = 0 *) input io_jtag_RESET,
    (* syn_peri_port = 0 *) input io_jtag_RUNTEST,
    (* syn_peri_port = 0 *) input io_jtag_SEL,
    (* syn_peri_port = 0 *) input io_jtag_SHIFT,
    (* syn_peri_port = 0 *) input io_jtag_TCK,
    (* syn_peri_port = 0 *) input io_jtag_TDI,
    (* syn_peri_port = 0 *) input io_jtag_TMS,
    (* syn_peri_port = 0 *) input io_jtag_UPDATE,
    (* syn_peri_port = 0 *) output io_jtag_TDO
);

/////////////////////////////////////////////////////
// APB wires
// SoC 的 io_apbSlave_0_PADDR 仅 16 位（字节偏移在低 16 位），须拼成完整 32 位
// 再送 framebuffer，否则 PADDR[31:16] 悬空，(PADDR-FB_BASE) 错、写不进显存。
/////////////////////////////////////////////////////

wire [15:0] soc_apb_paddr;
wire [31:0] PADDR;
wire        PENABLE;
wire [31:0] PRDATA;
wire        PREADY;
wire        PSEL;
wire        PSLVERROR;
wire [31:0] PWDATA;
wire        PWRITE;

/* 与 apb_framebuffer.v / main.c 中 FB_BASE 高半字一致 */
localparam [15:0] FB_APB_ADDR_HI = 16'hF810;

assign PADDR = {FB_APB_ADDR_HI, soc_apb_paddr};

/////////////////////////////////////////////////////
// framebuffer
/////////////////////////////////////////////////////

wire [11:0] pixel_addr;
wire [15:0] pixel_data;
wire [15:0] display_time_bcd;
wire [15:0] display_score_bcd;

/////////////////////////////////////////////////////
// RISC-V SoC
/////////////////////////////////////////////////////

wire [3:0] soc_gpio_write;
wire [3:0] soc_gpio_writeEnable;

riscv_soc u_riscv_soc
(
    .io_systemClk(clk),
    .io_asyncReset(~rst_n),

    .io_apbSlave_0_PADDR(soc_apb_paddr),
    .io_apbSlave_0_PENABLE(PENABLE),
    .io_apbSlave_0_PRDATA(PRDATA),
    .io_apbSlave_0_PREADY(PREADY),
    .io_apbSlave_0_PSEL(PSEL),
    .io_apbSlave_0_PSLVERROR(PSLVERROR),
    .io_apbSlave_0_PWDATA(PWDATA),
    .io_apbSlave_0_PWRITE(PWRITE),

    .jtagCtrl_enable(io_jtag_SEL),
    .jtagCtrl_tdi(io_jtag_TDI),
    .jtagCtrl_capture(io_jtag_CAPTURE),
    .jtagCtrl_shift(io_jtag_SHIFT),
    .jtagCtrl_update(io_jtag_UPDATE),
    .jtagCtrl_reset(io_jtag_RESET),
    .jtagCtrl_tck(io_jtag_TCK),
    .jtagCtrl_tdo(io_jtag_TDO),

    .io_systemReset(),

    .system_gpio_0_io_writeEnable(soc_gpio_writeEnable),
    .system_gpio_0_io_write(soc_gpio_write),
    /* io_read[3:0]：bit0=BUT1 … bit3=BUT4；写 bit3 经下面 assign 驱动蜂鸣器管脚 */
    .system_gpio_0_io_read({BUT4, BUT3, BUT2, BUT1})
);

/////////////////////////////////////////////////////
// APB framebuffer
/////////////////////////////////////////////////////

apb_framebuffer u_apb_fb
(
    .PCLK(clk),
    .PRESETn(rst_n),

    .PADDR(PADDR),
    .PENABLE(PENABLE),
    .PSEL(PSEL),
    .PWRITE(PWRITE),
    .PWDATA(PWDATA),

    .PRDATA(PRDATA),
    .PREADY(PREADY),
    .PSLVERROR(PSLVERROR),

    .switch_inputs({sw9, sw8, sw7, sw6, sw5}),
    .pixel_addr(pixel_addr),
    .pixel_data(pixel_data),
    .display_time_bcd(display_time_bcd),
    .display_score_bcd(display_score_bcd)
);

reg [6:0] seg_r;
reg [7:0] sel_r;
reg [15:0] seg_scan;
reg [21:0] led_step_div;
reg [7:0] led_pwm_cnt;
reg [7:0] led_anim_phase;
reg [9:0] led_sparkle;
reg [3:0] led_wave_pos;
reg       led_wave_dir;
reg [9:0] led_r;
reg [7:0] led_level;
reg       led_sparkle_bit;
integer   led_idx;
integer   led_dist;
integer   led_dist_b;
integer   led_center_dist;
wire [2:0] led_effect_sel;

assign seg = seg_r;
assign sel = sel_r;
assign LED = led_r;

// sw1 has the highest priority if more than one switch is on.
assign led_effect_sel = sw1 ? 3'd1 :
                        sw2 ? 3'd2 :
                        sw3 ? 3'd3 :
                        sw4 ? 3'd4 :
                              3'd0;

function [6:0] sevenseg_decode;
    input [3:0] digit;
    begin
        case(digit)
            4'd0: sevenseg_decode = 7'b1111110;
            4'd1: sevenseg_decode = 7'b0110000;
            4'd2: sevenseg_decode = 7'b1101101;
            4'd3: sevenseg_decode = 7'b1111001;
            4'd4: sevenseg_decode = 7'b0110011;
            4'd5: sevenseg_decode = 7'b1011011;
            4'd6: sevenseg_decode = 7'b1011111;
            4'd7: sevenseg_decode = 7'b1110000;
            4'd8: sevenseg_decode = 7'b1111111;
            4'd9: sevenseg_decode = 7'b1111011;
            4'hA: sevenseg_decode = 7'b1110111;
            4'hB: sevenseg_decode = 7'b0011111;
            4'hC: sevenseg_decode = 7'b1001110;
            4'hD: sevenseg_decode = 7'b0111101;
            4'hE: sevenseg_decode = 7'b1001111;
            4'hF: sevenseg_decode = 7'b1000111;
            default: sevenseg_decode = 7'b0000000;
        endcase
    end
endfunction

always @(posedge clk or negedge rst_n)
begin
    if(!rst_n)
        seg_scan <= 16'd0;
    else
        seg_scan <= seg_scan + 16'd1;
end

always @*
begin
    case(seg_scan[15:13])
        3'd7:
        begin
            sel_r = 8'b01111111;
            seg_r = sevenseg_decode(display_time_bcd[15:12]);
        end
        3'd6:
        begin
            sel_r = 8'b10111111;
            seg_r = sevenseg_decode(display_time_bcd[11:8]);
        end
        3'd5:
        begin
            sel_r = 8'b11011111;
            seg_r = sevenseg_decode(display_time_bcd[7:4]);
        end
        3'd4:
        begin
            sel_r = 8'b11101111;
            seg_r = sevenseg_decode(display_time_bcd[3:0]);
        end
        3'd3:
        begin
            sel_r = 8'b11110111;
            seg_r = sevenseg_decode(display_score_bcd[15:12]);
        end
        3'd2:
        begin
            sel_r = 8'b11111011;
            seg_r = sevenseg_decode(display_score_bcd[11:8]);
        end
        3'd1:
        begin
            sel_r = 8'b11111101;
            seg_r = sevenseg_decode(display_score_bcd[7:4]);
        end
        default:
        begin
            sel_r = 8'b11111110;
            seg_r = sevenseg_decode(display_score_bcd[3:0]);
        end
    endcase
end

/////////////////////////////////////////////////////
// LED effects
// 0: original soft wave when sw1..sw4 are all off
// 1: crossed comet trails
// 2: breathing aurora with moving highlight
// 3: center ripple
// 4: pseudo-random sparkle
/////////////////////////////////////////////////////

always @(posedge clk or negedge rst_n)
begin
    if(!rst_n)
        led_pwm_cnt <= 8'd0;
    else
        led_pwm_cnt <= led_pwm_cnt + 8'd1;
end

always @(posedge clk or negedge rst_n)
begin
    if(!rst_n)
    begin
        led_step_div <= 22'd0;
        led_anim_phase <= 8'd0;
        led_sparkle <= 10'b1001010110;
        led_wave_pos <= 4'd0;
        led_wave_dir <= 1'b0;
    end
    else if(led_step_div == 22'd2499999)
    begin
        led_step_div <= 22'd0;
        led_anim_phase <= led_anim_phase + 8'd1;
        led_sparkle <= {led_sparkle[8:0], led_sparkle[9] ^ led_sparkle[6]};

        if(!led_wave_dir)
        begin
            if(led_wave_pos == 4'd9)
            begin
                led_wave_pos <= 4'd8;
                led_wave_dir <= 1'b1;
            end
            else
                led_wave_pos <= led_wave_pos + 4'd1;
        end
        else
        begin
            if(led_wave_pos == 4'd0)
            begin
                led_wave_pos <= 4'd1;
                led_wave_dir <= 1'b0;
            end
            else
                led_wave_pos <= led_wave_pos - 4'd1;
        end
    end
    else
        led_step_div <= led_step_div + 22'd1;
end

always @*
begin
    led_r = 10'd0;
    led_level = 8'd0;
    led_sparkle_bit = 1'b0;
    led_dist = 0;
    led_dist_b = 0;
    led_center_dist = 0;

    case(led_effect_sel)
        3'd1:
        begin
            // Two opposite comets cross with PWM tails.
            for(led_idx = 0; led_idx < 10; led_idx = led_idx + 1)
            begin
                if(led_wave_pos >= led_idx)
                    led_dist = led_wave_pos - led_idx;
                else
                    led_dist = led_idx - led_wave_pos;

                if((4'd9 - led_wave_pos) >= led_idx)
                    led_dist_b = (4'd9 - led_wave_pos) - led_idx;
                else
                    led_dist_b = led_idx - (4'd9 - led_wave_pos);

                if(led_dist_b < led_dist)
                    led_dist = led_dist_b;

                case(led_dist)
                    0: led_level = 8'hFF;
                    1: led_level = 8'hB0;
                    2: led_level = 8'h60;
                    3: led_level = 8'h20;
                    default: led_level = 8'h00;
                endcase

                led_r[led_idx] = (led_pwm_cnt < led_level);
            end
        end

        3'd2:
        begin
            // Slow global breathing plus a brighter moving ridge.
            if(led_anim_phase[7])
                led_level = {~led_anim_phase[6:0], 1'b1};
            else
                led_level = {led_anim_phase[6:0], 1'b0};

            for(led_idx = 0; led_idx < 10; led_idx = led_idx + 1)
            begin
                if(led_wave_pos >= led_idx)
                    led_dist = led_wave_pos - led_idx;
                else
                    led_dist = led_idx - led_wave_pos;

                if(led_dist == 0)
                    led_r[led_idx] = (led_pwm_cnt < 8'hFF);
                else if(led_dist == 1)
                    led_r[led_idx] = (led_pwm_cnt < (led_level | 8'h60));
                else
                    led_r[led_idx] = (led_pwm_cnt < (led_level >> 1));
            end
        end

        3'd3:
        begin
            // Ripple grows from the center and fades toward the edge.
            for(led_idx = 0; led_idx < 10; led_idx = led_idx + 1)
            begin
                if(led_idx < 5)
                    led_center_dist = 4 - led_idx;
                else
                    led_center_dist = led_idx - 5;

                if((led_wave_pos[2:0] + led_center_dist) > 4)
                    led_dist = (led_wave_pos[2:0] + led_center_dist) - 4;
                else
                    led_dist = 4 - (led_wave_pos[2:0] + led_center_dist);

                case(led_dist)
                    0: led_level = 8'hFF;
                    1: led_level = 8'h90;
                    2: led_level = 8'h35;
                    default: led_level = 8'h08;
                endcase

                led_r[led_idx] = (led_pwm_cnt < led_level);
            end
        end

        3'd4:
        begin
            // Twinkling pseudo-random points with a moving bright sweep.
            for(led_idx = 0; led_idx < 10; led_idx = led_idx + 1)
            begin
                if(led_wave_pos >= led_idx)
                    led_dist = led_wave_pos - led_idx;
                else
                    led_dist = led_idx - led_wave_pos;

                case(led_idx)
                    0: led_sparkle_bit = led_sparkle[0];
                    1: led_sparkle_bit = led_sparkle[1];
                    2: led_sparkle_bit = led_sparkle[2];
                    3: led_sparkle_bit = led_sparkle[3];
                    4: led_sparkle_bit = led_sparkle[4];
                    5: led_sparkle_bit = led_sparkle[5];
                    6: led_sparkle_bit = led_sparkle[6];
                    7: led_sparkle_bit = led_sparkle[7];
                    8: led_sparkle_bit = led_sparkle[8];
                    default: led_sparkle_bit = led_sparkle[9];
                endcase

                if(led_dist == 0)
                    led_level = 8'hFF;
                else if(led_sparkle_bit)
                    led_level = led_anim_phase[2] ? 8'hD0 : 8'h50;
                else
                    led_level = led_anim_phase[3] ? 8'h18 : 8'h00;

                led_r[led_idx] = (led_pwm_cnt < led_level);
            end
        end

        default:
        begin
            for(led_idx = 0; led_idx < 10; led_idx = led_idx + 1)
            begin
                if(led_wave_pos >= led_idx)
                    led_dist = led_wave_pos - led_idx;
                else
                    led_dist = led_idx - led_wave_pos;

                case(led_dist)
                    0: led_level = 8'hFF;
                    1: led_level = 8'h70;
                    2: led_level = 8'h20;
                    default: led_level = 8'h00;
                endcase

                led_r[led_idx] = (led_pwm_cnt < led_level);
            end
        end
    endcase
end

/////////////////////////////////////////////////////
// 25MHz
/////////////////////////////////////////////////////

reg pixel_clk;

always @(posedge clk or negedge rst_n)
begin
    if(!rst_n)
        pixel_clk <= 1'b0;
    else
        pixel_clk <= ~pixel_clk;
end

/////////////////////////////////////////////////////
// VGA timing
/////////////////////////////////////////////////////

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

always @(posedge pixel_clk or negedge rst_n)
begin
    if(!rst_n)
        h_cnt <= 0;
    else if(h_cnt == H_TOTAL - 1)
        h_cnt <= 0;
    else
        h_cnt <= h_cnt + 1;
end

always @(posedge pixel_clk or negedge rst_n)
begin
    if(!rst_n)
        v_cnt <= 0;
    else if(h_cnt == H_TOTAL - 1)
    begin
        if(v_cnt == V_TOTAL - 1)
            v_cnt <= 0;
        else
            v_cnt <= v_cnt + 1;
    end
end

/////////////////////////////////////////////////////
// sync
/////////////////////////////////////////////////////

assign VGA_HS =
~(
(h_cnt >= H_VISIBLE + H_FRONT) &&
(h_cnt <  H_VISIBLE + H_FRONT + H_SYNC)
);

assign VGA_VS =
~(
(v_cnt >= V_VISIBLE + V_FRONT) &&
(v_cnt <  V_VISIBLE + V_FRONT + V_SYNC)
);

/////////////////////////////////////////////////////
// active
/////////////////////////////////////////////////////

wire active;

assign active =
(h_cnt < H_VISIBLE) &&
(v_cnt < V_VISIBLE);

/////////////////////////////////////////////////////
// 64x64 framebuffer 映射到 640x480
// 旧式 y=v_cnt/7 在 v_cnt=479 时 y=68，超出 64 行，读地址错位导致画面混乱。
// 用等比缩放：y = v*(64-1)/(480-1) 近似为 v*64/480（整数除法），并限制在消隐区。
/////////////////////////////////////////////////////

wire [5:0] x;
wire [5:0] y;
wire [9:0] h_vis;
wire [9:0] v_vis;
wire [15:0] x_mul;
wire [15:0] y_mul;

assign h_vis = (h_cnt < H_VISIBLE) ? h_cnt : 10'd0;
assign v_vis = (v_cnt < V_VISIBLE) ? v_cnt : 10'd0;

assign x_mul = h_vis * 10'd64;
assign y_mul = v_vis * 10'd64;
/* 整数除法：整幅 640x480 均匀映射到 64x64，避免 y 超过 63 */
assign x = x_mul / H_VISIBLE;
assign y = y_mul / V_VISIBLE;

assign pixel_addr = y * 64 + x;

/////////////////////////////////////////////////////
// RGB
/////////////////////////////////////////////////////

assign VGA_R = active ? pixel_data[15:11] : 5'd0;
assign VGA_G = active ? pixel_data[10:5]  : 6'd0;
assign VGA_B = active ? pixel_data[4:0]   : 5'd0;

/* 无源/有源蜂鸣器：软件置 GPIO 输出使能并对 bit3 写 1 发声（见 main.c BUZZER_MASK） */
assign buzzer = soc_gpio_write[3] & soc_gpio_writeEnable[3];

endmodule
