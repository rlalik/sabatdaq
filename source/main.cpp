// sabatdaq.cpp based on FERSlibDemo.cpp from CAEN JanusC package
// Copyright 2025 Rafał Lalik <Rafal.Lalik@uj.edu.pl>

#ifdef _WIN32
#    include <conio.h>
#    include <sys\timeb.h>
#    include <windows.h>
#else
#    include <ctype.h>
#    include <inttypes.h>
#    include <stdarg.h>
#    include <string.h>
#    include <sys/time.h>
#    include <time.h>
#    include <unistd.h>
#endif

#include <print>

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "config.h"
#include "lib.hpp"

// Stats Monitor
enum SMON
{
    CHTRG_RATE,
    CHTRG_CNT,
    PHA_RATE,
    PHA_CNT,
    HIT_RATE,
    HIT_CNT
};

// Plot type
enum PLOT_E_SPEC
{
    LG,
    HG
};

// #define GNUPLOTEXE  "pgnuplot"
#ifdef linux
constexpr auto GNUPLOTEXE = "gnuplot";  // Or '/usr/bin/gnuplot'
constexpr auto NULL_PATH = "/dev/null";
#    define popen popen
#    define pclose pclose
#else
constexpr auto GNUPLOTEXE = "..\\..\\..\\gnuplot\\pgnuplot.exe";
constexpr auto NULL_PATH = "nul";
#    define popen _popen /* redefine POSIX 'deprecated' popen as _popen */
#    define pclose _pclose /* redefine POSIX 'deprecated' pclose as _pclose */
#endif

/**************************************************************/
/*                  GLOBAL PARAMETERS                         */
/**************************************************************/
int stats_brd = 0, stats_ch = 0;
int stats_mon = 0, stats_plot = 0;

/*************************************************************/

auto print_menu() -> void
{
    std::print("\n\n-FERSlib_demo.exe:\tFERSlib demo executable\n");
    std::print("-Option:\tConfigFile.txt\n");
    std::print("\nIf no config file is provided, the default FERSlib_Config.txt in " "the same .exe folder will be used\n");
    std::print("\n\nExample:\n./FERSlib_demo.exe my_config.txt\n./FERSlib_demo.exe\n");
}

// ---------------------------------------------------------------------------------------------------------
// Local Functions
// ---------------------------------------------------------------------------------------------------------
int f_getch()
{
#ifdef _WIN32
    return _getch();
#else
    struct termios oldattr;
    if (tcgetattr(STDIN_FILENO, &oldattr) == -1) {
        perror(NULL);
    }
    struct termios newattr = oldattr;
    newattr.c_lflag &= ~(ICANON | ECHO);
    newattr.c_cc[VTIME] = 0;
    newattr.c_cc[VMIN] = 1;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &newattr) == -1) {
        perror(NULL);
    }
    const int ch = getchar();
    if (tcsetattr(STDIN_FILENO, TCSANOW, &oldattr) == -1) {
        perror(NULL);
    }
    return ch;
#endif
    //	unsigned char temp;
    //	raw();
    //    /* stdin = fd 0 */
    //	if(read(0, &temp, 1) != 1)
    //		return 0;
    //	return temp;
    // #endif
}

int f_kbhit()
{
#ifdef _WIN32
    return _kbhit();
#else
    struct termios oldattr;
    if (tcgetattr(STDIN_FILENO, &oldattr) == -1) {
        perror(NULL);
    }
    struct termios newattr = oldattr;
    newattr.c_lflag &= ~(ICANON | ECHO);
    newattr.c_cc[VTIME] = 0;
    newattr.c_cc[VMIN] = 1;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &newattr) == -1) {
        perror(NULL);
    }
    /* check stdin (fd 0) for activity */
    fd_set read_handles;
    FD_ZERO(&read_handles);
    FD_SET(0, &read_handles);
    struct timeval timeout;
    timeout.tv_sec = timeout.tv_usec = 0;
    int status = select(0 + 1, &read_handles, NULL, NULL, &timeout);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &oldattr) == -1) {
        perror(NULL);
    }
    if (status < 0) {
        std::print("select() failed in kbhit()\n");
        exit(1);
    }
    return status;
#endif
}

