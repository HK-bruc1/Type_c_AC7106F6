struct nand_flash_params_t def_params[] = {
    {
        .id = 0xcd71,//F35SQA001G-WWT
        .plane_number = 1,
        .block_number = 1024,
        .page_number = 64,
        .page_size = 2048,
        .spi_io_params = {
            .port_cs_hd = 0,
            .port_clk_hd = 3,
            .port_do_hd = 0,
            .port_di_hd = 0,
            .port_d2_hd = 0,
            .port_d3_hd = 0,
        },
        .oob_info[0] = {
            .oob_offset = 0,
            .oob_size = 64,
        },
        .oob_info[1] = {
            .oob_offset = 0,
            .oob_size = 0,
        },
        .oob_size = 64,
        // 00 - 00 -- succ
        // 01 - 01 -- ecc fix & succ
        // 10 , 11 -- ecc fix
        // 11 , 11 -- ecc error
        .ecc_mask = 0b00110000,
        .ecc_status_1 = 0b1100,
        .ecc_status_0 = 0b1110,
        .write_enable_position = 0,
        .dummy_cycles = 8,
        .init_cmd_size = 2,
        .init_cmd[0] = {
            .addr = 0xa0,
            .data = 0b01111100,  //wp all
            .mask = 0b11111101,
        },
        .init_cmd[1] = {
            .addr = 0xb0,
            .data = 0b00010001,
            .mask = 0b11010111,
        },
        .unlock_cmd_size = 1,
        .unlock_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00000000,
            .mask = 0b11111101,
        }
    },
    {
        .id = 0x115,//HYF1GQ4UTACAE
        .plane_number = 1,
        .block_number = 1024,
        .page_number = 64,
        .page_size = 2048,
        .spi_io_params = {
            .port_cs_hd = 0,
            .port_clk_hd = 3,
            .port_do_hd = 0,
            .port_di_hd = 0,
            .port_d2_hd = 0,
            .port_d3_hd = 0,
        },
        .oob_info[0] = {
            .oob_offset = 0,
            .oob_size = 64,
        },
        .oob_info[1] = {
            .oob_offset = 0,
            .oob_size = 0,
        },
        .oob_size = 64,
        // 00 - 00 -- succ
        // 01 - 01 -- ecc fix & succ
        // 10 , 10 -- ecc fix
        // 11 , 11 -- ecc error
        .ecc_mask = 0b00110000,
        .ecc_status_1 = 0b1100,
        .ecc_status_0 = 0b1010,
        .write_enable_position = 1,
        .dummy_cycles = 8,
        .init_cmd_size = 3,
        .init_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00000010,
            .mask = 0b11111111,
        },
        .init_cmd[1] = {
            .addr = 0xa0,
            .data = 0b01111000,  //wp all
            .mask = 0b11111000,
        },
        .init_cmd[2] = {
            .addr = 0xb0,
            .data = 0b00010000,
            .mask = 0b11110010,
        },
        .unlock_cmd_size = 2,
        .unlock_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00000010,
            .mask = 0b11111111,
        },
        .unlock_cmd[1] = {
            .addr = 0xa0,
            .data = 0b00000000,
            .mask = 0b11111111,
        },

    },

    {
        .id = 0x1903,//XS25D1GASL
        .plane_number = 1,
        .block_number = 1024,
        .page_number = 64,
        .page_size = 2048,
        .spi_io_params = {
            .port_cs_hd = 0,
            .port_clk_hd = 3,
            .port_do_hd = 0,
            .port_di_hd = 0,
            .port_d2_hd = 0,
            .port_d3_hd = 0,
        },
        .oob_info[0] = {
            .oob_offset = 0,
            .oob_size = 64,
        },
        .oob_info[1] = {
            .oob_offset = 0,
            .oob_size = 0,
        },
        .oob_size = 64,
        // 00 - 00 -- succ
        // 01 - 01 -- ecc fix
        // 10 , 11 -- ecc error
        // 11 , 11 -- ecc error
        .ecc_mask = 0b00110000,
        .ecc_status_1 = 0b1100,
        .ecc_status_0 = 0b1110,
        .write_enable_position = 0,
        .dummy_cycles = 8,
        .init_cmd_size = 2,
        .init_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00111000,
            .mask = 0b10111110,
        },
        .init_cmd[1] = {
            .addr = 0xb0,
            .data = 0b00010001,
            .mask = 0b00010001,
        },
        .unlock_cmd_size = 1,
        .unlock_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00000000,
            .mask = 0b10111110,
        },
    },

    {
        .id = 0x2004,//XS25D4GASL
        .plane_number = 1,
        .block_number = 4096,
        .page_number = 64,
        .page_size = 2048,
        .spi_io_params = {
            .port_cs_hd = 0,
            .port_clk_hd = 3,
            .port_do_hd = 0,
            .port_di_hd = 0,
            .port_d2_hd = 0,
            .port_d3_hd = 0,
        },
        .oob_info[0] = {
            .oob_offset = 0,
            .oob_size = 64,
        },
        .oob_info[1] = {
            .oob_offset = 0,
            .oob_size = 0,
        },
        .oob_size = 64,
        // 00 - 00 -- succ
        // 01 - 10 -- ecc fix
        // 10 , 11 -- ecc error
        // 11 , 11 -- ecc error
        .ecc_mask = 0b00110000,
        .ecc_status_1 = 0b1110,
        .ecc_status_0 = 0b1100,
        .write_enable_position = 0,
        .dummy_cycles = 8,
        .init_cmd_size = 2,
        .init_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00111000,
            .mask = 0b10111110,
        },
        .init_cmd[1] = {
            .addr = 0xb0,
            .data = 0b00010001,
            .mask = 0b00010001,
        },
        .unlock_cmd_size = 1,
        .unlock_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00000000,
            .mask = 0b10111110,
        },
    },
    {
        .id = 0x3cd1,//HSESYHDSW1G
        .plane_number = 1,
        .block_number = 1024,
        .page_number = 64,
        .page_size = 2048,
        .spi_io_params = {
            .port_cs_hd = 0,
            .port_clk_hd = 3,
            .port_do_hd = 0,
            .port_di_hd = 0,
            .port_d2_hd = 0,
            .port_d3_hd = 0,
        },
        .oob_info[0] = {
            .oob_offset = 0,
            .oob_size = 16,
        },
        .oob_info[1] = {
            .oob_offset = 0,
            .oob_size = 0,
        },
        .oob_size = 64,
        // 00 - 00 -- succ
        // 01 - 10 -- ecc fix
        // 10 , 11 -- ecc error
        // 11 , 00 -- succ
        .ecc_mask = 0b00110000,
        .ecc_status_1 = 0b0110,
        .ecc_status_0 = 0b0100,
        .write_enable_position = 0,
        .dummy_cycles = 8,
        .init_cmd_size = 2,
        .init_cmd[0] = {
            .addr = 0xa0,
            .data = 0b01111000,
            .mask = 0b11111111,
        },
        .init_cmd[1] = {
            .addr = 0xb0,
            .data = 0b00010000,
            .mask = 0b11010000,
        },
        .unlock_cmd_size = 1,
        .unlock_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00000000,
            .mask = 0b11111111,
        },
    },
    {
        .id = 0xe5f1,//DS35Q1GA
        .plane_number = 1,
        .block_number = 1024,
        .page_number = 64,
        .page_size = 2048,
        .spi_io_params = {
            .port_cs_hd = 0,
            .port_clk_hd = 3,
            .port_do_hd = 0,
            .port_di_hd = 0,
            .port_d2_hd = 0,
            .port_d3_hd = 0,
        },
        .oob_info[0] = {
            .oob_offset = 0,
            .oob_size = 64,
        },
        .oob_info[1] = {
            .oob_offset = 0,
            .oob_size = 0,
        },
        .oob_size = 64,
        // 000 - 00 -- succ
        // 001 - 00 -- ecc fix & succ
        // 010 , 11 -- ecc error
        // 011 , 10 -- ecc fix
        // 100 , 00 -- undef
        // 101 , 10 -- ecc fix
        .ecc_mask = 0b01110000,
        .ecc_status_1 = 0b101100,
        .ecc_status_0 = 0b000100,
        .write_enable_position = 0,
        .dummy_cycles = 8,
        .init_cmd_size = 2,
        .init_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00111000,  //wp all
            .mask = 0b10111110,
        },
        .init_cmd[1] = {
            .addr = 0xb0,
            .data = 0b00010001,
            .mask = 0b11010111,
        },
        .unlock_cmd_size = 1,
        .unlock_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00000000,
            .mask = 0b10111110,
        },
    },
    {
        .id = 0xe571,//DS35Q1GA-IB
        .plane_number = 1,
        .block_number = 1024,
        .page_number = 64,
        .page_size = 2048,
        .spi_io_params = {
            .port_cs_hd = 0,
            .port_clk_hd = 3,
            .port_do_hd = 0,
            .port_di_hd = 0,
            .port_d2_hd = 0,
            .port_d3_hd = 0,
        },
        .oob_info[0] = {
            .oob_offset = 0,
            .oob_size = 8,
        },
        .oob_info[1] = {
            .oob_offset = 0x10,
            .oob_size = 8,
        },
        .oob_size = 64,
        // 00 - 00 -- succ
        // 01 - 10 -- ecc fix
        // 10 , 11 -- ecc error
        // 11 , 00 -- undef
        .ecc_mask = 0b00110000,
        .ecc_status_1 = 0b000110,
        .ecc_status_0 = 0b000100,
        .write_enable_position = 0,
        .dummy_cycles = 8,
        .init_cmd_size = 2,
        .init_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00111000,  //wp all
            .mask = 0b10111110,
        },
        .init_cmd[1] = {
            .addr = 0xb0,
            .data = 0b00010001,
            .mask = 0b11010001,
        },
        .init_cmd[2] = {
            .addr = 0xd0, // 默认不操作
            .data = 0b00000000,
            .mask = 0b01100000,
        },
        .unlock_cmd_size = 1,
        .unlock_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00000000,
            .mask = 0b10111110,
        },
    },
    {
        .id = 0xc891,//GD5F1GM7UExxG
        .plane_number = 1,
        .block_number = 1024,
        .page_number = 64,
        .page_size = 2048,
        .spi_io_params = {
            .port_cs_hd = 0,
            .port_clk_hd = 3,
            .port_do_hd = 0,
            .port_di_hd = 0,
            .port_d2_hd = 0,
            .port_d3_hd = 0,
        },
        .oob_info[0] = {
            .oob_offset = 0,
            .oob_size = 64,
        },
        .oob_info[1] = {
            .oob_offset = 0,
            .oob_size = 0,
        },
        .oob_size = 64,
        // 00 - 00 -- succ
        // 01 - 01 -- ecc fix & succ
        // 10 , 11 -- ecc error
        // 11 , 10 -- ecc fix
        .ecc_mask = 0b00110000,
        .ecc_status_1 = 0b001100,
        .ecc_status_0 = 0b000110,
        .write_enable_position = 0,
        .dummy_cycles = 8,
        .init_cmd_size = 2,
        .init_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00111000,  //wp all
            .mask = 0b10111110,
        },
        .init_cmd[1] = {
            .addr = 0xb0,
            .data = 0b00010001,
            .mask = 0b11010001,
        },
        .unlock_cmd_size = 1,
        .unlock_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00000000,
            .mask = 0b10111110,
        },
    },
    {
        .id = 0x8c01,// XCSP1AAPK-IT
        .plane_number = 1,
        .block_number = 1024,
        .page_number = 64,
        .page_size = 2048,
        .spi_io_params = {
            .port_cs_hd = 0,
            .port_clk_hd = 3,
            .port_do_hd = 0,
            .port_di_hd = 0,
            .port_d2_hd = 0,
            .port_d3_hd = 0,
        },
        .oob_info[0] = {
            .oob_offset = 0,
            .oob_size = 64,
        },
        .oob_info[1] = {
            .oob_offset = 0,
            .oob_size = 0,
        },
        .oob_size = 64,
        // 00 - 00 -- succ
        // 01 - 01 -- ecc fix & succ
        // 10 , 11 -- ecc error
        // 11 , 10 -- ecc fix
        .ecc_mask = 0b00110000,
        .ecc_status_1 = 0b001100,
        .ecc_status_0 = 0b000110,
        .write_enable_position = 0,
        .dummy_cycles = 8,
        .init_cmd_size = 2,
        .init_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00111000,  //wp all
            .mask = 0b10111110,
        },
        .init_cmd[1] = {
            .addr = 0xb0,
            .data = 0b00010001,
            .mask = 0b11010001,
        },
        .unlock_cmd_size = 1,
        .unlock_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00000000,
            .mask = 0b10111110,
        },
    },
    {
        .id = 0x0b31,//XT26G01D
        .plane_number = 1,
        .block_number = 1024,
        .page_number = 64,
        .page_size = 2048,
        .spi_io_params = {
            .port_cs_hd = 0,
            .port_clk_hd = 3,
            .port_do_hd = 0,
            .port_di_hd = 0,
            .port_d2_hd = 0,
            .port_d3_hd = 0,
        },
        .oob_info[0] = {
            .oob_offset = 0,
            .oob_size = 64,
        },
        .oob_info[1] = {
            .oob_offset = 0,
            .oob_size = 0,
        },
        .oob_size = 64,
        // 0000 - 00 -- succ
        // 0001 - 01 -- ecc fix & succ
        // 0010 , 11 -- ecc error
        // 0011 , 10 -- ecc fix
        // 0100 - 00 -- succ
        // 0101 - 01 -- ecc fix & succ
        // 0110 , 11 -- ecc error
        // 0111 , 10 -- ecc fix
        // 1000 - 01 -- ecc fix & succ
        // 1001 - 01 -- ecc fix
        // 1010 , 11 -- ecc error
        // 1011 , 10 -- ecc fix
        // 1100 - 01 -- ecc fix
        // 1101 - 01 -- ecc fix
        // 1110 , 11 -- ecc error
        // 1111 , 10 -- ecc fix

        .ecc_mask = 0b11110000,
        .ecc_status_1 = 0b1100110011001100,
        .ecc_status_0 = 0b0111011101100110,
        .write_enable_position = 1,
        .dummy_cycles = 8,
        .init_cmd_size = 2,
        .init_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00111000,  //wp all
            .mask = 0b10111110,
        },
        .init_cmd[1] = {
            .addr = 0xb0,
            .data = 0b00010001,//HSE[1:1] =0: for random, 1:for sequence
            .mask = 0b11010011,
        },
        .init_cmd[2] = {
            .addr = 0xd0, // 默认不操作
            .data = 0b00100000, //drive strength[5:2] 00:25% 01:50% 10:75% 11:100%
            .mask = 0b01100000,
        },
        .unlock_cmd_size = 1,
        .unlock_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00000000,
            .mask = 0b10111110,
        },
    },
    {
        .id = 0x0b33,//XT26G02D
        .plane_number = 1,
        .block_number = 2048,
        .page_number = 64,
        .page_size = 2048,
        .spi_io_params = {
            .port_cs_hd = 0,
            .port_clk_hd = 3,
            .port_do_hd = 0,
            .port_di_hd = 0,
            .port_d2_hd = 0,
            .port_d3_hd = 0,
        },
        .oob_info[0] = {
            .oob_offset = 0,
            .oob_size = 64,
        },
        .oob_info[1] = {
            .oob_offset = 0,
            .oob_size = 0,
        },
        .oob_size = 64,
        // 0000 - 00 -- succ
        // 0001 - 01 -- ecc fix & succ
        // 0010 , 11 -- ecc error
        // 0011 , 10 -- ecc fix
        // 0100 - 00 -- succ
        // 0101 - 01 -- ecc fix & succ
        // 0110 , 11 -- ecc error
        // 0111 , 10 -- ecc fix
        // 1000 - 01 -- ecc fix & succ
        // 1001 - 01 -- ecc fix
        // 1010 , 11 -- ecc error
        // 1011 , 10 -- ecc fix
        // 1100 - 01 -- ecc fix
        // 1101 - 01 -- ecc fix
        // 1110 , 11 -- ecc error
        // 1111 , 10 -- ecc fix

        .ecc_mask = 0b11110000,
        .ecc_status_1 = 0b1100110011001100,
        .ecc_status_0 = 0b0111011101100110,
        .write_enable_position = 1,
        .dummy_cycles = 8,
        .init_cmd_size = 2,
        .init_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00111000,  //wp all
            .mask = 0b10111110,
        },
        .init_cmd[1] = {
            .addr = 0xb0,
            .data = 0b00010001,//HSE[1:1] =0: for random 1:for sequence
            .mask = 0b11010011,
        },
        .init_cmd[2] = {
            .addr = 0xd0, // 默认不操作
            .data = 0b00100000, //drive strength[5:2] 00:25% 01:50% 10:75% 11:100%
            .mask = 0b01100000,
        },
        .unlock_cmd_size = 1,
        .unlock_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00000000,
            .mask = 0b10111110,
        },
    },
    {
        .id = 0x5041,// ZB35Q01C
        .plane_number = 1,
        .block_number = 1024,
        .page_number = 64,
        .page_size = 2048,
        .spi_io_params = {
            .port_cs_hd = 0,
            .port_clk_hd = 3,
            .port_do_hd = 0,
            .port_di_hd = 0,
            .port_d2_hd = 0,
            .port_d3_hd = 0,
        },
        .oob_info[0] = {
            .oob_offset = 0,
            .oob_size = 11,
        },
        .oob_info[1] = {
            .oob_offset = 0x10,
            .oob_size = 11,
        },
        .oob_size = 64,
        // 00 - 00 -- succ
        // 01 - 01 -- ecc fix & succ
        // 10 , 11 -- ecc error
        // 11 , 10 -- ecc fix
        .ecc_mask = 0b00110000,
        .ecc_status_1 = 0b001100,
        .ecc_status_0 = 0b000110,
        .write_enable_position = 0,
        .dummy_cycles = 8,
        .init_cmd_size = 2,
        .init_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00111000,  //wp all
            .mask = 0b10111110,
        },
        .init_cmd[1] = {
            .addr = 0xb0,
            .data = 0b00010001,
            .mask = 0b11010001,
        },
        .unlock_cmd_size = 1,
        .unlock_cmd[0] = {
            .addr = 0xa0,
            .data = 0b00000000,
            .mask = 0b10111110,
        },
    },
};

