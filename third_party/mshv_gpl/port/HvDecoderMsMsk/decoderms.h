/* Minimal MSK144/MSK40 DecoderMs adapter extracted from MSHV for MadModem.
 * Original algorithms: MSHV / WSJT-derived GPL code, see third_party/mshv_gpl.
 */
#ifndef MADMODEM_MSHV_MSK_DECODERMS_H
#define MADMODEM_MSHV_MSK_DECODERMS_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>
#include <cmath>

#include "../HvDecoderMs/mshv_complex_compat.h"

#include "../HvDecoderMs/decoderpom.h"
#include "../HvGenMsk/genmesage_msk.h"

#ifndef RECENT_CALLS_COU
#define RECENT_CALLS_COU 6
#endif
#ifndef HASH_CALLS_COUNT
#define HASH_CALLS_COUNT 36
#endif
#ifndef RECENT_SHMSGS_COU
#define RECENT_SHMSGS_COU 50
#endif
#ifndef MAX_RTD_DISP_DEC_COU
#define MAX_RTD_DISP_DEC_COU 140
#endif

class DecoderFt8 { public: void SetStHisCallGrid(QString,QString,QString) {} };
class DecoderFt4 { public: void SetStHisCall(QString) {} };
class DecoderQ65 { public: void SetStHisCallGrid(QString,QString) {} };

class DecoderMs : public QObject
{
    Q_OBJECT
public:
    explicit DecoderMs(QString appPath = QString());
    ~DecoderMs();

    void setMode(int mode) { s_mode = mode; }
    void SetDecoderDeep(int d) { s_decoder_deep = d; }
    void SetPerodTime(int p);
    void SetDfSdb(int sdb,int df) { G_MinSigdB = sdb; G_DfTolerance = df; }
    void SetShOpt(bool f);
    void SetSwlOpt(bool f);
    void SetMsk144RxEqual(int v);
    void SetCalsHash(QStringList l);
    void SetCalsHashFileOpen(QString);
    void ResetCalsHashFileOpen();
    void SetZap(bool f) { s_nzap = f; }
    void SetDecode(int *dat,int count,QString time,int t_istart,int mousebutton,bool f_rtd,bool end_rtd,bool fopen);
    void DecodeMsk144Buffer(double *dat, int count, const QString &time, int mouseButton = 0, bool msk144ms = false);

signals:
    void EmitDecodetText(QStringList,bool,bool);
    void EmitDecodetTextRxFreq(QStringList,bool,bool);
    void EmitBackColor(bool);
    void EmitDecLinesPosToDisplay(int count,double pos,double pos_ping,QString p_time);
    void EmitDecode(bool,int);
    void EmitTimeElapsed(float);

private:
    bool f_back_color = false;
    void SetBackColor();
    void SetNexColor(bool f);
    void EndRtdPeriod();
    QString RemBegEndWSpaces(QString);
    mshv_complex dot_product_dca_dca(mshv_complex *a,int b_a,mshv_complex *b,int b_b,int count);
    double dot_product_da_da(double *a, double *b, int size, int offset_b);
    mshv_complex dot_product_dca_sum_dca_dca(mshv_complex *a,int a_b,int b_b,mshv_complex *c,int c_count);
    void analytic(double*,int,int,int,double*,mshv_complex*);
    void tweak1(mshv_complex *,int,double,mshv_complex *);
    void indexx(int,double*,int*);
    void ssort(double*,double*,int,int);
    void zero_int_beg_end(int*,int,int);
    void move_da_to_da(double*,int,double*,int,int);
    void smooth(double*,int);
    int ping(double*,int,double,int,double,double[100][3]);

    PomAll pomAll;
    F2a f2a;
    GenMsk *TGenMsk = nullptr;
    DecoderFt8 *DecFt8_0 = nullptr;
    DecoderFt4 *DecFt4_0 = nullptr;
    DecoderQ65 *DecQ65 = nullptr;
    double DEC_SAMPLE_RATE = 12000.0;
    double twopi = 6.283185307179586476925286766559;
    double pi = 3.1415926535897932384626433832795;
    int s_mode = 0;
    int s_period_time = 15;
    int s_decoder_deep = 2;
    int G_MinSigdB = 0;
    int G_DfTolerance = 100;
    bool G_ShOpt = false;
    bool G_SwlOpt = false;
    bool s_nzap = false;
    bool s_fopen = true;
    bool s_f_rtd = false;
    bool s_end_rtd = false;
    QString s_time;
    int s_mousebutton = 0;
    QString s_MyCall;
    QString s_MyBaseCall;
    QString HisGridLoc;
    QString s_HisCall;
    QString s_HisBaseCall;
    QString s_R1HisCall;
    QString s_R2HisCall;