// Convert a double in a string with unit (k, M, G)
auto double2str(double f, int space) -> std::string
{
    if (!space) {
        if (f <= 999.999) {
            return std::format("{:7.3f} ", f);
        } else if (f <= 999999) {
            return std::format("{:7.3f}k", f / 1e3);
        } else if (f <= 999999000) {
            return std::format("{:7.3f}M", f / 1e6);
        } else {
            return std::format("{:7.3f}G", f / 1e9);
        }
    } else {
        if (f <= 999.999) {
            return std::format("{:7.3f} ", f);
        } else if (f <= 999999) {
            return std::format("{:7.3f} k", f / 1e3);
        } else if (f <= 999999000) {
            return std::format("{:7.3f} M", f / 1e6);
        } else {
            return std::format("{:7.3f} G", f / 1e9);
        }
    }
}

auto cnt2str(uint64_t c, char* s) -> std::string
{
    if (c <= 9999999) {
        return std::format("{:7d} ", (uint32_t)c);
    } else if (c <= 9999999999) {
        return std::format("{:7d}k", (uint32_t)(c / 1000));
    } else {
        return std::format("{:7d}M", (uint32_t)(c / 1000000));
    }
}

void ClearScreen()
{
    std::print("\033[2J");  // ClearScreen
    std::print("{:c}[{:d};{:d}f", 0x1B, 0, 0);
}

void gotoxy(int x, int y)
{
    std::print("{:c}[{:d};{:d}f", 0x1B, y, x);  // goto(x,y)
}

