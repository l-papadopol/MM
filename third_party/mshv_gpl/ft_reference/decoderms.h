#pragma once
#include <QObject>
#include <functional>
#include <QStringList>
#include <QElapsedTimer>
#include <cmath>
#include "decoderpom.h"
#include "../port/HvGenFt8/gen_ft8.h"
#include "../port/HvGenFt4/gen_ft4.h"
#include "modems/weak_signal/WeakSignalCodecLock.h"
#include <mutex>
#include <memory>
#define ALL_MSG_SNR 200
#define MAXDEC 200
#define MAXDUPMSGTHR 240
namespace MshvReference {
class ReferenceCodec8 {
 std::unique_ptr<::GenFt8> p;
 public:
 ReferenceCodec8(bool fl) {std::lock_guard<WeakSignalCodecLock::Mutex> l(WeakSignalCodecLock::mutex());p.reset(new ::GenFt8(fl));}
 void make_c77_i4tone(bool *a,int *b) {std::lock_guard<WeakSignalCodecLock::Mutex> l(WeakSignalCodecLock::mutex());return p->make_c77_i4tone(a,b);}
 void make_c77_i4tone_codeword(bool *a,int *b,bool*c) {std::lock_guard<WeakSignalCodecLock::Mutex> l(WeakSignalCodecLock::mutex());return p->make_c77_i4tone_codeword(a,b,c);}
 void pack77_make_c77_i4tone(QString a,int*b) {std::lock_guard<WeakSignalCodecLock::Mutex> l(WeakSignalCodecLock::mutex());return p->pack77_make_c77_i4tone(a,b);}
 void save_hash_call_from_dec(QString a,int b,int c,int d) {std::lock_guard<WeakSignalCodecLock::Mutex> l(WeakSignalCodecLock::mutex());return p->save_hash_call_from_dec(a,b,c,d);}
 void save_hash_call_my_his_r1_r2(QString a,int b) {std::lock_guard<WeakSignalCodecLock::Mutex> l(WeakSignalCodecLock::mutex());return p->save_hash_call_my_his_r1_r2(a,b);}
 QString unpack77(bool *a,bool &b) {std::lock_guard<WeakSignalCodecLock::Mutex> l(WeakSignalCodecLock::mutex());return p->unpack77(a,b);}
 void pack77(QString a,int &b,int c,bool*d) {std::lock_guard<WeakSignalCodecLock::Mutex> l(WeakSignalCodecLock::mutex());return p->pack77(a,b,c,d);}
 void split77(QString &a,int &b,QString*c) {std::lock_guard<WeakSignalCodecLock::Mutex> l(WeakSignalCodecLock::mutex());return p->split77(a,b,c);}
};
static inline double cabs(double complex x){return std::hypot(creal(x),cimag(x));}
#define numdeccq 40
#define numdecmyc 25
#define numcqsig 20
#define nummycsig 5
#define MAXINC_PerDec 20
#define ncalldt 150
#define MAXStatOEt 130
typedef struct
{
    double freq{};
    double dt{};
    bool lstate{};
    QString msg;
}
xcopy_struct;
class DecoderFt8
{
public:
    std::function<void(const QStringList &)> onMessage;
public:
    explicit DecoderFt8(int id);
    ~DecoderFt8();
    void SetStMultiAnswerMod(bool f);
    void SetStWords(QString,QString,int,int,QString);
    void SetStHisCallGrid(QString,QString,QString);
    void SetStDecode(QString time,int mousebutton,bool);
    void SetStDecoderDeep(int d);
    void SetStApDecode(bool f);
    void SetBandChangedStaticAll(uint8_t);
    void SetStQSOProgress(int,int);
    void SetStTxFreq(double f);
    void Decode3intFt(bool);//2.39 remm
    void SetNewP(bool);
    void SetEndPStaticAll();//2.76.5
    void SetModeChangedStaticAll();//2.76.5
    //void SetResetPrevT(QString ptime);
    void ft8_decode(double *dd,int c_dd,double f0a,double f0b,double fqso,bool &f,int id3dec,double,double);
    int ft8_even_odd(QString);//2.76sf
    void SetVarDecodeFtPar(bool,int,int);
    void SetFreqGlobal(QString);//2.76.5

public:
    void EmitDecodetTextFt(QStringList row) { if (onMessage) onMessage(row); }
    void EmitBackColor() {}

private:
double s_napwid8 = 50.0;
int s_nft8sens = 2;
int s_nft8cycles = 2;
bool s_3intFt8_d_ = true;
uint8_t s_band8 = 0;
int s_decoder_deep8 = 1;
 struct incall_struct {double xdt=0;QString msg;};
incall_struct incall[35+10]{};
int s_nmsg=0;
xcopy_struct evencopy[150+10]{};
xcopy_struct oddcopy[150+10]{};
xcopy_struct even[150+10]{};
xcopy_struct odd[150+10]{};
double avexdt=0;
int c_xdtt=0;
double xdtt[150+80]{};
bool once_static_init = true;
int nintcount = 4;
bool f_statis_all_new_p = true;
    QString s_MyBaseCall8 = "NA";
    int s_id_cont_ft8_28 = 0;
    int s_ty_cont_ft8_28 = 0;
    QString s_cont_ft8_cq = "CQ";
    QString s_MyCall8 = "NA";
    bool s_lmycallstd = false;
    bool s_lhiscallstd = false;
    QString s_HisBaseCall8 = "NA";
    QString s_HisCall8 = "NA";
    QString s_HisGrid8 = "NA";
    QString s_time8 = "000000";
    int s_mousebutton8 = 0;
    bool s_fopen8 = false;
    bool s_lapon8 = false;
    int s_nQSOProgress8 = 0;
    int s_nlasttx = 0;
    double s_nftx8 = 1200.0;
    bool f_multi_answer_mod8 = false;

