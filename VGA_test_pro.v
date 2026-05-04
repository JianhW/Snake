module VGA_test
(
  input clk,
  input rst_n,
  input BUT4,

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
        pixel_clk <= ~pixel_clk;
end

// ================= VGA timing =================
parameter H_VISIBLE = 640;
parameter H_FRONT = 16;
parameter H_SYNC = 96;
parameter H_BACK = 48;
parameter H_TOTAL = 800;

parameter V_VISIBLE = 480;
parameter V_FRONT = 10;
parameter V_SYNC = 2;
parameter V_BACK = 33;
parameter V_TOTAL = 525;

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

// ================= HS / VS =================
assign VGA_HS = ~((h_cnt >= (H_VISIBLE + H_FRONT)) &&
                  (h_cnt < (H_VISIBLE + H_FRONT + H_SYNC)));

assign VGA_VS = ~((v_cnt >= (V_VISIBLE + V_FRONT)) &&
                  (v_cnt < (V_VISIBLE + V_FRONT + V_SYNC)));

wire active;
assign active = (h_cnt < H_VISIBLE) && (v_cnt < V_VISIBLE);

// ================= 按键去抖 =================
reg [19:0] btn_cnt;
reg btn_sync_0, btn_sync_1;
reg btn_last;
wire btn_posedge;

// 双触发同步（防亚稳态）
always @(posedge clk) begin
    btn_sync_0 <= BUT4;
    btn_sync_1 <= btn_sync_0;
end

// 简单去抖
always @(posedge clk or negedge rst_n) begin
    if (!rst_n)
        btn_cnt <= 0;
    else if (btn_sync_1 != btn_last)
        btn_cnt <= btn_cnt + 1;
    else
        btn_cnt <= 0;
end

always @(posedge clk) begin
    if (btn_cnt == 20'hFFFFF)
        btn_last <= btn_sync_1;
end

assign btn_posedge = (btn_sync_1 & ~btn_last);

// ================= 颜色状态机 =================
reg [2:0] color_state;

always @(posedge clk or negedge rst_n) begin
    if (!rst_n)
        color_state <= 0;
    else if (btn_posedge)
        color_state <= (color_state == 4) ? 0 : color_state + 1;
end

// ================= 颜色输出 =================
reg [4:0] r;
reg [4:0] g;
reg [4:0] b;

always @(*) begin
    case (color_state)
        0: begin r = 5'b11111; g = 0;      b = 0;      end // 红
        1: begin r = 0;      g = 5'b11111; b = 0;      end // 绿
        2: begin r = 0;      g = 0;      b = 5'b11111; end // 蓝
        3: begin r = 5'b11111; g = 5'b11111; b = 5'b11111; end // 白
        4: begin r = 0; g = 0; b = 0; end // 黑
        default: begin r = 0; g = 0; b = 0; end
    endcase
end

assign VGA_R = active ? r : 0;
assign VGA_G = active ? g : 0;
assign VGA_B = active ? b : 0;

endmodule