int CheckRunTimeCmd(sabatdaq::sabat_daq* daq, sabatdaq::DAQ_t* tcfg)
{
    int c = 0;
    char input_name[1024], input_val[1024];
    int r = 0;
    int brd = 0;
    if (!f_kbhit()) {
        return 0;
    }
    c = f_getch();

    if (c == 'q') {
        tcfg->Quit = 1;
        return 1;
    }
    if (c == 's') {
        r = daq->StartRun(tcfg);
        return 1;
    }
    if (c == 'S') {
        r = daq->StopRun(tcfg);
        return 1;
    }
    if (c == 'w' && tcfg->acq_status == sabatdaq::ACQSTATUS::READY) {
        // Clear Screen
        std::print("* SET PARAMETER FUNCTION\n\n");
        std::print("Enter board idx [0-{:d}]: ", tcfg->num_brd - 1);
        r = scanf("%d", &brd);
        if (brd < 0 or brd > (tcfg->num_brd - 1)) {
            std::print("Board index out of range\n");
            return FERSLIB_ERR_OPER_NOT_ALLOWED;
        }
        std::print("Enter parameter name: ");
        r = scanf("%s", input_name);
        std::print("Enter parameter value: ");
        r = scanf("%s", input_val);
        r = FERS_SetParam(daq->get_handles()[brd], input_name, input_val);
        if (r != 0) {
            //! [LastError]
            char emsg[1024];
            FERS_GetLastError(emsg);
            //! [LastError]
            std::print(stderr, "ERROR: {:s}\n", emsg);
            return r;
        }
        return 1;  // Parameter set, configure
    }
    if (c == 'r' && tcfg->acq_status == sabatdaq::ACQSTATUS::READY) {
        // Get from keyboard param_name
        // Clear Screen
        std::print("* GET PARAMETER FUNCTION\n\n");
        std::print("Enter board idx [0-{:d}]: ", tcfg->num_brd - 1);
        auto ret = scanf("%d", &brd);
        if (brd < 0 or brd > (tcfg->num_brd - 1)) {
            std::print("Board index out of range\n");
            return FERSLIB_ERR_OPER_NOT_ALLOWED;
        }
        std::print("Enter parameter name: ");
        ret = scanf("%s", input_name);
        r = FERS_GetParam(daq->get_handles()[brd], input_name, input_val);
        if (r != 0) {
            char description[1024];
            FERS_GetLastError(description);
            std::print("ERROR: {:s}\n", description);
            return FERSLIB_ERR_GENERIC;
        }
        std::print("\n{:s} = {:s}\n", input_name, input_val);
        return 1;
    }
    if (c == 'h' && tcfg->acq_status == sabatdaq::ACQSTATUS::READY) {
        // histograms
        std::print("\n\nSelect histo binning\n");
        std::print("0 = 256\n");
        std::print("1 = 512\n");
        std::print("2 = 1024\n");
        std::print("3 = 2048\n");
        std::print("4 = 4096\n");
        std::print("5 = 8192\n");
        c = f_getch() - '0';
        if (c == '0') {
            tcfg->EHistoNbin = 256;
        } else if (c == '1') {
            tcfg->EHistoNbin = 512;
        } else if (c == '2') {
            tcfg->EHistoNbin = 1024;
        } else if (c == '3') {
            tcfg->EHistoNbin = 2048;
        } else if (c == '4') {
            tcfg->EHistoNbin = 4096;
        } else if (c == '5') {
            tcfg->EHistoNbin = 8192;
        }
        return 4;
    }
    if (c == 'p') {
        std::print("\n\nSelect plot\n");
        std::print("0 = Spect Low Gain\n");
        std::print("1 = Spect High Gain\n");
        c = f_getch() - '0';
        if ((c >= 0) && (c < 2)) {
            stats_plot = c;
        }
        return 1;
    }
    if (c == 'b') {
        int new_brd;
        std::print("Current Active Board = {:d}\n", stats_brd);
        std::print("New Active Board = ");
        auto ret = scanf("%d", &new_brd);

        if ((new_brd >= 0) && (new_brd < tcfg->num_brd)) {
            stats_brd = new_brd;
            std::print("Active Board = {:d}\n", stats_brd);
        }
        return 1;
    }
    if (c == 'c') {
        int new_ch;
        char chs[10];
        std::print("Current Active Channel = {:d}\n", stats_ch);
        std::print("New Active Channel ");
        auto ret = scanf("%s", chs);
        if (isdigit(chs[0])) {
            sscanf(chs, "%d", &new_ch);
        }
        if ((new_ch >= 0) && (new_ch < FERSLIB_MAX_NCH_5202)) {
            stats_ch = new_ch;
        }
        return 1;
    }
    if (c == 'm') {
        std::print("\n\nSelect statistics monitor\n");
        std::print("0 = ChTrg Rate\n");
        std::print("1 = ChTrg Cnt\n");
        std::print("2 = PHA Rate\n");
        std::print("3 = PHA Cnt\n");
        std::print("4 = Hit Rate\n");
        std::print("5 = Hit Cnt\n");
        c = f_getch() - '0';
        if ((c >= 0) && (c <= 5)) {
            stats_mon = c;
        }
        return 1;
    }
    if (c == 'C' && tcfg->acq_status == sabatdaq::ACQSTATUS::READY) {
        return 2;
    }
    if (c == 'l' && tcfg->acq_status == sabatdaq::ACQSTATUS::READY) {
        return 3;
    }
    if (c == ' ') {
        std::print("\n");
        std::print("[q] Quit\n");
        std::print("[s] Start Acquisition\n");
        std::print("[S] Stop Acquisition\n");
        std::print("[C] Configure FERS (not available when board is in RUN)\n");
        std::print("[l] Load configuration from file (not available when board is in " "RUN)\n");
        std::print("[w] Set Param (not available when board is in RUN)\n");
        std::print("[r] Get Param (not available when board is in RUN)\n");
        std::print("[b] Change board\n");
        std::print("[c] Change channel\n");
        std::print("[m] Set StatsMonitor Type\n");
        std::print("[h] Set Histo binning (not available when board is in RUN)\n");
        std::print("[p] Set Plot Mode\n");
        return 1;
    }
    return 0;
}

auto QuitProgram(sabatdaq::sabat_daq& daq, sabatdaq::DAQ_t& cfg) -> void
{
    daq.StopRun(&cfg);
    for (int b = 0; b < cfg.num_brd; ++b) {
        if (daq.get_handles()[0] < 0) {
            break;
        }
        //! [CloseReadout]
        FERS_CloseReadout(daq.get_handles()[0]);
        //! [CloseReadout]

        //! [Disconnect]
        FERS_CloseDevice(daq.get_handles()[0]);
        //! [Disconnect]
    }

    for (int b = 0; b < FERSLIB_MAX_NCNC; ++b) {
        if (daq.get_cnc_handles()[b] < 0) {
            break;
        }
        FERS_CloseDevice(daq.get_cnc_handles()[0]);
    }
}