    int decid{};
    F2a f2a;
    PomAll pomAll;
    PomFt pomFt;
    std::unique_ptr<ReferenceCodec8> TGenFt8;
    double DEC_SAMPLE_RATE{};
    double twopi{};
    double pi{};
    uint8_t band8{};

    bool f_new_p{};
    //QString s_time8_prev;
    int s_ndecodes{};
    QString allmessages[ALL_MSG_SNR+50];
    double xdt_per_dec[ALL_MSG_SNR+20]{};
    double f1_save[ALL_MSG_SNR+50]{};
    double xdt_save[ALL_MSG_SNR+50]{};
    int itone_save[ALL_MSG_SNR+50][100]{};
    bool lsubtracted[ALL_MSG_SNR+50]{};
    int s_cou_dd1{};
    double dd1[182600]{};

    bool first_sync8d{};
    double complex csync_ft8_2[7][32]{};
    void sync8d(double complex *cd0,int i0,double complex *ctwk,int itwk,double &sync);

    bool first_ft8_downsample{};
    double taper_ft8_ds[120]{};
    double complex cx_ft8[192100]{}; //2.09 ->error //96000+100   //(0:NFFT1/2)  NFFT1=192000 // 96000
    void ft8_downsample(double *dd0,bool &newdat,double f1,double complex *cd0);

    double pulse_ft8_rx[5770]{}; //    !1920*3=5760
    double complex ctab8_[65536+10]{};
    void gen_ft8cwaveRx(int *i4tone,double f_tx,double complex *cwave);

    //bool first_subsft8;
    double complex cw_subsft8[190400]{}; //180000+300  15*12000
    double endcorrectionft8[2200]{}; //(NFILT/2+1)  NFILT=4000; 4000/2+1=2001
    double BestIdtft8(double *dd,double f0,double dt,double idt,double complex *cref,
                      double complex *cfilt,double complex *cw_subs,double *endcorr,
                      double *xdd,double complex *cx);
    void subtractft8(double *dd,int *itone,double f0,double dt,bool refdt);

    bool first_ft8b_2{};
    int mcq_ft8_2[29]{};
    /*int mcqfd_ft8_2[29];
    int mcqru_ft8_2[29]{};
    int mcqtest_ft8_2[29]{};
    int mcqww_ft8_2[29]{};
    int mcqbu_ft8_2[29]{};
    int mcqft_ft8_2[29];*/
    int mrrr_ft8_2[19]{};
    int m73_ft8_2[19]{};
    int mrr73_ft8_2[19]{};
    int cont_id0_ft8_2{};
    bool one_ft8_2[9][512]{}; //(0:511,0:8);
    QString hiscall12_0_ft8_2;
    bool ft8_downs_sync_bmet(double *,bool ap7,bool &,double &,double &,int &,int &,double s8_[79][8],
                             double *,double *,double *,double *,double *,int,int);
    void first_ft8b_and_ft8var(QString,int);
    void ft8b(double *dd,bool &newdat,int nQSOProgress,double nfqso,double nftx,int ndepth,bool n4pas3int,bool lapon,
              double napwid,bool lsubtract,bool nagain,int cid,int cty,int,int &iaptype,double &f1,double &xdt,
              double xbase,int *apsym,int &nharderrors,double &dmin,int &nbadcrc,QString &message,
              double &xsnr,QString hiscall12,int *it,int &,int &);

