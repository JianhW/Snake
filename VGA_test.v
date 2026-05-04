module VGA_test
(
  input clk,        // 50MHz
  input rst_n,

  output [4:0] VGA_B,
  output [4:0] VGA_G,
  output VGA_HS,
  output [4:0] VGA_R,
  output VGA_VS
);

// ================= 50MHz -> 25MHz =================
reg pixel_clk;
always @(posedge clk or negedge rst_n) begin
    if (!rst_n)
        pixel_clk <= 0;
    else
        pixel_clk <= ~pixel_clk;  // ÷2
end

// ================= VGA 640x480@60Hz =================
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

// ================= 计数器 =================
reg [9:0] h_cnt;
reg [9:0] v_cnt;

// 行计数
always @(posedge pixel_clk or negedge rst_n) begin
    if (!rst_n)
        h_cnt <= 0;
    else if (h_cnt == H_TOTAL - 1)
        h_cnt <= 0;
    else
        h_cnt <= h_cnt + 1;
end

// 场计数
always @(posedge pixel_clk or negedge rst_n) begin
    if (!rst_n)
        v_cnt <= 0;
    else if (h_cnt == H_TOTAL - 1) begin
        if (v_cnt == V_TOTAL - 1)
            v_cnt <= 0;
        else
            v_cnt <= v_cnt + 1;
    end
end

// ================= HS / VS（标准负极性）=================
assign VGA_HS = ~((h_cnt >= (H_VISIBLE + H_FRONT)) &&
                  (h_cnt <  (H_VISIBLE + H_FRONT + H_SYNC)));

assign VGA_VS = ~((v_cnt >= (V_VISIBLE + V_FRONT)) &&
                  (v_cnt <  (V_VISIBLE + V_FRONT + V_SYNC)));

// ================= 显示区域 =================
wire active;
assign active = (h_cnt < H_VISIBLE) && (v_cnt < V_VISIBLE);

// ================= RGB（纯红测试）=================
assign VGA_R = active ? 5'b11111 : 5'b00000;
assign VGA_G = active ? 5'b11111 : 5'b00000;
assign VGA_B = active ? 5'b00000 : 5'b00000;
endmodule