    // MSK144 state copied from MSHV DecoderMs
    bool first_dec_msk = true;
    char s_msk144_2s8[8] = {0};
    double pp_msk144[12] = {0};
    double rcw_msk144[12] = {0};
    mshv_complex cb_msk144[42] = {0};
    bool f_first_msk144 = true;
    double dt_msk144 = 0.0;
    double fs_msk144 = 12000.0;
    int ihlo_msk144 = 0, ihhi_msk144 = 0, illo_msk144 = 0, ilhi_msk144 = 0, i2000_msk144 = 0, i4000_msk144 = 0;
    int last_ntol_msk144 = -99999;
    double last_df_msk144 = -99999.0;
    bool s_f_rtd_dummy = false;
    double tsec0_rtd_msk = 0.0;
    bool first_rtd_msk = true;
    double pnoise_rtd_msk = 0.0;
    QString s_time_last;
    int s_nsnrlast = -999;
    QString s_msglast;
    int s_nsnrlastswl = -999;
    QString s_msglastswl;
    int navg_sq = 0;
    mshv_complex cross_avg_sq[864] = {0};
    double wt_avg_sq = 0.0;
    double tlast_sq = 0.0;
    QString trained_dxcall_sq;
    QString training_dxcall_sq;
    bool currently_training_sq = false;
    bool first_sq = true;
    double s_pcoeffs_msk144[3] = {0};
    bool s_trained_msk144 = false;
    QStringList s_list_rpt_msk;
    double prev_pt_msk = 0.0;
    double ping_width_msk = 0.0;
    double prev_ping_t_msk = 0.0;
    double last_rpt_snr_msk = 0.0;
    bool one_end_ping_msk = false;
    bool is_new_rpt_msk = false;
    bool ss_msk144ms = false;
    bool s_msk144rxequal_s = true;
    bool s_msk144rxequal_d = true;
    mshv_complex h_msk144_2[524500] = {0};
    mshv_complex s_corrs[524500] = {0};
    mshv_complex s_corrd[524500] = {0};
    int nfft0_msk144_2 = 0;
    double dpclast_msk144_2[3] = {0};
    int rtd_dupe_cou = 0;
    double rtd_dupe_pos = 0.0;

    typedef struct { int hash; QString calls; } call_;
    call_ hash_msk40_calls[HASH_CALLS_COUNT];
    QString recent_calls[RECENT_CALLS_COU];
    QString recent_shmsgs[RECENT_SHMSGS_COU];
    char s_msk40_2s8r[8] = {0};
    double pp_msk40[12] = {0};
    double rcw_msk40[12] = {0};
    mshv_complex cbr_msk40[42] = {0};
    bool f_first_msk40 = true;
    double dt_msk40 = 0.0;
    double fs_msk40 = 12000.0;
    int ihlo_msk40 = 0, ihhi_msk40 = 0, illo_msk40 = 0, ilhi_msk40 = 0, i2000_msk40 = 0, i4000_msk40 = 0;
    int last_ntol_msk40 = -99999;
    double last_df_msk40 = -99999.0;