    void baseline(double *s,int nfa,int nfb,double *sbase);
    bool first_ft8sbl{};
    double window_ft8sbl[3890]{}; //NFFT1=2*NSPS NSPS=1920 1920*2=3840
    void get_spectrum_baseline(double *dd,int nfa,int nfb,double *sbase);
    void sync8(double *dd,double nfa,double nfb,double syncmin,double nfqso,double s_[402][1970],double candidate[2][620],int &ncand,double *sbase);
    void ft8apset(QString mycall12,QString hiscall12,int *apsym2);//int &iaptype ,QString hisgrid6,bool bcontest,QString mygrid6,

    void PrintMsg(QString,int,double,double,QString,int,float,float,bool &,bool,bool,bool);
    QString nutc0;
    int c_zerop{};
    QString msg0[2][2][MAXDEC+50];
    double dt0[2][2][MAXDEC+50]{};
    double f0[2][2][MAXDEC+50]{};
    int ndec[2][2]{};
    int jseq{};
    void ft8_a7_save(QString,double,double,QString);
    void ft8_a7d(double *dd0,bool &newdat,QString call_1,QString call_2,QString grid4,
                 double &xdt,double &f1,double xbase,int &nharderrors,double &,QString &msg37,double &xsnr);
    double pulse_ft8_rxAp8[130]{}; //    32*3=96
    void gen_ft8cwaveRxAp8(int *i4tone,double f_tx,double complex *cwave);
    bool s_ltry_a8{};
    void ft8_a8d(double *dd,QString mycall,QString dxcall,QString dxgrid,double f1a,double &xdt,
                 double &fbest,double &xsnr,double &plog,QString &msgbest);
    void TryAp8(double *,bool,int,int,double,bool &);
//////////  FT8DecVar //////////////////////////////////////////////////
    bool s_use_var_dec = false;
    //static int nintcount;
    typedef struct
    {
        double freq{};
        double xdt{};
    }
    tfxdec;
    tfxdec tmpcqdec[numdeccq+2];
    tfxdec tmpmyc[numdecmyc+2];

    typedef struct
    {
        double xdt{};
        bool lstate{};
        QString lastmsg;
    }
    lastrxmsg_;
    lastrxmsg_ lastrxmsg[1];
    typedef struct
    {
        double dt{};
        QString call2;
    }
    callsigndt_struct;
    callsigndt_struct calldteven[ncalldt+4];
    callsigndt_struct calldtodd[ncalldt+4];
    typedef struct
    {
        double freq{};
        double xdt{};
        double complex cs_[79][8]{}; //(0:7,79)
    }
    cq_struct;
    cq_struct evencq[numcqsig+2];
    cq_struct oddcq[numcqsig+2];
    cq_struct evenmyc[nummycsig+2];
    cq_struct oddmyc[nummycsig+2];
    cq_struct evenqso[1];
    cq_struct oddqso[1];
    cq_struct tmpcqsig[numcqsig+2];//! 20 sigs*/
    cq_struct tmpmycsig[nummycsig+2];//! 5 sigs
    cq_struct tmpqsosig[1];

    int npos{};
    bool lsubtracted_{};
    double freqsub[250]{};
    int nft8cycles{};
    int ncount{};
    //QString msgsrcvd[150];
    bool lrepliedother{};
    bool lft8sdec{};
    int ncqsignal{};
    int nmycsignal{};
    bool lqsomsgdcd{};
    QString lasthcall;
    QString msgroot;
    int nlasttx{};
    int mycalllen1{};
    void ft8var_init();

    double dd8m[190000]{};
    double dd8[190000]{};

    double windowx[220]{};
    double windowc1[80]{};
    double complex csync[7][32]{}; //csync(0:6,32)
    double complex csynccq[8][32]{}; //csynccq(0:7,32)
    double complex ctwkw[20][32]{}; //(11,32)
    double complex ctwkn[20][32]{}; //(11,32)
    double complex ctwk256[256]{};

    double complex csold_[79][8]{}; //(0:7,79)
    void cwfilter_init();

