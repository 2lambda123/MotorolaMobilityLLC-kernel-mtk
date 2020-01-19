static u8 icnl9911_driver_builtin_firmware_hjc[] = {
#include"firmware/mdt/Moto-Blackjack-Chipone-ICNL9911S-MDT_Hmode_V0111_20200116.h"
};

static u8 icnl9911_driver_builtin_firmware_rs[] = {
#include"firmware/truly/Moto-Blackjack-Chipone-ICNL9911S-TRULY_Vmode_V0103_20191220.h"
};

const static struct cts_firmware cts_driver_builtin_firmwares_hjc[] = {
    {
        .name = "Ontim-Moto Blackjack+ hjc",      /* MUST set non-NULL */
        .hwid = CTS_DEV_HWID_ICNL9911S,
        .fwid = CTS_DEV_FWID_ICNL9911S,
        .data = icnl9911_driver_builtin_firmware_hjc,
        .size = ARRAY_SIZE(icnl9911_driver_builtin_firmware_hjc),
    },
};

const static struct cts_firmware cts_driver_builtin_firmwares_rs[] = {
    {
        .name = "Ontim-Moto Blackjack+ rs",      /* MUST set non-NULL */
        .hwid = CTS_DEV_HWID_ICNL9911S,
        .fwid = CTS_DEV_FWID_ICNL9911S,
        .data = icnl9911_driver_builtin_firmware_rs,
        .size = ARRAY_SIZE(icnl9911_driver_builtin_firmware_rs),
    },
};