int main(int argc, char* argv[])
{
    FILE *fcfg, *plotter;
    sabatdaq::sabat_daq daq;
    sabatdaq::DAQ_t this_cfg;

    FERS_BoardInfo_t BoardInfo[FERSLIB_MAX_NBRD];

    float FiberDelayAdjust[FERSLIB_MAX_NCNC][FERSLIB_MAX_NTDL][FERSLIB_MAX_NNODES] = {0};

    int AcqMode = 0;
    int tmp_brd = 0, tmp_ch = 0;
    int clrscr = 0, dtq, ch, b;

    std::print("**************************************************************\n");
    std::print("SabatDAQ v{:s}\n", RELEASE_NUM);
    std::print("FERSlib v{:s}\n", FERS_GetLibReleaseNum());
    // std::print("\nWith this demo, you can perform the following:\n");
    // std::print(" - Open and configure FERS module\n");
    // std::print(" - Read an event\n");
    // std::print(" - Plot and collect statistics for Spectroscopy and Timing Common " "Start mode(Timing mode not yet implemented)");
    // std::print("\nFor more specific usage, refer to the Janus software version " "related to the FERS module you use\n");
    std::print("**************************************************************\n");

    // std::print("\n\n{:s}\n{:d}\n", argv[0], argc);

    std::string cfg_file = DEFAULT_CONFIG_FILENAME;
    if (argc == 1) {  // No options provided
        fcfg = fopen(DEFAULT_CONFIG_FILENAME, "r");
        if (fcfg == NULL) {
            std::print(
                "No config file provided and no default one {:s} found. Exiting " "(ret={:d})\n",
                DEFAULT_CONFIG_FILENAME,
                (int)FERSLIB_ERR_INVALID_PATH);
            print_menu();
            return FERSLIB_ERR_INVALID_PATH;
        }
    } else if (argc == 2) {
        if (strcmp(argv[1], "-h") == 0 or strcmp(argv[1], "--help") == 0) {
            print_menu();
            return 0;
        }
        fcfg = fopen(argv[1], "r");
        if (fcfg == NULL) {
            std::print("No valid config file name {:s} provided. Exiting (ret={:d})\n", argv[1], (int)FERSLIB_ERR_INVALID_PATH);
            return FERSLIB_ERR_INVALID_PATH;
        }
        cfg_file = argv[1];
    } else {
        return FERSLIB_ERR_GENERIC;
    }

    // OPEN CFG FILE
    this_cfg.num_brd = get_brd_path_from_file(fcfg, &this_cfg);
    fseek(fcfg, 0, SEEK_SET);
    get_delayfiber_from_file(fcfg, &this_cfg);
    fclose(fcfg);
    // OPEN BOARDS

    for (int b = 0, cnc = 0; b < this_cfg.num_brd; b++) {
        char *cc, cpath[100];
        if (((cc = strstr(this_cfg.brd_path[b], "tdl")) != NULL)) {  // TDlink used => Open connection to concentrator (this is not
                                                                     // mandatory, it is done for reading information about the
                                                                     // concentrator)
            FERS_Get_CncPath(this_cfg.brd_path[b], cpath);
            if (!FERS_IsOpen(cpath)) {
                FERS_CncInfo_t CncInfo;
                std::print("\n--------------- Concentrator {:2d} ----------------\n", cnc);
                std::print("Opening connection to {:s}\n", cpath);
                int ret = FERS_OpenDevice(cpath, &daq.get_cnc_handles()[cnc]);
                if (ret == 0) {
                    std::print("Connected to Concentrator {:s}\n", cpath);
                } else {
                    std::print("Can't open concentrator at {:s}\n", cpath);
                    return FERSLIB_ERR_GENERIC;
                }
                if (!FERS_TDLchainsInitialized(daq.get_cnc_handles()[cnc])) {
                    std::print("Initializing TDL chains. This will take a few " "seconds...\n");
                    ret = FERS_InitTDLchains(daq.get_cnc_handles()[cnc], this_cfg.FiberDelayAdjust[cnc]);
                    if (ret != 0) {
                        std::print("Failure in TDL chain init\n");
                        return FERSLIB_ERR_TDL_ERROR;
                    }
                }
                ret |= FERS_ReadConcentratorInfo(daq.get_cnc_handles()[cnc], &CncInfo);
                if (ret == 0) {
                    std::print("FPGA FW revision = {:s}\n", CncInfo.FPGA_FWrev);
                    std::print("SW revision = {:s}\n", CncInfo.SW_rev);
                    std::print("PID = {:d}\n", CncInfo.pid);
                    if (CncInfo.ChainInfo[0].BoardCount == 0) {  // Rising error if no board is connected to link 0
                        std::print("No board connected to link 0\n");
                        return FERSLIB_ERR_TDL_ERROR;
                    }
                    for (int i = 0; i < 8; i++) {
                        if (CncInfo.ChainInfo[i].BoardCount > 0) {
                            std::print("Found {:d} board(s) connected to TDlink n. {:d}\n", CncInfo.ChainInfo[i].BoardCount, i);
                        }
                    }
                } else {
                    std::print("Can't read concentrator info\n");
                    return FERSLIB_ERR_GENERIC;
                }
                cnc++;
            }
        }
        if ((this_cfg.num_brd > 1) || (cnc > 0)) {
            std::print("\n------------------ Board {:2d} --------------------\n", b);
        }
        std::print("Opening connection to {:s}\n", this_cfg.brd_path[b]);
        char BoardPath[500];
        strcpy(BoardPath, this_cfg.brd_path[b]);
        //! [LastErrorOpen]
        //! [Connect]
        int ret = FERS_OpenDevice(BoardPath, &daq.get_handles()[b]);
        //! [Connect]
        if (ret != 0) {
            char emsg[1024];
            FERS_GetLastError(emsg);
            std::print("Can't open board: {:s} (ret={:d})\n", emsg, ret);
        }
        //! [LastErrorOpen]
        else
        {
            std::print("Connected to {:s}\n", this_cfg.brd_path[b]);
        }
        // PRINT BOARD INFO
        ret = FERS_GetBoardInfo(daq.get_handles()[b], &BoardInfo[b]);
        if (ret == 0) {
            std::string fver;
            if (BoardInfo[b].FPGA_FWrev == 0) {
                fver = std::format("BootLoader");
            } else {
                fver = std::format("{:d}.{:d} (Build = {:04X})",
                                   (BoardInfo[b].FPGA_FWrev >> 8) & 0xFF,
                                   BoardInfo[b].FPGA_FWrev & 0xFF,
                                   (BoardInfo[b].FPGA_FWrev >> 16) & 0xFFFF);
            }
            int MajorFWrev = min((int)(BoardInfo[b].FPGA_FWrev >> 8) & 0xFF, MajorFWrev);
            std::print("FPGA FW revision = {:s}\n", fver);
            if (strstr(this_cfg.brd_path[b], "tdl") == NULL) {
                std::print("uC FW revision = {:08X}\n", BoardInfo[b].uC_FWrev);
            }
            std::print("PID = {:d}\n", BoardInfo[b].pid);
        } else {
            std::print("Can't read board info\n");
            return FERSLIB_ERR_GENERIC;
        }
    }

LoadConfigFERS:
    //! [ParseFile]
    int ret = FERS_LoadConfigFile(const_cast<char*>(cfg_file.c_str()));
    //! [ParseFile]
    if (ret != 0) {
        std::print("Cannot load FERS configuration from file {:s}\n", cfg_file);
    }

    // Get Acquisition Mode
    char AcqModeVal[32];
    char ParName[32] = "AcquisitionMode";
    ret = FERS_GetParam(daq.get_handles()[0], ParName, AcqModeVal);
    sscanf(AcqModeVal, "%d", &AcqMode);

    daq.InitReadout(&this_cfg);

    this_cfg.EHistoNbin = 4096;  // 256 - 512 - 1024 - 2048 - 4096 - 8192
    daq.CreateStatistics(this_cfg.num_brd, FERSLIB_MAX_NCH_5202, this_cfg.EHistoNbin);

    // OPEN PLOTTER
    char sstr[200];
    strcpy(sstr, "");
    strcat(sstr, GNUPLOTEXE);

#ifndef _WIN32
    strcat(sstr, " 2> ");
    strcat(sstr, NULL_PATH);
#endif
    plotter = popen(sstr, "w");  // Open plotter pipe (gnuplot)
    if (plotter == NULL) {
        std::print("Can't open gnuplot at {:s}\n", GNUPLOTEXE);
    }
    std::print(plotter, "set grid\n");
    std::print(plotter, "set mouse\n");

// CONFIGURE FROM FILE
ConfigFERS:
    std::print("Configuring boards ...\n");
    for (int b = 0; b < this_cfg.num_brd; ++b) {
        //! [Configure]
        ret = FERS_configure(daq.get_handles()[0], CFG_HARD);
        //! [Configure]
        if (ret != 0) {
            std::print("Board {:d} failed!!!\n", b);
        } else {
            std::print("Board {:d} configured\n", b);
        }
        char fdname[512];
        FERS_DumpBoardRegister(daq.get_handles()[0]);
        FERS_DumpCfgSaved(daq.get_handles()[0]);
    }
    FERS_SetEnergyBitsRange(0);

    daq.ResetStatistics();

    this_cfg.acq_status = sabatdaq::ACQSTATUS::READY;
    auto curr_time = sabatdaq::get_time();
    auto print_time = curr_time;  // force 1st print with no delay
    auto kb_time = curr_time;
    stats_mon = SMON::PHA_RATE;
    stats_plot = PLOT_E_SPEC::LG;
    stats_brd = 0;
    stats_ch = 0;

    int rdymsg = 1;
    while (!this_cfg.Quit) {
        curr_time = sabatdaq::get_time();
        daq.get_stats().current_time = curr_time;

        if ((curr_time - kb_time) > 200) {
            int code = CheckRunTimeCmd(&daq, &this_cfg);
            if (code > 0) {  // Cmd executed
                if (this_cfg.Quit) {
                    // goto QuitProgram;
                    QuitProgram(daq, this_cfg);
                    pclose(plotter);
                    std::print("Quitting ...\n");
                    return 0;
                }
                if (code == 2) {
                    clrscr = 1;
                    goto LoadConfigFERS;
                }
                if (code == 3) {
                    clrscr = 1;
                    goto ConfigFERS;
                }
                if (code == 4) {
                    clrscr = 1;
                    daq.DestroyStatistics();
                    daq.CreateStatistics(this_cfg.num_brd, FERSLIB_MAX_NCH_5202, this_cfg.EHistoNbin);
                }
                if (code == 1) {
                    print_time = curr_time;
                    rdymsg = 1;
                    clrscr = 1;
                }
            }
            kb_time = curr_time;
        }

        if (this_cfg.acq_status == sabatdaq::ACQSTATUS::READY) {
            if (rdymsg) {
                std::print("Press [s] to start run, [q] to quit, [SPACE] to enter the " "menu\n\n");
                rdymsg = 0;
            }
            // continue;
        }

        if (this_cfg.acq_status == sabatdaq::ACQSTATUS::RUNNING) {
            double tstamp_us {0}, curr_tstamp_us {0};
            void* Event {nullptr};
            auto nb = 0;
            //! [GetEvent]
            ret = FERS_GetEvent(daq.get_handles(), &b, &dtq, &tstamp_us, &Event, &nb);
            //! [GetEvent]
            if (nb > 0) {
                curr_tstamp_us = tstamp_us;
            }
            if (ret == 0) {
                std::print("ERROR: Readout failure (ret = {:d}).\nStopping Data " "Acquisition ...\n", ret);
                daq.StopRun(&this_cfg);
            }
            if (nb > 0) {
                daq.get_stats().current_tstamp_us[b] = curr_tstamp_us;
                daq.get_stats().ByteCnt[b].cnt += (uint64_t)nb;
                daq.get_stats().GlobalTrgCnt[b].cnt++;
                if ((dtq & 0xF) == DTQ_SPECT) {
                    //! [CastEvent]
                    SpectEvent_t* Ev = (SpectEvent_t*)Event;
                    //! [CastEvent]
                    int ediv = 1;  // this_cfg.EHistoNbin;
                    daq.get_stats().current_trgid[b] = Ev->trigger_id;
                    ediv = ((1 << 13) / this_cfg.EHistoNbin);
                    if (ediv < 1) {
                        ediv = 1;
                    }
                    for (ch = 0; ch < FERSLIB_MAX_NCH_5202; ++ch) {
                        uint16_t EbinLG = Ev->energyLG[ch] / ediv;
                        uint16_t EbinHG = Ev->energyHG[ch] / ediv;
                        if (this_cfg.EHistoNbin > 0) {
                            if (EbinLG > 0) {
                                sabatdaq::AddCount(&daq.get_stats().H1_PHA_LG[b][ch], EbinLG);
                            }
                            if (EbinHG > 0) {
                                sabatdaq::AddCount(&daq.get_stats().H1_PHA_HG[b][ch], EbinHG);
                            }
                        }
                        if ((EbinLG > 0) || (EbinHG > 0)) {
                            daq.get_stats().PHACnt[b][ch].cnt++;
                        }
                        // std::cout << " HG: " << Ev->energyHG[0] << std::endl;
                    }
                    // std::print(" Trigger ID: %" PRIu64 "  LG[0]: %" PRIu16 "
                    // HG[0]: % " PRIu16 "\n", Ev->trigger_id, Ev->energyLG[0],
                    // Ev->energyHG[0]);

                } else if ((dtq & 0x0F) == DTQ_TIMING) {  // 5202+5203 Need to give some statistics
                    ListEvent_t* Ev = (ListEvent_t*)Event;
                    if (AcqMode == ACQMODE_COMMON_START) {
                        for (int s = 0; s < Ev->nhits; s++) {
                            daq.get_stats().ReadHitCnt[b][ch].cnt++;
                        }
                    }
                } else if (dtq == DTQ_COUNT) {  // 5202 Only
                    CountingEvent_t* Ev = (CountingEvent_t*)Event;
                    daq.get_stats().current_trgid[b] = Ev->trigger_id;
                    for (auto i = 0; i < FERSLIB_MAX_NCH_5202; i++) {
                        daq.get_stats().ChTrgCnt[b][i].cnt += Ev->counts[i];
                    }
                    daq.get_stats().T_OR_Cnt[b].cnt += Ev->t_or_counts;
                    daq.get_stats().Q_OR_Cnt[b].cnt += Ev->q_or_counts;
                    daq.get_stats().trgcnt_update_us[b] = curr_tstamp_us;
                } else if (dtq == DTQ_WAVE) {  // 5202 Only and not implemented
                    WaveEvent_t* Ev;
                } else if (dtq == DTQ_SERVICE) {
                    ServEvent_t* Ev = (ServEvent_t*)Event;
                    // memcpy(&sEvt[b], Ev, sizeof(ServEvent_t)); FIXME check below
                    daq.get_service_events()[b] = *Ev;
                    for (ch = 0; ch < FERSLIB_MAX_NCH_5202; ch++) {
                        daq.get_stats().ChTrgCnt[b][ch].cnt += Ev->ch_trg_cnt[ch];
                    }

                    daq.get_stats().T_OR_Cnt[b].cnt += Ev->t_or_cnt;
                    daq.get_stats().Q_OR_Cnt[b].cnt += Ev->q_or_cnt;
                    daq.get_stats().trgcnt_update_us[b] = (double)Ev->update_time * 1000;
                }
                // Count lost triggers (per board)
                if (dtq != DTQ_SERVICE) {
                    if ((daq.get_stats().current_trgid[b] > 0)
                        && (daq.get_stats().current_trgid[b] > (daq.get_stats().previous_trgid[b] + 1)))
                    {
                        daq.get_stats().LostTrg[b].cnt +=
                            ((uint32_t)daq.get_stats().current_trgid[b] - (uint32_t)daq.get_stats().previous_trgid[b] - 1);
                    }
                    daq.get_stats().previous_trgid[b] = daq.get_stats().current_trgid[b];
                }
            }
        }
        // Statistcs and Plot
        if ((curr_time - print_time) > 1000) {
            if (this_cfg.acq_status == sabatdaq::ACQSTATUS::RUNNING) {
                if (clrscr) {
                    ClearScreen();
                    clrscr = 0;
                }
                gotoxy(1, 1);
            }

            for (int bb = 0; bb < this_cfg.num_brd; ++bb) {
                daq.UpdateServiceInfo(bb);
            }

            std::string ss[FERSLIB_MAX_NCH_5202], totror, ror, trr;
            auto rtime = (float)(curr_time - daq.get_stats().start_time) / 1000;
            if (this_cfg.acq_status == sabatdaq::ACQSTATUS::RUNNING) {
                daq.UpdateStatistics(0);
                double totrate = 0;
                for (auto i = 0; i < this_cfg.num_brd; ++i) {
                    totrate += daq.get_stats().ByteCnt[i].rate;
                    totror = double2str(totrate, 1);
                }

                std::print("Elapsed Time: {:10.2f} s\n", rtime);

                ror = double2str(daq.get_stats().ByteCnt[0].rate, 1);
                trr = double2str(daq.get_stats().GlobalTrgCnt[0].rate, 1);

                // Select Monitor Type
                for (auto i = 0; i < FERSLIB_MAX_NCH_5202; ++i) {
                    if (stats_mon == SMON::CHTRG_RATE) {
                        ss[i] = double2str(daq.get_stats().ChTrgCnt[0][i].rate, 0);
                    }
                    if (stats_mon == SMON::PHA_RATE) {
                        ss[i] = double2str(daq.get_stats().PHACnt[0][i].rate, 0);
                    }
                    if (stats_mon == SMON::HIT_RATE) {
                        ss[i] = double2str(daq.get_stats().ReadHitCnt[0][i].rate, 0);  // 5203 Only
                    }

                    if (stats_mon == SMON::CHTRG_CNT) {
                        ss[i] = double2str((double)daq.get_stats().ChTrgCnt[0][i].cnt, 0);
                    }
                    if (stats_mon == SMON::PHA_CNT) {
                        ss[i] = double2str((double)daq.get_stats().PHACnt[0][i].cnt, 0);
                    }
                    if (stats_mon == SMON::HIT_CNT) {
                        ss[i] = double2str((double)daq.get_stats().ReadHitCnt[0][i].cnt, 0);  // 5203 Only
                    }
                }

                // Print Global statistics
                if (this_cfg.num_brd > 1) {
                    std::print("Board n. {:d} (press [b] to change active board)\n", 0);
                }
                std::print("Time Stamp:   {:10.3f} s                \n", daq.get_stats().current_tstamp_us[0] / 1e6);
                std::print("Trigger-ID:   {:10d}            \n", daq.get_stats().current_trgid[0]);
                std::print("Trg Rate:        {:s}cps                 \n", trr);
                std::print("Trg Reject:   {:10.2f} %               \n", daq.get_stats().LostTrgPerc[0]);
                std::print("Tot Lost Trg: {:10d}            \n", daq.get_stats().LostTrg[0].cnt);
                if (this_cfg.num_brd > 1) {
                    std::print("Readout Rate:    {:s}B/s (Tot: {:s}B/s)             \n", ror, totror);
                } else {
                    std::print("Readout Rate:    {:s}B/s                   \n", ror);
                }
                std::print("Temp (degC):     FPGA={:4.1f}              \n", daq.get_board_temp(0, sabatdaq::TEMP::FPGA));
                std::print("\n");
                // Print channels statistics
                for (auto i = 0; i < FERSLIB_MAX_NCH_5202; i++) {
                    std::print("{:02d}: {:s}     ", i, ss[i]);
                    if ((i & 0x3) == 0x3) {
                        std::print("\n");
                    }
                }
                std::print("\n");
            }

            ////////////////////////////////////
            // Plot - only for PHA LG/HG 5202 //
            ////////////////////////////////////
            if (this_cfg.acq_status == sabatdaq::ACQSTATUS::RUNNING || stats_brd != tmp_brd || stats_ch != tmp_ch) {
                tmp_brd = stats_brd;
                tmp_ch = stats_ch;
                std::string ptitle;
                FILE* hist_file;
                hist_file = fopen("hist.txt", "w");
                for (int j = 0; j < this_cfg.EHistoNbin; ++j) {
                    if (stats_plot == PLOT_E_SPEC::LG) {
                        std::print(hist_file, "{:f}\t{:d}\n", float(j), daq.get_stats().H1_PHA_LG[stats_brd][stats_ch].H_data[j]);
                        ptitle = std::format("Spect LG");
                    } else if (stats_plot == PLOT_E_SPEC::HG) {
                        std::print(hist_file, "{:f}\t{:d}\n", float(j), daq.get_stats().H1_PHA_HG[stats_brd][stats_ch].H_data[j]);
                        ptitle = std::format("Spect HG");
                    }
                }
                fclose(hist_file);
                if (plotter) {
                    std::print(plotter, "set xlabel 'Channels'\n");
                    std::print(plotter, "set ylabel 'Counts'\n");
                    std::print(plotter, "set title '{:s}'\n", ptitle);
                    std::print(plotter, "plot 'hist.txt' w boxes fs solid 0.7 title " "'Brd:{:d} Ch:{:d}'\n", stats_brd, stats_ch);
                    fflush(plotter);
                }
            }
            print_time = curr_time;
        }
    }

    return 0;
}