    bool lhound{};
    bool lskiptx1{};
    bool ltxing{};
    double complex csynce[19][32]{}; //csynce(0:18,32)
    int idtone56[56][58]{}; //(56,58),
    int itone56[56][79]{}; //(56,79)
    QString msg_[56];//(56)
    int idtonecqdxcns[58]{}; //(58)
    int idtonedxcns73[58]{}; //(58),
    int idtonefox73[58]{}; //(58),
    int idtonespec[58]{}; //(58)
    void tone8(bool,bool);

    bool first_apsetvar{};
    QString hiscallprev;
    QString mycallprev;
    bool lhoundprev{};
    int apsym_[58]{};
    int apcqsym_[77]{}; //(77);
    int apsymdxns1_[58]{};
    int apsymdxnsrrr_[77]{};
    int apsymdxns732_[77]{};
    int apsymdxnsr73_[77]{};
    int apsymdxns73_[77]{};
    int apsymdxnsrr73_[77]{};
    int apsymdxstd_[58]{};
    int apsymmyns1_[29]{};
    int apsymmyns2_[58]{};
    int apsymmynsrrr_[77]{};
    int apsymmyns73_[77]{};
    int apsymmynsrr73_[77]{};
    int apsymsp_[66]{};
    void MakeApFlip(int *,bool *,int);
    void ft8apsetvar(bool,bool);

    int idtonemyc[58]{}; //(58)
    void tone8myc();

    double complex cxx_ft8[193000]{};
    void ft8_downsamplevar(bool &,double,int,double complex *c0,double complex *c2,double complex *c3,bool,int);

    /*bool first_agccft8;
    bool lagccbail{};
    double w11[1024+128]{}; //NFFT
    void agccft8(double nfa,double nfb);*/

    void sync8var(double,double,double,double,double c[4][512],int&,int,int,int,bool,int,int,double,double);
    bool extract_calls(QString msgd,QString &c1,QString &c2,int &ispc1,int &ispc2,int &ispc3);

    double complex csyncsd[19][32]{}; //(0:18,32)
    double complex csyncsdcq[58][32]{}; //(0:57,32)
    int itone76[76][79]{}; //(76,79)
    int idtone76[76][58]{}; //(76,58)
    QString msgsd76[76];
    void tonesdvar(QString,bool);

    void twkfreq1var(double complex *ca,int npts,double fsample,double *a,double complex *cb);//,int cd_off
    void sync8dvar(double complex *cd0,int i0,double complex *ctwk,int itwk,double &sync,int ipass,
                   bool lastsync,int iqso,bool lcq,bool lcallsstd,bool lcqcand,int);

    void syncdist(double *,int *,int);
    void set_itonedem_s8(int *itonedem,bool *lmatched,double s8_1_[79][8]);
    void ft8sd1var(double s8_[79][8],int *itone,QString msgd,QString &msg37,bool &lft8sd,bool lcq);
    void ft8mf1var(double s8_[79][8],int *itone,QString msgd,QString &msg37,bool &lft8sd);
    int idtone25[25][58]{}; //(25,58)
    void ft8mfcqvar(double s8_[79][8],int *itone,QString msgd,QString &msg37,bool &lft8sd);
    void ft8svar(double s8_[79][8],double srr,int *itone,QString &msg37,bool &lft8s,int nft8rxfsens,bool stophint);
	
    //double napwid8;
    bool lapmyc{};
    //int nqsoend[3];
    void ft8sdvar(double s8_[79][8],double srr,int *itone,QString msgd,QString &msg37,bool &lft8sd,bool lcq);
    void subtractft8var(int *itone,double f0,double dt);
    
