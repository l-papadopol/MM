#include "decoderpom.h"
#include "../../../dsp/FftwPlannerLock.h"
#include <map>
#include <tuple>
#include <vector>
#include <mutex>
#include <stdexcept>
#include <cstring>
namespace MshvReference {
namespace {auto &planner=MadModemDsp::fftwPlannerMutex;struct Plan {
 int n; int kind; fftw_plan plan=nullptr; double *real=nullptr; fftw_complex *data=nullptr;
 Plan(int count,int type,int sign):n(count),kind(type){
  data=fftw_alloc_complex(n);real=fftw_alloc_real(n+2);if(!data||!real){fftw_free(data);fftw_free(real);throw std::bad_alloc();}
  std::lock_guard<std::mutex> lock(planner);
  if(type==1)plan=fftw_plan_dft_1d(n,data,data,sign,FFTW_ESTIMATE);
  else if(type==0)plan=fftw_plan_dft_r2c_1d(n,real,data,FFTW_ESTIMATE);
  else plan=fftw_plan_dft_c2r_1d(n,data,real,FFTW_ESTIMATE);
  if(!plan){fftw_free(data);fftw_free(real);throw std::runtime_error("FFT plan creation failed");}
 }
 ~Plan(){std::lock_guard<std::mutex> lock(planner);fftw_destroy_plan(plan);fftw_free(data);fftw_free(real);}
};}
struct F2a::Impl {std::map<std::tuple<int,int,int>,std::unique_ptr<Plan>> plans;
 Plan &get(int n,int kind,int sign){if(n<=0||n>1441000)throw std::runtime_error("FFT length out of range");auto key=std::make_tuple(n,kind,sign);auto &p=plans[key];if(!p)p.reset(new Plan(n,kind,sign));return *p;}
};
F2a::F2a():impl(new Impl){} F2a::~F2a()=default;
void F2a::four2a_c2c(double complex *a,int n,int sign,int kind,int){auto &p=impl->get(n,kind,sign);std::memcpy(p.data,a,n*sizeof(fftw_complex));fftw_execute(p.plan);std::memcpy(a,p.data,n*sizeof(fftw_complex));}
void F2a::four2a_d2c(double complex *a,double *d,int n,int sign,int kind,int){auto &p=impl->get(n,kind,sign);if(kind==0){std::memcpy(p.real,d,n*sizeof(double));fftw_execute(p.plan);std::memcpy(a,p.data,(n/2+1)*sizeof(fftw_complex));}else{std::memcpy(p.data,a,(n/2+1)*sizeof(fftw_complex));fftw_execute(p.plan);std::memcpy(d,p.real,n*sizeof(double));}}
void F2a::DestroyPlansAll(bool){impl->plans.clear();}
}
