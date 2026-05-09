module VGA_test
(
    input clk,
    input rst_n,

    input BUT1,
    input BUT2,
    input BUT3,
    input BUT4,

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
    .pixel_data(pixel_data)
);

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