    uint8_t isFalseDecode(QString msg,int xsnr,double qual,int nharderror);   
    int nindex{};
    QString msgincall[MAXINC_PerDec+5];//(174)
    double xdtincall[MAXINC_PerDec+5]{}; //(174)
    void ft8bvar(bool &newdat1,int nQSOProgress,double nfqso,double nftx,bool lsubtract,
                 bool nagainfil,double &f1,double &xdt,int &nbadcrc,
                 QString &msg37,double &xsnr,bool stophint,bool &lFreeText,int ipass,bool lft8subpass,
                 bool &lspecial,bool lcqcand,int npass,bool lmycallstd,bool lhiscallstd,
                 bool f_od_ev,bool &lft8sd,int &i3,int nft8rxfsens,bool &lhashmsg,bool lqsothread,
                 bool lft8lowth,bool lhighsens,bool lnohiscall,bool lnomycall,bool lnohisgrid,
                 double &qual,int &iaptype2,int,double);
    void extract_callvar(QString,QString &);
    int nmsgloc{};
    xcopy_struct eventmp[MAXStatOEt+5];
    xcopy_struct oddtmp[MAXStatOEt+5];
    uint8_t band8var{};
    void ft8_SetStart_ev_od_var(bool,uint8_t);
    void ft8_Write_evt_odt_var(QString,int,bool,double,double,bool,bool,bool);
    void ft8_decodevar(double *,int,double,double,double,bool &,double,double,bool,bool,bool);

////////// end FT8DecVar //////////////////////////////////////////////////

};


class ReferenceCodec4 {
 std::unique_ptr<::GenFt4> p; std::unique_ptr<::GenFt8> parser;
 public:
 ReferenceCodec4(bool fl){std::lock_guard<WeakSignalCodecLock::Mutex> l(WeakSignalCodecLock::mutex());p.reset(new ::GenFt4(fl));parser.reset(new ::GenFt8(fl));}
 void split77(QString&a,int&b,QString*c){std::lock_guard<WeakSignalCodecLock::Mutex> l(WeakSignalCodecLock::mutex());parser->split77(a,b,c);}
 void make_c77_i4tone_codeword(bool *c77,int *tones,bool *cw){
  std::lock_guard<WeakSignalCodecLock::Mutex> l(WeakSignalCodecLock::mutex());p->make_c77_i4tone(c77,tones);
  const bool rvec[77]={0,1,0,0,1,0,1,0,0,1,0,1,1,1,1,0,1,0,0,0,1,0,0,1,1,0,1,1,0,1,0,0,1,0,1,1,0,0,0,0,1,0,0,0,1,0,1,0,0,1,1,1,1,0,0,1,0,1,0,1,0,1,0,1,1,0,1,1,1,1,1,0,0,0,1,0,1};
  bool scrambled[180]{};for(int i=0;i<77;++i)scrambled[i]=c77[i]^rvec[i];p->encode174_91(scrambled,cw);
 }
 QString unpack77(bool*a,bool&b){std::lock_guard<WeakSignalCodecLock::Mutex> l(WeakSignalCodecLock::mutex());return p->unpack77(a,b);}
 void pack77(QString a,int &b,int c,bool*d){std::lock_guard<WeakSignalCodecLock::Mutex> l(WeakSignalCodecLock::mutex());return p->pack77(a,b,c,d);}
 void make_c77_i4tone(bool*a,int*b){std::lock_guard<WeakSignalCodecLock::Mutex> l(WeakSignalCodecLock::mutex());return p->make_c77_i4tone(a,b);}
 void encode174_91(bool*a,bool*b){std::lock_guard<WeakSignalCodecLock::Mutex> l(WeakSignalCodecLock::mutex());return p->encode174_91(a,b);}
 void save_hash_call_mam(QStringList a){std::lock_guard<WeakSignalCodecLock::Mutex> l(WeakSignalCodecLock::mutex());return p->save_hash_call_mam(a);}
};
class DecoderFt4
{
public:
    std::function<void(const QStringList &)> onMessage;
public:
    explicit DecoderFt4(int id);
    ~DecoderFt4();
    void SetStTxFreq(double f);
    void SetStMultiAnswerMod(bool f);
    void SetStDecoderDeep(int d);
    void SetStApDecode(bool f);// only in mshv
    void SetBandChangedStaticAll(uint8_t);
    void SetStQSOProgress(int i);
    void SetStDecode(QString time,int mousebutton,bool);
    void SetStWords(QString,QString,int,int,QString);
    void SetStHisCall(QString);
    void SetMAMCalls(QStringList ls);
    //void SetNewP(bool);
    //void SetResetPrevT(QString ptime);
    void ft4_decode(double *dd,double f0a,double f0b,double,double,double fqso,bool &f);

public:
    void EmitDecodetTextFt(QStringList row) { if (onMessage) onMessage(row); }
    void EmitBackColor() {}

private:
double s_nftx4 = 1200.0;
bool f_multi_answer_mod4 = false;
bool s_lapon4 = false;// only in mshv
int s_nQSOProgress4 = 0;
QString s_time4 = "000000";
int s_mousebutton4 = 0;
bool s_fopen4 = false;
QString s_MyCall4 = "NOT__EXIST";
int s_id_cont_ft4_28 = 0;
int s_ty_cont_ft4_28 = 0;
QString s_cont_ft4_cq = "CQ";
QString s_HisCall4 = "NOCALL";

uint8_t s_band4 = 0;
int s_decoder_deep4 = 1;
    int decid{};
    F2a f2a;
    PomAll pomAll;
    PomFt pomFt;
    std::unique_ptr<ReferenceCodec4> TGenFt4;
    double DEC_SAMPLE_RATE{};
    double twopi{};
    double pi{};
    uint8_t band4{};