    void first_msk144();
    void first_msk40();
    void dftool_msk144(int ntol, double nrxfreq,double fd);
    void dftool_msk40(int ntol, double nrxfreq,double df);
    void cshift2(mshv_complex *a,mshv_complex *b,int cou,int ish);
    void mplay_dca_dca_dca(mshv_complex *a,int a_beg,int a_end,mshv_complex *b,int b_beg,mshv_complex *mp,int mp_b,int ord);
    void mplay_dca_dca_da(mshv_complex *a,int a_beg,int a_end,mshv_complex *b,int b_beg,double *mp,int mp_b,int ord);
    void mplay_da_da_i(double *a,int b_a,int e_a,double *b,int b_b,int mp);
    void mplay_da_absdca_absdca(double *a,int a_c,mshv_complex *b,mshv_complex *mp);
    void sum_dca_dca_dca(mshv_complex *a,int a_cou,mshv_complex *b,mshv_complex *c);
    void copy_double_ar_ainc(double*a,int a_beg,int a_inc,double*b,int b_beg,int b_end);
    void copy_dca_or_sum_max3dca(mshv_complex *a,int a_cou, mshv_complex *b, int b_beg, mshv_complex *c=0, int c_beg=-1, mshv_complex *d=0, int d_beg=-1);
    double sum_da(double*a,int a_beg,int a_end);
    int sum_ia(int*a,int a_beg,int a_end);
    int maxloc_absdca_beg_to_end(mshv_complex*a,int a_beg,int a_end);
    double maxval_da_beg_to_end(double*a,int a_beg,int a_end);
    int maxloc_da_end_to_beg(double*a,int a_beg,int a_end);
    double minval_da_beg_to_end(double*a,int a_beg,int a_end);
    int minloc_da_beg_to_end(double*a,int a_beg,int a_end);
    QString extractmessage144(char *decoded,int &nhashflag,char &ident);
    void msk144decodeframe_p(mshv_complex *c,double *softbits,QString &msgreceived,int &nsuccess,char &ident,double phase0);
    void msk144decodeframe(mshv_complex *c,double *softbits,QString &msg, int &nsuccess,char &ident,bool f_phase);
    void msk144sync(mshv_complex *cdat,int nframes,int ntol,double delf,int *navmask,int npeaks,double fc,double &fest,int *npklocs,int &nsuccess,mshv_complex *c);
    void msk144spd(mshv_complex *cdat,int np,int &nsuccess,QString &msgreceived,double fc,double &fest,double &tdec,char &ident,int &navg,mshv_complex *ct,double *softbits);
    void msk144signalquality(mshv_complex *cframe,double snr,double freq,double t0,double *softbits,QString msg,QString dxcall,int &nbiterrors,double &eyeopening,bool &trained,double *pcoeffs,bool f_calc_pcoeffs);
    QString GetStandardRPT(double width, double peak);
    QString str_round_20ms(double v);
    double MskPingDuration(double *detmet_dur,int istp_real,int il,double level,int nstepsize,int nspm,double dt);
    void SetDecodetTextMsk2DL(QStringList);
    void detectmsk144(mshv_complex *cdat,int npts,double s_istart,int &nmessages);
    void opdetmsk144(mshv_complex *cdat,int npts,double s_istart,int &nmessages);
    void msk144_freq_search(mshv_complex *cdat,double fc,int if1,int if2,double delf,int nframes,int *navmask,double &xmax,double &bestf,mshv_complex *cs,double *xccs);
    bool any_not_and_save_in_a(double *a,double *b,int c);
    void analytic_msk144(double *d,int d_count_begin,int npts,int nfft,mshv_complex *c);
    void analytic_msk144_2_init_s_corrs_full();
    void analytic_msk144_2(double *d,int d_count_begin,int npts,int nfft,mshv_complex *c,double *dpc,bool bseq,bool bdeq);
    void msk_144_40_decode(double *dat,int npts_in,double s_istart,bool);
    void msk_144_40_rtd(double *d2,int n,double s_istart,bool);
    void print_rtd_decode_text(QString msg,QString &smsg,int in_snr,int &s_snr,double ts,double fest,double t0,int navg,int ncorrected,double eyeopening,char ident);
    void SetMyGridMsk144ContM(QString,bool);
    bool isValidCallsign(QString);

    bool check_hash_msk40(int hash_in,int rpt,QString &msg);
    bool check_hash_msk40_swl(int hash_in,int rpt,QString &msg);
    void hash_msk40_all_calls(int id, QString calls);
    void hash_msk40_swl();
    void update_recent_calls(QString call);
    bool update_recent_shmsgs(QString message);
    QString FindBaseFullCallRemAllSlash(QString str);
    void detectmsk40(mshv_complex *cdat,int npts,double s_istart);
    void msk40decodeframe_p(mshv_complex *c,double *softbits,double xsnr,QString &msgreceived,int &nsuccess,char &ident,double phase0);
    void msk40decodeframe(mshv_complex *ct,double *softbits,double xsnr,QString &msg, int &nsuccess,char &ident,bool f_phase);
    void msk40sync(mshv_complex *cdat,int nframes,int ntol,double delf,int *navmask,int npeaks,double fc,double &fest,int *npklocs,int &nsuccess,mshv_complex *c);
    void msk40spd(mshv_complex *cdat,int np,int &nsuccess,QString &msgreceived,double fc,double &fest,double &tdec,char &ident,int &navg,mshv_complex *ct,double *softbits);
    void msk40_freq_search(mshv_complex *cdat,double fc,int if1,int if2,double delf,int nframes,int *navmask,double &xmax,double &bestf,mshv_complex *cs,double *xccs);
};

#endif
