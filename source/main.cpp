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

#include "lib.hpp"

#define DEFAULT_CONFIG_FILENAME "SabatDAQ_Config.txt"

// Stats Monitor
#define SMON_CHTRG_RATE 0
#define SMON_CHTRG_CNT 1
#define SMON_PHA_RATE 2
#define SMON_PHA_CNT 3
#define SMON_HIT_RATE 4
#define SMON_HIT_CNT 5

// Plot type
#define PLOT_E_SPEC_LG 0
#define PLOT_E_SPEC_HG 1

#ifdef linux
#    define myscanf _scanf
#else
#    define myscanf scanf
#endif

// #define GNUPLOTEXE  "pgnuplot"
#ifdef linux
#    define GNUPLOTEXE "gnuplot"  // Or '/usr/bin/gnuplot'
#    define NULL_PATH "/dev/null"
#    define popen popen
#    define pclose pclose
#else
#    define GNUPLOTEXE "..\\..\\..\\gnuplot\\pgnuplot.exe"
#    define NULL_PATH "nul"
#    define popen _popen /* redefine POSIX 'deprecated' popen as _popen */
#    define pclose _pclose /* redefine POSIX 'deprecated' pclose as _pclose */
#endif

/**************************************************************/
/*                  GLOBAL PARAMETERS                         */
/**************************************************************/
char description[1024];
int cnc_handle[FERSLIB_MAX_NCNC];

int stats_brd = 0, stats_ch = 0;
int stats_mon = 0, stats_plot = 0;

uint64_t CurrentTime, PrevKbTime, PrevRateTime, ElapsedTime, StartTime, StopTime;

/*************************************************************/

int print_menu()
{
    printf("\n\n-FERSlib_demo.exe:\tFERSlib demo executable\n");
    printf("-Option:\tConfigFile.txt\n");
    printf("\nIf no config file is provided, the default FERSlib_Config.txt in " "the same .exe folder will be used\n");
    printf("\n\nExample:\n./FERSlib_demo.exe my_config.txt\n./FERSlib_demo.exe\n");

    return 0;
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
        printf("select() failed in kbhit()\n");
        exit(1);
    }
    return status;
#endif
}

#ifdef linux
// ----------------------------------------------------
//
// ----------------------------------------------------
int _scanf(char* fmt, ...)
{
    int ret;
    va_list args;
    va_start(args, fmt);
    ret = vscanf(fmt, args);
    va_end(args);
    return ret;
}
#endif

// Convert a double in a string with unit (k, M, G)
void double2str(double f, int space, char* s)
{
    if (!space) {
        if (f <= 999.999) {
            sprintf(s, "%7.3f ", f);
        } else if (f <= 999999) {
            sprintf(s, "%7.3fk", f / 1e3);
        } else if (f <= 999999000) {
            sprintf(s, "%7.3fM", f / 1e6);
        } else {
            sprintf(s, "%7.3fG", f / 1e9);
        }
    } else {
        if (f <= 999.999) {
            sprintf(s, "%7.3f ", f);
        } else if (f <= 999999) {
            sprintf(s, "%7.3f k", f / 1e3);
        } else if (f <= 999999000) {
            sprintf(s, "%7.3f M", f / 1e6);
        } else {
            sprintf(s, "%7.3f G", f / 1e9);
        }
    }
}

void cnt2str(uint64_t c, char* s)
{
    if (c <= 9999999) {
        sprintf(s, "%7d ", (uint32_t)c);
    } else if (c <= 9999999999) {
        sprintf(s, "%7dk", (uint32_t)(c / 1000));
    } else {
        sprintf(s, "%7dM", (uint32_t)(c / 1000000));
    }
}

void ClearScreen()
{
    printf("\033[2J");  // ClearScreen
    printf("%c[%d;%df", 0x1B, 0, 0);
}

