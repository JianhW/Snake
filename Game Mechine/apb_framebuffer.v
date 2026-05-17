module apb_framebuffer
(
    input              PCLK,
    input              PRESETn,

    input      [31:0]  PADDR,
    input              PENABLE,
    input              PSEL,
    input              PWRITE,
    input      [31:0]  PWDATA,

    output reg [31:0]  PRDATA,
    output             PREADY,
    output             PSLVERROR,

    input      [4:0]   switch_inputs,
    input      [11:0]  pixel_addr,
    output     [15:0]  pixel_data,
    output reg [15:0]  display_time_bcd,
    output reg [15:0]  display_score_bcd
);

reg         we_a;
reg [11:0]  addr_a;
reg [7:0]   wdata_a;
wire [7:0]  rdata_a;
wire [7:0]  rdata_b;

wire apb_write;
wire apb_read;
wire fb_access;
wire switch_access;
wire time_access;
wire score_access;
wire [31:0] fb_offset;
wire [15:0] rdata_a_565;
wire [15:0] rdata_b_565;

localparam FB_BASE = 32'hF8100000;
localparam FB_WORDS = 4096;
localparam FB_BYTES = FB_WORDS * 2;
localparam SWITCH_REG_OFFSET = FB_BYTES;
localparam TIME_REG_OFFSET = FB_BYTES + 4;
localparam SCORE_REG_OFFSET = FB_BYTES + 8;

assign fb_offset = PADDR - FB_BASE;
assign fb_access = fb_offset < FB_BYTES;
assign switch_access = fb_offset == SWITCH_REG_OFFSET;
assign time_access = fb_offset == TIME_REG_OFFSET;
assign score_access = fb_offset == SCORE_REG_OFFSET;

assign apb_write = PSEL && PENABLE && PWRITE;
assign apb_read  = PSEL && PENABLE && !PWRITE;
assign rdata_a_565 = {rdata_a[7:5], rdata_a[7:6], rdata_a[4:2], rdata_a[4:2], rdata_a[1:0], rdata_a[1:0], rdata_a[1]};
assign rdata_b_565 = {rdata_b[7:5], rdata_b[7:6], rdata_b[4:2], rdata_b[4:2], rdata_b[1:0], rdata_b[1:0], rdata_b[1]};

always @(posedge PCLK or negedge PRESETn)
begin
    if(!PRESETn)
    begin
        we_a <= 1'b0;
        addr_a <= 12'd0;
        wdata_a <= 8'd0;
        display_time_bcd <= 16'd0;
        display_score_bcd <= 16'd0;
    end
    else
    begin
        if(apb_write)
        begin
            if(fb_access)
            begin
                we_a <= 1'b1;
                addr_a <= fb_offset[12:1];
                wdata_a <= {PWDATA[15:13], PWDATA[10:8], PWDATA[4:3]};
            end
            else
            begin
                we_a <= 1'b0;
            end

            if(time_access)
                display_time_bcd <= PWDATA[15:0];
            else if(score_access)
                display_score_bcd <= PWDATA[15:0];
        end
        else
        begin
            we_a <= 1'b0;
        end
    end
end

always @(posedge PCLK)
begin
    if(apb_read)
    begin
        if(fb_access)
            PRDATA <= {16'd0, rdata_a_565};
        else if(switch_access)
            PRDATA <= {27'd0, switch_inputs};
        else if(time_access)
            PRDATA <= {16'd0, display_time_bcd};
        else if(score_access)
            PRDATA <= {16'd0, display_score_bcd};
        else
            PRDATA <= 32'd0;
    end
end

assign PREADY = 1'b1;
assign PSLVERROR = 1'b0;

framebuffer u_framebuffer
(
    .we_a    (we_a),
    .addr_a  (addr_a),
    .wdata_a (wdata_a),
    .rdata_a (rdata_a),

    .we_b    (1'b0),
    .addr_b  (pixel_addr),
    .wdata_b (8'd0),
    .rdata_b (rdata_b),

    .clk     (PCLK),
    .clke    (1'b1)
);

assign pixel_data = rdata_b_565;

endmodule