    //bool f_new_p;
    //void dshift1(double *a,int cou_a,int ish);//???

    bool first_ft4_ds{};
    double complex cx_ft4_ds[40000]{}; //(0:NMAX/2)=36288   31104                 [NMAX] (NMAX=21*3456)=72576
    double window_ft4_ds[4096]{}; //(0:NFFT2-1) (NFFT2=NMAX/18)=4032     (0:NFFT2-1) (NFFT2=NMAX/16)=3888
    void ft4_downsample(double *dd,bool newdata,double f0,double complex *c);

    //void nuttal_window(double *win,int n);
    void ft4_baseline(double *s,int nfa,int nfb,double *sbase);

    bool first_ft4detcad{};
    double window_ft4[2314]{}; //2304;//NFFT1=2048;
    void getcandidates4(double *dd,double fa,double fb,double,double,double syncmin,double nfqso,
                        int maxcand,double candidate[2][180],int &ncand,double*);

    bool first_ft4_sync4d{};
    double complex csynca_ft4_sync[70]{}; //(2*NSS) 2*32=64
    double complex csyncb_ft4_sync[70]{};
    double complex csyncc_ft4_sync[70]{};
    double complex csyncd_ft4_sync[70]{};
    void sync4d(double complex *cd0,int i0,double complex *ctwk,int itwk,double &sync);

    double pulse_ft4_rx[1748]{}; //576*3=1728    !512*3=1536
    void gen_ft4cwaveRx(int *i4tone,double f_tx,double complex *cwave);

    bool first_subsft4{};
    double complex cw_subsft4[72800]{}; //72576   =62208
    void subtractft4(double *dd,int *itone,double f0,double dt);

    bool first_ft4bm{};
    bool one_ft4_2[8][256]{}; //(0:255,0:7)
    void get_ft4_bitmetrics(double complex *cd,double bitmetrics_[5][220],bool &badsync,double s4_[120][4],int,int);//2*NN=206

    bool first_ft4d{};
    int mrrr_ft4[19]{};
    int m73_ft4[19]{};
    int mrr73_ft4[19]{};
    int mcq_ft4[29]{};
    /*int mcqru_ft4[29];
    int mcqfd_ft4[29]{};
    int mcqtest_ft4[29]{};
    int mcqww_ft4[29]{};
    int mcqbu_ft4[29]{};
    int mcqft_ft4[29];*/
    int cont_id0_ft4_2{};
    QString mycall0_ft4;
    QString hiscall0_ft4;
    double fac_ft4_sync{};
    double complex ctwk2_ft4_[41][70]{}; //ctwk2(2*NSS,-16:16) 2*32=64
    int apbits_ft4[174]{}; //174
    int apmy_ru_ft4[28]{};
    int aphis_fd_ft4[28]{};
    int ft4_even_odd(QString s);
    void sync_dt_df(double complex *cd2,int iseg,int &idfbest,int &ibest,double &smax,double &smax1);
    //int count_eq_bits(bool *a,int b_a,bool *b,int c);
    //int sync_qual(double *bitmetrics);
    double get_xibest(double complex *cd2,int ibest,int idfbest,double smax);
    //////////AP7//////////////////
    int jseq_a7{};
    QString nutc0;
    int c_zerop{};
	QString msg0[2][2][MAXDEC+50];
    double dt0[2][2][MAXDEC+50]{};
    double f0[2][2][MAXDEC+50]{};
    int ndec[2][2]{};
	void ft4_a7_save(QString,double,double,QString);
	void ft4_a7d(double *dd0,bool &newdat,QString call_1,QString call_2,QString grid4,
                 double &xdt,double &f1,double xbase,int &nharderrors,double &,QString &msg37,double &xsnr);
    //////////end AP7//////////////////
    void PrintMsg(QString,int nsnr,double xdt,double f1,QString message,int iaptype,float,bool &have_dec,bool &f_only_one_color,bool);

};

}