void gotoxy(int x, int y)
{
    printf("%c[%d;%df", 0x1B, y, x);  // goto(x,y)
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
        printf("* SET PARAMETER FUNCTION\n\n");
        printf("Enter board idx [0-%d]: ", tcfg->num_brd - 1);
        r = scanf("%d", &brd);
        if (brd < 0 or brd > (tcfg->num_brd - 1)) {
            printf("Board index out of range\n");
            return FERSLIB_ERR_OPER_NOT_ALLOWED;
        }
        printf("Enter parameter name: ");
        r = scanf("%s", input_name);
        printf("Enter parameter value: ");
        r = scanf("%s", input_val);
        r = FERS_SetParam(daq->get_handles()[brd], input_name, input_val);
        if (r != 0) {
            //! [LastError]
            char emsg[1024];
            FERS_GetLastError(emsg);
            //! [LastError]
            printf("ERROR: %s\n", emsg);
            return r;
        }
        return 1;  // Parameter set, configure
    }
    if (c == 'r' && tcfg->acq_status == sabatdaq::ACQSTATUS::READY) {
        // Get from keyboard param_name
        // Clear Screen
        printf("* GET PARAMETER FUNCTION\n\n");
        printf("Enter board idx [0-%d]: ", tcfg->num_brd - 1);
        scanf("%d", &brd);
        if (brd < 0 or brd > (tcfg->num_brd - 1)) {
            printf("Board index out of range\n");
            return FERSLIB_ERR_OPER_NOT_ALLOWED;
        }
        printf("Enter parameter name: ");
        scanf("%s", input_name);
        r = FERS_GetParam(daq->get_handles()[brd], input_name, input_val);
        if (r != 0) {
            FERS_GetLastError(description);
            printf("ERROR: %s\n", description);
            return FERSLIB_ERR_GENERIC;
        }
        printf("\n%s = %s\n", input_name, input_val);
        return 1;
    }
    if (c == 'h' && tcfg->acq_status == sabatdaq::ACQSTATUS::READY) {
        // histograms
        printf("\n\nSelect histo binning\n");
        printf("0 = 256\n");
        printf("1 = 512\n");
        printf("2 = 1024\n");
        printf("3 = 2048\n");
        printf("4 = 4096\n");
        printf("5 = 8192\n");
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
        printf("\n\nSelect plot\n");
        printf("0 = Spect Low Gain\n");
        printf("1 = Spect High Gain\n");
        c = f_getch() - '0';
        if ((c >= 0) && (c < 2)) {
            stats_plot = c;
        }
        return 1;
    }
    if (c == 'b') {
        int new_brd;
        printf("Current Active Board = %d\n", stats_brd);
        printf("New Active Board = ");
        scanf("%d", &new_brd);

        if ((new_brd >= 0) && (new_brd < tcfg->num_brd)) {
            stats_brd = new_brd;
            printf("Active Board = %d\n", stats_brd);
        }
        return 1;
    }
    if (c == 'c') {
        int new_ch;
        char chs[10];
        printf("Current Active Channel = %d\n", stats_ch);
        printf("New Active Channel ");
        scanf("%s", chs);
        if (isdigit(chs[0])) {
            sscanf(chs, "%d", &new_ch);
        }
        if ((new_ch >= 0) && (new_ch < FERSLIB_MAX_NCH_5202)) {
            stats_ch = new_ch;
        }
        return 1;
    }
    if (c == 'm') {
        printf("\n\nSelect statistics monitor\n");
        printf("0 = ChTrg Rate\n");
        printf("1 = ChTrg Cnt\n");
        printf("2 = PHA Rate\n");
        printf("3 = PHA Cnt\n");
        printf("4 = Hit Rate\n");
        printf("5 = Hit Cnt\n");
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
        printf("\n");
        printf("[q] Quit\n");
        printf("[s] Start Acquisition\n");
        printf("[S] Stop Acquisition\n");
        printf("[C] Configure FERS (not available when board is in RUN)\n");
        printf("[l] Load configuration from file (not available when board is in " "RUN)\n");
        printf("[w] Set Param (not available when board is in RUN)\n");
        printf("[r] Get Param (not available when board is in RUN)\n");
        printf("[b] Change board\n");
        printf("[c] Change channel\n");
        printf("[m] Set StatsMonitor Type\n");
        printf("[h] Set Histo binning (not available when board is in RUN)\n");
        printf("[p] Set Plot Mode\n");
        return 1;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    FILE *fcfg, *plotter;
    sabatdaq::DAQ_t this_cfg;
    sabatdaq::sabat_daq daq;

    FERS_BoardInfo_t BoardInfo[FERSLIB_MAX_NBRD];
    char cfg_file[500];  // =
                         // "C:\\Users\\dninci\\source\\repos\\FERSlibDemo\\x64\\Debug\\FERSlib_Config.txt";
                         // //DEFAULT_CONFIG_FILENAME;
    int ret = 0;
    float FiberDelayAdjust[FERSLIB_MAX_NCNC][FERSLIB_MAX_NTDL][FERSLIB_MAX_NNODES] = {0};
    int cnc = 0;
    int offline_conn = 0;
    int UsingCnc = 0;
    int MajorFWrev = 255;
    int rdymsg = 1;
    int AcqMode = 0;
    memset(&this_cfg, 0, sizeof(sabatdaq::DAQ_t));

    int tmp_brd = 0, tmp_ch = 0;

    int i = 0, clrscr = 0, dtq, ch, b;  // jobrun = 0,
    int nb = 0;
    double tstamp_us, curr_tstamp_us = 0;
    uint64_t kb_time = 0, curr_time = 0, print_time = 0;  // , wave_time;
    float rtime;
    void* Event;

    printf("**************************************************************\n");
    printf("FERSlib Demo v%s\n", DEMO_RELEASE_NUM);
    printf("FERSlib v%s\n", FERS_GetLibReleaseNum());
    printf("\nWith this demo, you can perform the following:\n");
    printf(" - Open and configure FERS module\n");
    printf(" - Read an event\n");
    printf(" - Plot and collect statistics for Spectroscopy and Timing Common " "Start mode(Timing mode not yet implemented)");
    printf("\nFor more specific usage, refer to the Janus software version " "related to the FERS module you use\n");
    printf("**************************************************************\n");

    // printf("\n\n%s\n%d\n", argv[0], argc);

    if (argc == 1) {  // No options provided
        fcfg = fopen(DEFAULT_CONFIG_FILENAME, "r");
        if (fcfg == NULL) {
            printf(
                "No config file provided and no default one %s found. Exiting " "(ret=%d)\n",
                DEFAULT_CONFIG_FILENAME,
                FERSLIB_ERR_INVALID_PATH);
            print_menu();
            return FERSLIB_ERR_INVALID_PATH;
        }
        strcpy(cfg_file, DEFAULT_CONFIG_FILENAME);
    } else if (argc == 2) {
        if (strcmp(argv[1], "-h") == 0 or strcmp(argv[1], "--help") == 0) {
            print_menu();
            return 0;
        }
        fcfg = fopen(argv[1], "r");
        if (fcfg == NULL) {
            printf("No valid config file name %s provided. Exiting (ret=%d)\n", argv[1], FERSLIB_ERR_INVALID_PATH);
            return FERSLIB_ERR_INVALID_PATH;
        }
        strcpy(cfg_file, argv[1]);
    } else {
        return FERSLIB_ERR_GENERIC;
    }

    // OPEN CFG FILE
    this_cfg.num_brd = get_brd_path_from_file(fcfg, &this_cfg);
    fseek(fcfg, 0, SEEK_SET);
    get_delayfiber_from_file(fcfg, &this_cfg);
    fclose(fcfg);
    // OPEN BOARDS

    for (int b = 0; b < this_cfg.num_brd; b++) {
        char *cc, cpath[100];
        if (((cc = strstr(this_cfg.brd_path[b], "tdl")) != NULL)) {  // TDlink used => Open connection to concentrator (this is not
                                                                     // mandatory, it is done for reading information about the
                                                                     // concentrator)
            UsingCnc = 1;
            FERS_Get_CncPath(this_cfg.brd_path[b], cpath);
            if (!FERS_IsOpen(cpath)) {
                FERS_CncInfo_t CncInfo;
                printf("\n--------------- Concentrator %2d ----------------\n", cnc);
                printf("Opening connection to %s\n", cpath);
                ret = FERS_OpenDevice(cpath, &cnc_handle[cnc]);
                if (ret == 0) {
                    printf("Connected to Concentrator %s\n", cpath);
                } else {
                    printf("Can't open concentrator at %s\n", cpath);
                    return FERSLIB_ERR_GENERIC;
                }
                if (!FERS_TDLchainsInitialized(cnc_handle[cnc])) {
                    printf("Initializing TDL chains. This will take a few " "seconds...\n");
                    ret = FERS_InitTDLchains(cnc_handle[cnc], this_cfg.FiberDelayAdjust[cnc]);
                    if (ret != 0) {
                        printf("Failure in TDL chain init\n");
                        return FERSLIB_ERR_TDL_ERROR;
                    }
                }
                ret |= FERS_ReadConcentratorInfo(cnc_handle[cnc], &CncInfo);
                if (ret == 0) {
                    printf("FPGA FW revision = %s\n", CncInfo.FPGA_FWrev);
                    printf("SW revision = %s\n", CncInfo.SW_rev);
                    printf("PID = %d\n", CncInfo.pid);
                    if (CncInfo.ChainInfo[0].BoardCount == 0) {  // Rising error if no board is connected to link 0
                        printf("No board connected to link 0\n");
                        return FERSLIB_ERR_TDL_ERROR;
                    }
                    for (int i = 0; i < 8; i++) {
                        if (CncInfo.ChainInfo[i].BoardCount > 0) {
                            printf("Found %d board(s) connected to TDlink n. %d\n", CncInfo.ChainInfo[i].BoardCount, i);
                        }
                    }
                } else {
                    printf("Can't read concentrator info\n");
                    return FERSLIB_ERR_GENERIC;
                }
                cnc++;
            }
        }
        if ((this_cfg.num_brd > 1) || (cnc > 0)) {
            printf("\n------------------ Board %2d --------------------\n", b);
        }
        printf("Opening connection to %s\n", this_cfg.brd_path[b]);
        char BoardPath[500];
        strcpy(BoardPath, this_cfg.brd_path[b]);
        //! [LastErrorOpen]
        //! [Connect]
        ret = FERS_OpenDevice(BoardPath, &daq.get_handles()[b]);
        //! [Connect]
        if (ret != 0) {
            char emsg[1024];
            FERS_GetLastError(emsg);
            printf("Can't open board: %s (ret=%d)\n", emsg, ret);
        }
        //! [LastErrorOpen]
        else
        {
            printf("Connected to %s\n", this_cfg.brd_path[b]);
        }
        // PRINT BOARD INFO
        ret = FERS_GetBoardInfo(daq.get_handles()[b], &BoardInfo[b]);
        if (ret == 0) {
            char fver[100];
            if (BoardInfo[b].FPGA_FWrev == 0) {
                sprintf(fver, "BootLoader");
            } else {
                sprintf(fver,
                        "%d.%d (Build = %04X)",
                        (BoardInfo[b].FPGA_FWrev >> 8) & 0xFF,
                        BoardInfo[b].FPGA_FWrev & 0xFF,
                        (BoardInfo[b].FPGA_FWrev >> 16) & 0xFFFF);
            }
            MajorFWrev = min((int)(BoardInfo[b].FPGA_FWrev >> 8) & 0xFF, MajorFWrev);
            printf("FPGA FW revision = %s\n", fver);
            if (strstr(this_cfg.brd_path[b], "tdl") == NULL) {
                printf("uC FW revision = %08X\n", BoardInfo[b].uC_FWrev);
            }
            printf("PID = %d\n", BoardInfo[b].pid);
        } else {
            printf("Can't read board info\n");
            return FERSLIB_ERR_GENERIC;
        }
    }

LoadConfigFERS:
    //! [ParseFile]
    ret = FERS_LoadConfigFile(cfg_file);
    //! [ParseFile]
    if (ret != 0) {
        printf("Cannot load FERS configuration from file %s\n", cfg_file);
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
        printf("Can't open gnuplot at %s\n", GNUPLOTEXE);
    }
    fprintf(plotter, "set grid\n");
    fprintf(plotter, "set mouse\n");

// CONFIGURE FROM FILE
ConfigFERS:
    printf("Configuring boards ...\n");
    for (int b = 0; b < this_cfg.num_brd; ++b) {
        //! [Configure]
        ret = FERS_configure(daq.get_handles()[0], CFG_HARD);
        //! [Configure]
        if (ret != 0) {
            printf("Board %d failed!!!\n", b);
        } else {
            printf("Board %d configured\n", b);
        }
        char fdname[512];
        FERS_DumpBoardRegister(daq.get_handles()[0]);
        FERS_DumpCfgSaved(daq.get_handles()[0]);
    }
    FERS_SetEnergyBitsRange(0);

    daq.ResetStatistics();

    this_cfg.acq_status = sabatdaq::ACQSTATUS::READY;
    curr_time = sabatdaq::get_time();
    print_time = curr_time;  // force 1st print with no delay
    kb_time = curr_time;
    stats_mon = SMON_PHA_RATE;
    stats_plot = PLOT_E_SPEC_LG;
    stats_brd = 0;
    stats_ch = 0;

    while (!this_cfg.Quit) {
        curr_time = sabatdaq::get_time();
        daq.get_stats().current_time = curr_time;
        nb = 0;

        if ((curr_time - kb_time) > 200) {
            int code = CheckRunTimeCmd(&daq, &this_cfg);
            if (code > 0) {  // Cmd executed
                if (this_cfg.Quit) {
                    goto QuitProgram;
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
                printf("Press [s] to start run, [q] to quit, [SPACE] to enter the " "menu\n\n");
                rdymsg = 0;
            }
            // continue;
        }

        if (this_cfg.acq_status == sabatdaq::ACQSTATUS::RUNNING) {
            //! [GetEvent]
            ret = FERS_GetEvent(daq.get_handles(), &b, &dtq, &tstamp_us, &Event, &nb);
            //! [GetEvent]
            if (nb > 0) {
                curr_tstamp_us = tstamp_us;
            }
            if (ret == 0) {
                printf("ERROR: Readout failure (ret = %d).\nStopping Data " "Acquisition ...\n", ret);
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
                    // printf(" Trigger ID: %" PRIu64 "  LG[0]: %" PRIu16 "
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
                    for (i = 0; i < FERSLIB_MAX_NCH_5202; i++) {
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
                    if ((daq.get_stats().current_trgid[b] > 0) && (daq.get_stats().current_trgid[b] > (daq.get_stats().previous_trgid[b] + 1))) {
                        daq.get_stats().LostTrg[b].cnt += ((uint32_t)daq.get_stats().current_trgid[b] - (uint32_t)daq.get_stats().previous_trgid[b] - 1);
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

            char ss[FERSLIB_MAX_NCH_5202][10], totror[20], ror[20],
                trr[20];  // torr[20],
            rtime = (float)(curr_time - daq.get_stats().start_time) / 1000;
            if (this_cfg.acq_status == sabatdaq::ACQSTATUS::RUNNING) {
                daq.UpdateStatistics(0);
                double totrate = 0;
                for (i = 0; i < this_cfg.num_brd; ++i) {
                    totrate += daq.get_stats().ByteCnt[i].rate;
                    double2str(totrate, 1, totror);
                }

                printf("Elapsed Time: %10.2f s\n", rtime);

                double2str(daq.get_stats().ByteCnt[0].rate, 1, ror);
                double2str(daq.get_stats().GlobalTrgCnt[0].rate, 1, trr);

                // Select Monitor Type
                for (i = 0; i < FERSLIB_MAX_NCH_5202; ++i) {
                    if (stats_mon == SMON_CHTRG_RATE) {
                        double2str(daq.get_stats().ChTrgCnt[0][i].rate, 0, ss[i]);
                    }
                    if (stats_mon == SMON_PHA_RATE) {
                        double2str(daq.get_stats().PHACnt[0][i].rate, 0, ss[i]);
                    }
                    if (stats_mon == SMON_HIT_RATE) {
                        double2str(daq.get_stats().ReadHitCnt[0][i].rate, 0,
                                   ss[i]);  // 5203 Only
                    }

                    if (stats_mon == SMON_CHTRG_CNT) {
                        double2str((double)daq.get_stats().ChTrgCnt[0][i].cnt, 0, ss[i]);
                    }
                    if (stats_mon == SMON_PHA_CNT) {
                        double2str((double)daq.get_stats().PHACnt[0][i].cnt, 0, ss[i]);
                    }
                    if (stats_mon == SMON_HIT_CNT) {
                        double2str((double)daq.get_stats().ReadHitCnt[0][i].cnt, 0,
                                   ss[i]);  // 5203 Only
                    }
                }

                // Print Global statistics
                if (this_cfg.num_brd > 1) {
                    printf("Board n. %d (press [b] to change active board)\n", 0);
                }
                printf("Time Stamp:   %10.3lf s                \n", daq.get_stats().current_tstamp_us[0] / 1e6);
                printf("Trigger-ID:   %10" PRIu64 "            \n", daq.get_stats().current_trgid[0]);
                printf("Trg Rate:        %scps                 \n", trr);
                printf("Trg Reject:   %10.2lf %%               \n", daq.get_stats().LostTrgPerc[0]);
                printf("Tot Lost Trg: %10" PRIu64 "            \n", daq.get_stats().LostTrg[0].cnt);
                if (this_cfg.num_brd > 1) {
                    printf("Readout Rate:    %sB/s (Tot: %sB/s)             \n", ror, totror);
                } else {
                    printf("Readout Rate:    %sB/s                   \n", ror);
                }
                printf("Temp (degC):     FPGA=%4.1f              \n", daq.get_board_temp(0, sabatdaq::TEMP::FPGA));
                printf("\n");
                // Print channels statistics
                for (i = 0; i < FERSLIB_MAX_NCH_5202; i++) {
                    printf("%02d: %s     ", i, ss[i]);
                    if ((i & 0x3) == 0x3) {
                        printf("\n");
                    }
                }
                printf("\n");
            }

            ////////////////////////////////////
            // Plot - only for PHA LG/HG 5202 //
            ////////////////////////////////////
            if (this_cfg.acq_status == sabatdaq::ACQSTATUS::RUNNING || stats_brd != tmp_brd || stats_ch != tmp_ch) {
                tmp_brd = stats_brd;
                tmp_ch = stats_ch;
                char ptitle[50];
                FILE* hist_file;
                hist_file = fopen("hist.txt", "w");
                for (int j = 0; j < this_cfg.EHistoNbin; ++j) {
                    if (stats_plot == PLOT_E_SPEC_LG) {
                        fprintf(hist_file, "%f\t%d\n", float(j), daq.get_stats().H1_PHA_LG[stats_brd][stats_ch].H_data[j]);
                        sprintf(ptitle, "Spect LG");
                    } else if (stats_plot == PLOT_E_SPEC_HG) {
                        fprintf(hist_file, "%f\t%d\n", float(j), daq.get_stats().H1_PHA_HG[stats_brd][stats_ch].H_data[j]);
                        sprintf(ptitle, "Spect HG");
                    }
                }
                fclose(hist_file);
                if (plotter) {
                    fprintf(plotter, "set xlabel 'Channels'\n");
                    fprintf(plotter, "set ylabel 'Counts'\n");
                    fprintf(plotter, "set title '%s'\n", ptitle);
                    fprintf(plotter, "plot 'hist.txt' w boxes fs solid 0.7 title " "'Brd:%d Ch:%d'\n", stats_brd, stats_ch);
                    fflush(plotter);
                }
            }
            print_time = curr_time;
        }
    }

QuitProgram:
    daq.StopRun(&this_cfg);
    for (int b = 0; b < this_cfg.num_brd; ++b) {
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
        if (cnc_handle[b] < 0) {
            break;
        }
        FERS_CloseDevice(daq.get_cnc_handles()[0]);
    }
    pclose(plotter);
    printf("Quitting ...\n");

    return 0;
}
