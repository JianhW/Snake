//FRAMBUFFER深度为4096，width=16

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
    output [4:0] VGA_G,
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
/////////////////////////////////////////////////////

wire [31:0] PADDR;
wire        PENABLE;
wire [31:0] PRDATA;
wire        PREADY;
wire        PSEL;
wire        PSLVERROR;
wire [31:0] PWDATA;
wire        PWRITE;

/////////////////////////////////////////////////////
// framebuffer wires
/////////////////////////////////////////////////////

wire [11:0] pixel_addr;
wire [15:0] pixel_data;

/////////////////////////////////////////////////////
// RISC-V SoC
/////////////////////////////////////////////////////

riscv_soc u_riscv_soc
(
    .io_systemClk(clk),
    .io_asyncReset(~rst_n),

    .io_apbSlave_0_PADDR(PADDR),
    .io_apbSlave_0_PENABLE(PENABLE),
    .io_apbSlave_0_PRDATA(PRDATA),
    .io_apbSlave_0_PREADY(PREADY),
    .io_apbSlave_0_PSEL(PSEL),
    .io_apbSlave_0_PSLVERROR(PSLVERROR),
    .io_apbSlave_0_PWDATA(PWDATA),
    .io_apbSlave_0_PWRITE(PWRITE),

    // JTAG disable
    .jtagCtrl_enable(io_jtag_SEL),
    .jtagCtrl_tdi(io_jtag_TDI),
    .jtagCtrl_capture(io_jtag_CAPTURE),
    .jtagCtrl_shift(io_jtag_SHIFT),
    .jtagCtrl_update(io_jtag_UPDATE),
    .jtagCtrl_reset(io_jtag_RESET),
    .jtagCtrl_tck(io_jtag_TCK),

    .jtagCtrl_tdo(io_jtag_TDO),

    .io_systemReset(),

    .system_gpio_0_io_writeEnable(),
    .system_gpio_0_io_write(),
    .system_gpio_0_io_read(32'd0)
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

    .pixel_addr(pixel_addr),
    .pixel_data(pixel_data)
);

/////////////////////////////////////////////////////
// 50MHz -> 25MHz
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

/////////////////////////////////////////////////////
// horizontal counter
/////////////////////////////////////////////////////

always @(posedge pixel_clk or negedge rst_n)
begin
    if(!rst_n)
        h_cnt <= 10'd0;
    else if(h_cnt == H_TOTAL - 1)
        h_cnt <= 10'd0;
    else
        h_cnt <= h_cnt + 1'b1;
end

/////////////////////////////////////////////////////
// vertical counter
/////////////////////////////////////////////////////

always @(posedge pixel_clk or negedge rst_n)
begin
    if(!rst_n)
        v_cnt <= 10'd0;
    else if(h_cnt == H_TOTAL - 1)
    begin
        if(v_cnt == V_TOTAL - 1)
            v_cnt <= 10'd0;
        else
            v_cnt <= v_cnt + 1'b1;
    end
end

/////////////////////////////////////////////////////
// HS / VS
/////////////////////////////////////////////////////

assign VGA_HS =
~(
    (h_cnt >= (H_VISIBLE + H_FRONT)) &&
    (h_cnt <  (H_VISIBLE + H_FRONT + H_SYNC))
);

assign VGA_VS =
~(
    (v_cnt >= (V_VISIBLE + V_FRONT)) &&
    (v_cnt <  (V_VISIBLE + V_FRONT + V_SYNC))
);

/////////////////////////////////////////////////////
// active area
/////////////////////////////////////////////////////

wire active;

assign active =
(h_cnt < H_VISIBLE) &&
(v_cnt < V_VISIBLE);

/////////////////////////////////////////////////////
// framebuffer coordinate
/////////////////////////////////////////////////////

wire [5:0] x;
wire [5:0] y;

assign x = h_cnt[8:3];
assign y = v_cnt[8:3];

/////////////////////////////////////////////////////
// framebuffer address
/////////////////////////////////////////////////////

assign pixel_addr = y * 64 + x;

/////////////////////////////////////////////////////
// RGB565 output
/////////////////////////////////////////////////////

assign VGA_R = active ? pixel_data[15:11] : 5'd0;
assign VGA_G = active ? pixel_data[10:5]  : 6'd0;
assign VGA_B = active ? pixel_data[4:0]   : 5'd0;

/////////////////////////////////////////////////////
// buzzer
/////////////////////////////////////////////////////

assign buzzer = 1'b0;

endmodule
