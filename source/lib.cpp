#include <cstring>
#include <print>

#include <sys/time.h>

#include "lib.hpp"

namespace sabatdaq
{

/* Get time in milliseconds from the computer internal clock */
auto get_time() -> long
{
    long time_ms;
#ifdef WIN32
    struct _timeb timebuffer;
    _ftime(&timebuffer);
    time_ms = (long)timebuffer.time * 1000 + (long)timebuffer.millitm;
#else
    struct timeval t1;
    struct timezone tz;
    gettimeofday(&t1, &tz);
    time_ms = (t1.tv_sec) * 1000 + t1.tv_usec / 1000;
#endif
    return time_ms;
}

auto get_brd_path_from_file(FILE* f_ini, DAQ_t* cfg) -> int
{
    char tstr[1000], parval[1000], str1[1000];
    int brd;  // target board defined as ParamName[b][ch]
    int num_brd = 0;
    // int brd2 = -1, ch2 = -1;  // target board/ch defined as Section ([BOARD
    // b] [CHANNEL ch])

    // read config file and assign parameters
    while (fgets(tstr, sizeof(tstr), f_ini)) {
        if (strstr(tstr, "Open") != NULL) {
            sscanf(tstr, "%s %s", str1, parval);
            char* str = strtok(str1, "[]");  // Param name with [brd][ch]
            char* token = strtok(NULL, "[]");
            if (token != NULL) {
                sscanf(token, "%d", &brd);
            }
            sprintf(cfg->brd_path[brd], "%s", parval);
            ++num_brd;
        }
    }
    return num_brd;
}

auto get_delayfiber_from_file(FILE* f_ini, DAQ_t* cfg) -> int
{
    int ret = -1;
    char tstr[1000], parval[1000], str1[1000];
    int cnc = -1, node = -1, brd = -1;  // target board defined as ParamName[b][ch]
    int num_brd = 0;
    // int brd2 = -1, ch2 = -1;  // target board/ch defined as Section ([BOARD
    // b] [CHANNEL ch])

    // read config file and assign parameters
    while (fgets(tstr, sizeof(tstr), f_ini)) {
        if (strstr(tstr, "FiberDelayAdjust") != NULL) {
            sscanf(tstr, "%s %s", str1, parval);
            char* str = strtok(str1, "[]");  // Param name with [brd][ch]
            char* token = strtok(NULL, "[]");
            if (token != NULL) {
                sscanf(token, "%d", &cnc);
                if ((token = strtok(NULL, "[]")) != NULL) {
                    sscanf(token, "%d", &node);
                    if ((token = strtok(NULL, "[]")) != NULL) {
                        sscanf(token, "%d", &brd);
                    }
                }
            }
            ret = 0;
            int cm, cM, nm, nM, bm, bM;
            if (brd == -1) {
                bm = 0;
                bM = FERSLIB_MAX_NBRD;
            } else {
                bm = brd;
                bM = brd + 1;
            }
            if (node == -1) {
                nm = 0;
                nM = FERSLIB_MAX_NTDL;
            } else {
                nm = node;
                nM = node + 1;
            }
            if (cnc == -1) {
                cm = 0;
                cM = FERSLIB_MAX_NNODES;
            } else {
                cm = cnc;
                cM = cnc + 1;
            }
            for (int b = bm; b < bM; ++b) {
                for (int n = nm; n < nM; ++n) {
                    for (int c = cm; c < cM; ++c) {
                        float val;
                        sscanf(parval, "%f", &val);
                        cfg->FiberDelayAdjust[c][n][b] = val;
                    }
                }
            }
        }
    }
    return ret;
}

auto AddCount(Histogram1D_t* Histo, int Bin) -> bool
{
    if (Bin < 0) {
        Histo->Unf_cnt++;
        return false;
    } else if (Bin >= (int)(Histo->Nbin - 1)) {
        Histo->Ovf_cnt++;
        return false;
    }
    Histo->H_data[Bin]++;
    Histo->H_cnt++;
    Histo->mean += (double)Bin;
    Histo->rms += (double)Bin * (double)Bin;
    return true;
}

sabat_daq::sabat_daq()
{
    handle.fill(-1);
    cnc_handle.fill(-1);
}

// Init Readout
auto sabat_daq::InitReadout(DAQ_t* tcfg) -> void
{
    // std::println("{:s}:{:d} NumBrd={}", __PRETTY_FUNCTION__, __LINE__, tcfg->num_brd);
    for (int b = 0; b < tcfg->num_brd; b++) {
        int a1;
        //! [InitReadout]
        FERS_InitReadout(handle[b], 0, &a1);
        //! [InitReadout]
        sEvt[b] = {};
        // memset(&sEvt[b], 0, sizeof(ServEvent_t)); FIXME
    }
}

// ******************************************************************************************
// Run Control functions
// ******************************************************************************************
// Start Run (starts acq in all boards)
auto sabat_daq::StartRun(DAQ_t* tcfg) -> int
{
    int ret = 0, b, tdl = 1;
    char parvalue[1024];
    char parname[1024] = "";
    strcpy(parname, "StartRunMode");
    if (tcfg->acq_status == ACQSTATUS::RUNNING) {
        return 0;
    }

    for (b = 0; b < tcfg->num_brd; ++b) {
        if (FERS_CONNECTIONTYPE(handle[b]) != FERS_CONNECTIONTYPE_TDL) {
            tdl = 0;
        }
    }

    // GetStartRun Mode
    // STARTRUN_ASYNC
    // STARTARUN_CHAIN_T0
    // STARTARUN_CHAIN_T1
    // STARTRUN_TDL : this mode can be used only for TDL connection

    //! [GetParam]
    ret = FERS_GetParam(handle[0], parname, parvalue);
    //! [GetParam]
    sscanf(parvalue, "%d", &b);
    if (!tdl && (b == STARTRUN_TDL)) {
        printf("WARNING: StartRunMode: can't start run in TDL mode; switching to " "Async mode\n");
        for (b = 0; b < tcfg->num_brd; ++b) {
            sprintf(parvalue, "%d", STARTRUN_ASYNC);
            //! [SetParam]
            FERS_SetParam(handle[b], parname, parvalue);
            //! [SetParam]
            FERS_configure(handle[b], CFG_SOFT);
            printf("Brd%d Start mode Async\n", b);
        }
    }

    ret = FERS_StartAcquisition(handle.data(), tcfg->num_brd, STARTRUN_ASYNC, 0);

    stats.start_time = get_time();
    stats.stop_time = 0;
    first_sEvt = 1;

    tcfg->acq_status = ACQSTATUS::RUNNING;
    return ret;
}

// Stop Run
auto sabat_daq::StopRun(DAQ_t* tcfg) -> int
{
    int ret = 0;

    ret = FERS_StopAcquisition(handle.data(), tcfg->num_brd, STARTRUN_ASYNC, 0);
    if (stats.stop_time == 0) {
        stats.stop_time = get_time();
    }

    if (tcfg->acq_status == ACQSTATUS::RUNNING) {
        // printf("Run #%d stopped. Elapsed Time = %.2f\n",
        // (float)(Stats.stop_time - Stats.start_time) / 1000);
        tcfg->acq_status = ACQSTATUS::READY;
    }
    return ret;
}

auto sabat_daq::CreateStatistics(int nb, int nch, int Nbin) -> void
{
    for (int brd = 0; brd < nb; brd++) {
        for (int ch = 0; ch < nch; ch++) {
            stats.H1_PHA_LG[brd][ch].H_data = (uint32_t*)malloc(Nbin * sizeof(uint32_t));
            memset(stats.H1_PHA_LG[brd][ch].H_data, 0, Nbin * sizeof(uint32_t));
            stats.H1_PHA_LG[brd][ch].Nbin = Nbin;
            stats.H1_PHA_LG[brd][ch].H_cnt = 0;
            stats.H1_PHA_LG[brd][ch].Ovf_cnt = 0;
            stats.H1_PHA_LG[brd][ch].Unf_cnt = 0;

            stats.H1_PHA_HG[brd][ch].H_data = (uint32_t*)malloc(Nbin * sizeof(uint32_t));
            memset(stats.H1_PHA_HG[brd][ch].H_data, 0, Nbin * sizeof(uint32_t));
            stats.H1_PHA_HG[brd][ch].Nbin = Nbin;
            stats.H1_PHA_HG[brd][ch].H_cnt = 0;
            stats.H1_PHA_HG[brd][ch].Ovf_cnt = 0;
            stats.H1_PHA_HG[brd][ch].Unf_cnt = 0;
        }
    }
}

auto sabat_daq::ResetStatistics() -> void
{
    stats.start_time = get_time();
    stats.current_time = stats.start_time;
    stats.previous_time = stats.start_time;
    for (int b = 0; b < FERSLIB_MAX_NBRD; b++) {
        stats.current_trgid[b] = 0;
        stats.previous_trgid[b] = 0;
        stats.current_tstamp_us[b] = 0;
        stats.previous_tstamp_us[b] = 0;
        stats.trgcnt_update_us[b] = 0;
        stats.previous_trgcnt_update_us[b] = 0;
        stats.LostTrgPerc[b] = 0;
        stats.BuildPerc[b] = 0;
        memset(&stats.LostTrg[b], 0, sizeof(Counter_t));
        memset(&stats.Q_OR_Cnt[b], 0, sizeof(Counter_t));
        memset(&stats.T_OR_Cnt[b], 0, sizeof(Counter_t));
        memset(&stats.GlobalTrgCnt[b], 0, sizeof(Counter_t));
        memset(&stats.ByteCnt[b], 0, sizeof(Counter_t));
        for (int ch = 0; ch < FERSLIB_MAX_NCH_5202; ch++) {
            memset(stats.H1_PHA_LG[b][ch].H_data, 0, stats.H1_PHA_LG[b][ch].Nbin * sizeof(uint32_t));
            stats.H1_PHA_LG[b][ch].H_cnt = 0;
            stats.H1_PHA_LG[b][ch].H_p_cnt = 0;
            stats.H1_PHA_LG[b][ch].Ovf_cnt = 0;
            stats.H1_PHA_LG[b][ch].Unf_cnt = 0;

            memset(stats.H1_PHA_HG[b][ch].H_data, 0, stats.H1_PHA_HG[b][ch].Nbin * sizeof(uint32_t));
            stats.H1_PHA_HG[b][ch].H_cnt = 0;
            stats.H1_PHA_HG[b][ch].H_p_cnt = 0;
            stats.H1_PHA_HG[b][ch].Ovf_cnt = 0;
            stats.H1_PHA_HG[b][ch].Unf_cnt = 0;
            HistoCreatedE[b][ch] = 1;
        }
    }
}

auto sabat_daq::UpdateStatistics(int RateMode) -> void
{
    double pc_elapstime =
        (RateMode == 1) ? 1e3 * (stats.current_time - stats.start_time) : 1e3 * (stats.current_time - stats.previous_time);  // us
    stats.previous_time = stats.current_time;

    for (int b = 0; b < FERSLIB_MAX_NBRD; b++) {
        double brd_elapstime =
            (RateMode == 1) ? stats.current_tstamp_us[b] : stats.current_tstamp_us[b] - stats.previous_tstamp_us[b];  // - Stats.start_time
        double elapstime = (brd_elapstime > 0) ? brd_elapstime : pc_elapstime;
        double trgcnt_elapstime = (RateMode == 1) ? stats.trgcnt_update_us[b] - stats.start_time * 1000
                                                  : stats.trgcnt_update_us[b] - stats.previous_trgcnt_update_us[b];  // - Stats.start_time
        stats.previous_tstamp_us[b] = stats.current_tstamp_us[b];
        stats.previous_trgcnt_update_us[b] = stats.trgcnt_update_us[b];
        for (int ch = 0; ch < FERSLIB_MAX_NCH_5202; ch++) {
            UpdateCntRate(&stats.ChTrgCnt[b][ch], trgcnt_elapstime, RateMode);
            UpdateCntRate(&stats.HitCnt[b][ch], elapstime, RateMode);
            UpdateCntRate(&stats.PHACnt[b][ch], elapstime, RateMode);
        }
        UpdateCntRate(&stats.GlobalTrgCnt[b], elapstime, RateMode);
        UpdateCntRate(&stats.ByteCnt[b], pc_elapstime, RateMode);
    }
}

auto sabat_daq::DestroyStatistics() -> void
{
    int b, ch;
    for (b = 0; b < FERSLIB_MAX_NBRD; b++) {
        for (ch = 0; ch < FERSLIB_MAX_NCH_5202; ch++) {
            if (HistoCreatedE[b][ch]) {
                DestroyHistogram1D(stats.H1_PHA_LG[b][ch]);
                DestroyHistogram1D(stats.H1_PHA_HG[b][ch]);
                HistoCreatedE[b][ch] = 0;
            }
        }
    }
}

auto sabat_daq::DestroyHistogram1D(Histogram1D_t Histo) -> void
{
    if (Histo.H_data != NULL) {
        free(Histo.H_data);  // DNIN error in memory access
    }
}

void sabat_daq::UpdateCntRate(Counter_t* Counter, double elapsed_time_us, int RateMode)
{
    if (elapsed_time_us <= 0) {
        // Counter->rate = 0;
        return;
    } else if (RateMode == 1) {
        Counter->rate = Counter->cnt / (elapsed_time_us * 1e-6);
    } else {
        Counter->rate = (Counter->cnt - Counter->pcnt) / (elapsed_time_us * 1e-6);
    }
    Counter->pcnt = Counter->cnt;
}

// Update service event info
auto sabat_daq::UpdateServiceInfo(int board) -> int
{
    int ret = 0, fail = 0;
    uint64_t now = sabatdaq::get_time();

    if (first_sEvt >= 1 && first_sEvt < 10) {  // Skip the check on the first event service missing
        ++first_sEvt;
        return 0;
    } else if (first_sEvt == 10) {
        first_sEvt = 0;
        return 0;
    }

    if (sEvt[board].update_time > (now - 2000)) {
        BrdTemp[board][TEMP::BOARD] = sEvt[board].tempBoard;
        BrdTemp[board][TEMP::FPGA] = sEvt[board].tempFPGA;
        StatusReg[board] = sEvt[board].Status;
    } else {
        ret |= FERS_Get_FPGA_Temp(handle[board], &BrdTemp[board][TEMP::FPGA]);
        ret |= FERS_Get_Board_Temp(handle[board], &BrdTemp[board][TEMP::BOARD]);
        ret |= FERS_ReadRegister(handle[board], a_acq_status, &StatusReg[board]);
    }
    if (ret < 0) {
        fail = 1;
    }
    return ret;
}

}  // namespace sabatdaq
