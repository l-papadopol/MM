// SPDX-License-Identifier: GPL-3.0-or-later
#include "FtSyncCompute.h"
#define CL_TARGET_OPENCL_VERSION 120
#include "../../third_party/opencl_headers/CL/cl.h"
#include <QLibrary>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <thread>

namespace FtSyncCompute {
using Clock = std::chrono::steady_clock;
constexpr double twoPi = 6.283185307179586476925286766559;
int Input::timings() const { return tones.empty() ? 0 : int(starts.size() / tones.size()); }
size_t Input::cells() const { return starts.size() * size_t(std::max(0, frequencies)); }
bool Input::valid() const {
    if (samples.empty() || samples.size() > (toneEnergies ? 192000u : 24000u) || tones.empty() || tones.size() > (toneEnergies ? 103u : 32u) ||
        starts.size() > 64 * tones.size() || starts.size() % tones.size() || timings() < 1 || timings() > 64 ||
        frequencies < 2 || frequencies > 512 || symbolSamples < 1 || symbolSamples > (toneEnergies ? 1920 : 256) ||
        !std::isfinite(lowHz) || !std::isfinite(stepHz) || !std::isfinite(spacingHz) ||
        !std::isfinite(mixHz) || !std::isfinite(rate) || rate <= 0 || stepHz <= 0) return false;
    if (!std::all_of(starts.begin(),starts.end(),[this](int n){return n>=-24000 && n<=(toneEnergies ? 192000 : 48000);}) ||
        !std::all_of(tones.begin(),tones.end(),[](int n){return n>=0 && n<8;})) return false;
    return std::all_of(samples.begin(), samples.end(), [](const auto &v) {
        return std::isfinite(v.real()) && std::isfinite(v.imag());
    });
}
void cpuTiming(const Input &in, int timing, double *out) {
    for (int b = 0; b < int(in.tones.size()); ++b) for (int fi = 0; fi < in.frequencies; ++fi) {
        const int first = in.starts[size_t(timing) * in.tones.size() + b];
        double value = in.toneEnergies ? 0.0 : -1e9;
        if (first >= 0 && size_t(first + in.symbolSamples) <= in.samples.size()) {
            if (in.toneEnergies) {
                const double c=2*std::cos(twoPi*(in.lowHz+fi*in.stepHz)/in.rate);
                double a=0,prev=0;
                for(int n=0;n<in.symbolSamples;++n){const double v=in.samples[size_t(first+n)].real()+c*a-prev;prev=a;a=v;}
                value=std::max(0.0,a*a+prev*prev-c*a*prev);
                out[size_t(b) * in.frequencies + fi] = value;
                continue;
            }
            const double omega = -twoPi * (in.lowHz + fi * in.stepHz + in.tones[b] * in.spacingHz - in.mixHz) / in.rate;
            const std::complex<double> step(std::cos(omega), std::sin(omega));
            std::complex<double> osc(1,0), sum(0,0); double power = 0;
            for (int n = 0; n < in.symbolSamples; ++n) {
                const auto x = in.samples[size_t(first+n)];
                sum += x * osc; power += std::norm(x); osc *= step;
            }
            value = std::log1p(std::norm(sum) / (power + 1e-18));
        }
        out[size_t(b) * in.frequencies + fi] = value;
    }
}
bool equivalent(const std::vector<double> &cpu, const std::vector<double> &device, double *maxError) {
    if (cpu.size() != device.size() || cpu.empty()) return false;
    double worst = 0;
    for (size_t i = 0; i < cpu.size(); ++i) {
        if (!std::isfinite(cpu[i]) || !std::isfinite(device[i])) return false;
        const double error = std::abs(cpu[i]-device[i]); worst = std::max(worst,error);
        if (error > 1e-9 * std::max(1.0,std::abs(cpu[i]))) { if(maxError)*maxError=worst; return false; }
    }
    if (maxError) *maxError=worst;
    return true;
}
namespace {
const char *source = R"CLC(
#pragma OPENCL EXTENSION cl_khr_fp64 : enable
#pragma OPENCL FP_CONTRACT OFF
__kernel void costas(__global const double2 *samples, int sampleCount,
 __global const int *starts, __global const int *tones, int syncCount, int frequencies,
 int symbolSamples, double lowHz, double stepHz, double spacingHz, double mixHz,
 double rate, __global double *scores, int cells, int toneEnergies) {
 size_t id=get_global_id(0);if(id>=(size_t)cells)return;
 int fi=id%frequencies, sync=(id/frequencies)%syncCount;
 int first=starts[id/frequencies];
 if(first<0 || first+symbolSamples>sampleCount){scores[id]=toneEnergies?0:-1e9;return;}
 if(toneEnergies){
  double c=2*cos(6.283185307179586476925286766559*(lowHz+fi*stepHz)/rate),a=0,b=0;
  for(int n=0;n<symbolSamples;++n){double v=samples[first+n].x+c*a-b;b=a;a=v;}
  scores[id]=fmax(0.0,a*a+b*b-c*a*b);return;
 }
 double omega=-6.283185307179586476925286766559*(lowHz+fi*stepHz+tones[sync]*spacingHz-mixHz)/rate;
 double sc=cos(omega),ss=sin(omega),cr=1,ci=0,sr=0,si=0,power=0;
 for(int n=0;n<symbolSamples;++n){
  double2 x=samples[first+n];
  sr+=x.x*cr-x.y*ci;si+=x.x*ci+x.y*cr;power+=x.x*x.x+x.y*x.y;
  double next=cr*sc-ci*ss;ci=cr*ss+ci*sc;cr=next;
 }
 scores[id]=log1p((sr*sr+si*si)/(power+1e-18));
}
)CLC";
struct Runtime {
    QLibrary lib;
#define API(name) decltype(&name) p##name = nullptr
    API(clGetPlatformIDs); API(clGetDeviceIDs); API(clGetDeviceInfo);
    API(clCreateContext); API(clCreateCommandQueue); API(clCreateProgramWithSource);
    API(clBuildProgram); API(clGetProgramBuildInfo); API(clCreateKernel); API(clGetKernelWorkGroupInfo);
    API(clCreateBuffer); API(clSetKernelArg); API(clEnqueueWriteBuffer);
    API(clEnqueueNDRangeKernel); API(clEnqueueReadBuffer); API(clFinish);
    API(clReleaseMemObject); API(clReleaseKernel); API(clReleaseProgram);
    API(clReleaseCommandQueue); API(clReleaseContext);
#undef API
    cl_device_id device = nullptr;
    cl_context context = nullptr; cl_command_queue queue = nullptr;
    cl_program program = nullptr; cl_kernel kernel = nullptr;
    std::array<cl_mem,4> buffers{}; std::array<size_t,4> capacity{};
    Status info;
    size_t localSize=1;
    ~Runtime() {
        for(auto b:buffers) if(b && pclReleaseMemObject)pclReleaseMemObject(b);
        if(kernel)pclReleaseKernel(kernel); if(program)pclReleaseProgram(program);
        if(queue)pclReleaseCommandQueue(queue); if(context)pclReleaseContext(context);
    }
    bool fail(QString reason) { info.ready=false; info.unavailable=true; info.reason=std::move(reason); return false; }
    bool init(bool allowCpu) {
#ifdef Q_OS_WIN
        lib.setFileName(QStringLiteral("OpenCL"));
#elif defined(Q_OS_MACOS)
        lib.setFileName(QStringLiteral("/System/Library/Frameworks/OpenCL.framework/OpenCL"));
#else
        lib.setFileNameAndVersion(QStringLiteral("OpenCL"),1);
#endif
        if(!lib.load())return fail(QStringLiteral("OpenCL runtime unavailable"));
#define LOAD(name) p##name=reinterpret_cast<decltype(p##name)>(lib.resolve(#name)); if(!p##name)return fail(QStringLiteral("OpenCL entry point missing: " #name))
        LOAD(clGetPlatformIDs); LOAD(clGetDeviceIDs); LOAD(clGetDeviceInfo);
        LOAD(clCreateContext); LOAD(clCreateCommandQueue); LOAD(clCreateProgramWithSource);
        LOAD(clBuildProgram); LOAD(clGetProgramBuildInfo); LOAD(clCreateKernel); LOAD(clGetKernelWorkGroupInfo);
        LOAD(clCreateBuffer); LOAD(clSetKernelArg); LOAD(clEnqueueWriteBuffer);
        LOAD(clEnqueueNDRangeKernel); LOAD(clEnqueueReadBuffer); LOAD(clFinish);
        LOAD(clReleaseMemObject); LOAD(clReleaseKernel); LOAD(clReleaseProgram);
        LOAD(clReleaseCommandQueue); LOAD(clReleaseContext);
#undef LOAD
        cl_uint count=0;
        if(pclGetPlatformIDs(0,nullptr,&count)!=CL_SUCCESS || !count || count>64)
            return fail(QStringLiteral("No OpenCL platform"));
        std::vector<cl_platform_id> platforms(count);
        if(pclGetPlatformIDs(count,platforms.data(),nullptr)!=CL_SUCCESS)return fail(QStringLiteral("Platform enumeration failed"));
        for(auto platform:platforms){
            cl_uint n=0; const auto mask=allowCpu?CL_DEVICE_TYPE_ALL:CL_DEVICE_TYPE_GPU;
            if(pclGetDeviceIDs(platform,mask,0,nullptr,&n)!=CL_SUCCESS || n>128)continue;
            std::vector<cl_device_id> devices(n);
            if(n && pclGetDeviceIDs(platform,mask,n,devices.data(),nullptr)!=CL_SUCCESS)continue;
            for(auto candidate:devices){
                cl_device_fp_config fp=0; cl_bool available=CL_FALSE,compiler=CL_FALSE;
                pclGetDeviceInfo(candidate,CL_DEVICE_DOUBLE_FP_CONFIG,sizeof(fp),&fp,nullptr);
                pclGetDeviceInfo(candidate,CL_DEVICE_AVAILABLE,sizeof(available),&available,nullptr);
                pclGetDeviceInfo(candidate,CL_DEVICE_COMPILER_AVAILABLE,sizeof(compiler),&compiler,nullptr);
                if(fp && available && compiler){device=candidate;break;}
            }
            if(device)break;
        }
        if(!device)return fail(QStringLiteral("No eligible OpenCL GPU with FP64"));
        char name[512]={};cl_device_type type=0;
        pclGetDeviceInfo(device,CL_DEVICE_NAME,sizeof(name)-1,name,nullptr);
        pclGetDeviceInfo(device,CL_DEVICE_TYPE,sizeof(type),&type,nullptr);
        info.device=QString::fromUtf8(name);info.gpuDevice=(type&CL_DEVICE_TYPE_GPU)!=0;
        cl_int e=CL_SUCCESS;
        context=pclCreateContext(nullptr,1,&device,nullptr,nullptr,&e);
        if(!context||e)return fail(QStringLiteral("OpenCL context failed: %1").arg(e));
        queue=pclCreateCommandQueue(context,device,0,&e);
        if(!queue||e)return fail(QStringLiteral("OpenCL queue failed: %1").arg(e));
        program=pclCreateProgramWithSource(context,1,&source,nullptr,&e);
        if(!program||e)return fail(QStringLiteral("OpenCL program creation failed: %1").arg(e));
        e=pclBuildProgram(program,1,&device,"-cl-std=CL1.2",nullptr,nullptr);
        if(e){char log[2048]={};pclGetProgramBuildInfo(program,device,CL_PROGRAM_BUILD_LOG,sizeof(log)-1,log,nullptr);return fail(QStringLiteral("Kernel build failed: %1").arg(QString::fromUtf8(log)));}
        kernel=pclCreateKernel(program,"costas",&e);
        if(!kernel||e)return fail(QStringLiteral("OpenCL kernel creation failed: %1").arg(e));
        size_t maximum=1;
        if(pclGetKernelWorkGroupInfo(kernel,device,CL_KERNEL_WORK_GROUP_SIZE,sizeof(maximum),&maximum,nullptr)!=CL_SUCCESS)
            return fail(QStringLiteral("Kernel workgroup query failed"));
        localSize=std::min(size_t(64),maximum);
        if(!localSize)return fail(QStringLiteral("Invalid kernel workgroup size"));
        // Compile the first dispatch away from any slot deadline, and verify
        // actual device arithmetic before making this service available.
        Input warm;warm.samples.resize(1024);warm.tones={3,1,4,0,6,5,2};
        warm.starts={0,100,200,300,400,500,600};warm.frequencies=7;
        warm.symbolSamples=160;warm.lowHz=900;warm.stepHz=1.5625;
        warm.spacingHz=6.25;warm.mixHz=1022;
        for(size_t i=0;i<warm.samples.size();++i)warm.samples[i]={.13*std::cos(i*.17),.09*std::sin(i*.11)};
        std::vector<double> reference(warm.cells());cpuTiming(warm,0,reference.data());
        const auto verified=run(warm,Clock::now());
        if(!verified.ok||!equivalent(reference,verified.scores))return fail(QStringLiteral("OpenCL warmup numerical self-test failed"));
        // Also warm representative full batches. Some runtimes specialize the
        // first large launch, not only the first kernel invocation.
        for(bool ft4 : {false,true}) {
            Input full;full.samples.resize(ft4?7500:15000,{.01,.02});
            full.symbolSamples=ft4?48:160;full.frequencies=211;
            full.lowHz=850;full.stepHz=ft4?5.208333333:1.5625;
            full.spacingHz=ft4?20.833333333:6.25;full.mixHz=1055;
            const int count=ft4?16:21;
            for(int k=0;k<count;++k)full.tones.push_back(k%(ft4?4:8));
            for(int ti=0;ti<(ft4?35:26);++ti)
                for(int k=0;k<count;++k)full.starts.push_back(100+ti*24+k*(ft4?400:600));
            if(!run(full,Clock::now()).ok)return fail(QStringLiteral("OpenCL full-batch warmup failed"));
        }
        for(bool ft4 : {false,true}) {
            Input normal;normal.toneEnergies=true;normal.symbolSamples=ft4?576:1920;
            normal.frequencies=ft4?4:8;normal.rate=12000;normal.lowHz=1000;
            normal.stepHz=ft4?12000./576.:6.25;
            const int count=ft4?103:79;
            normal.samples.resize(size_t(count)*normal.symbolSamples,{.01,0});
            normal.tones.resize(count,0);
            for(int k=0;k<count;++k)normal.starts.push_back(k*normal.symbolSamples);
            std::vector<double> ref(normal.cells());cpuTiming(normal,0,ref.data());
            auto result=run(normal,Clock::now());
            if(!result.ok||!equivalent(ref,result.scores))return fail(QStringLiteral("OpenCL demodulation self-test failed"));
        }
        info.ready=true;info.reason=QStringLiteral("ready; warmup parity passed");return true;
    }
    bool ensure(int i,size_t bytes,cl_mem_flags flags) {
        if(capacity[i]>=bytes)return true;
        if(buffers[i]){pclReleaseMemObject(buffers[i]);buffers[i]=nullptr;capacity[i]=0;}
        cl_int e;buffers[i]=pclCreateBuffer(context,flags,bytes,nullptr,&e);
        if(!buffers[i]||e)return false;
        capacity[i]=bytes;return true;
    }
    Result run(const Input &in,Clock::time_point submitted) {
        Result out;out.device=info.device;out.gpuDevice=info.gpuDevice;
        auto error=[&](QString reason){out.reason=std::move(reason);out.totalMs=std::chrono::duration<double,std::milli>(Clock::now()-submitted).count();return out;};
        if(!in.valid())return error(QStringLiteral("Invalid correlation batch"));
        // Do not rely on std::complex object layout when transferring to double2.
        std::vector<std::array<double,2>> packed;packed.reserve(in.samples.size());
        for(auto x:in.samples)packed.push_back({x.real(),x.imag()});
        const size_t sizes[4]={packed.size()*sizeof(packed[0]),in.starts.size()*sizeof(int),in.tones.size()*sizeof(int),in.cells()*sizeof(double)};
        for(int i=0;i<4;++i)if(!ensure(i,sizes[i],i==3?CL_MEM_WRITE_ONLY:CL_MEM_READ_ONLY))return error(QStringLiteral("Device buffer allocation failed"));
        struct Drain { Runtime *r; ~Drain(){r->pclFinish(r->queue);} } drain{this};
        const void *data[3]={packed.data(),in.starts.data(),in.tones.data()};
        for(int i=0;i<3;++i)if(pclEnqueueWriteBuffer(queue,buffers[i],CL_FALSE,0,sizes[i],data[i],0,nullptr,nullptr)!=CL_SUCCESS){pclFinish(queue);return error(QStringLiteral("Device upload failed"));}
        int n=int(in.samples.size()),sync=int(in.tones.size());cl_int e=0;
        auto arg=[&](cl_uint i,size_t size,const void *value){e|=pclSetKernelArg(kernel,i,size,value);};
        arg(0,sizeof(cl_mem),&buffers[0]);arg(1,sizeof(n),&n);arg(2,sizeof(cl_mem),&buffers[1]);arg(3,sizeof(cl_mem),&buffers[2]);
        arg(4,sizeof(sync),&sync);arg(5,sizeof(in.frequencies),&in.frequencies);arg(6,sizeof(in.symbolSamples),&in.symbolSamples);
        arg(7,sizeof(double),&in.lowHz);arg(8,sizeof(double),&in.stepHz);arg(9,sizeof(double),&in.spacingHz);arg(10,sizeof(double),&in.mixHz);arg(11,sizeof(double),&in.rate);arg(12,sizeof(cl_mem),&buffers[3]);
        int cells=int(in.cells());arg(13,sizeof(cells),&cells);
        int toneEnergies=in.toneEnergies?1:0;arg(14,sizeof(toneEnergies),&toneEnergies);
        size_t size=in.cells(),global=((size+localSize-1)/localSize)*localSize;
        if(e||pclEnqueueNDRangeKernel(queue,kernel,1,nullptr,&global,&localSize,0,nullptr,nullptr)!=CL_SUCCESS){pclFinish(queue);return error(QStringLiteral("Kernel submission failed"));}
        out.scores.resize(size);
        if(pclEnqueueReadBuffer(queue,buffers[3],CL_TRUE,0,sizes[3],out.scores.data(),0,nullptr,nullptr)!=CL_SUCCESS){pclFinish(queue);return error(QStringLiteral("Device readback failed"));}
        if(!std::all_of(out.scores.begin(),out.scores.end(),[](double v){return std::isfinite(v);}))return error(QStringLiteral("Non-finite device scores"));
        out.ok=true;out.totalMs=std::chrono::duration<double,std::milli>(Clock::now()-submitted).count();return out;
    }
};
}
struct Engine::Impl {
    mutable std::mutex mutex;std::condition_variable wake;bool stop=false,busy=false;
    Status state;
    struct Job {std::shared_ptr<const Input> input;std::promise<Result> promise;Clock::time_point submitted;};
    std::shared_ptr<Job> pending;std::thread thread;
    explicit Impl(bool allowCpu):thread([this,allowCpu]{
        Runtime runtime;
        try{runtime.init(allowCpu);}catch(...){runtime.fail(QStringLiteral("OpenCL initialization exception"));}
        {std::lock_guard<std::mutex> lock(mutex);state=runtime.info;}
        while(true){
            std::shared_ptr<Job> job;
            {std::unique_lock<std::mutex> lock(mutex);wake.wait(lock,[&]{return stop||pending;});
             if(stop){if(pending){Result r;r.reason=QStringLiteral("Stopped");pending->promise.set_value(std::move(r));pending.reset();}break;}
             job=std::move(pending);}
            Result result;
            try{result=runtime.run(*job->input,job->submitted);}catch(...){result.reason=QStringLiteral("OpenCL execution exception");}
            job->promise.set_value(std::move(result));
            {std::lock_guard<std::mutex> lock(mutex);busy=false;}
        }
    }){}
    ~Impl(){ {std::lock_guard<std::mutex> lock(mutex);stop=true;}wake.notify_one();if(thread.joinable())thread.join(); }
};
Engine::Engine(bool allowCpu):m(new Impl(allowCpu)){}
Engine::~Engine()=default;
Engine &Engine::instance(){static Engine engine;return engine;}
Status Engine::status()const{std::lock_guard<std::mutex> lock(m->mutex);return m->state;}
std::shared_future<Result> Engine::submit(std::shared_ptr<const Input> input){
    if(!input||!input->valid())return {};
    auto job=std::make_shared<Impl::Job>();job->input=std::move(input);job->submitted=Clock::now();
    auto future=job->promise.get_future().share();
    {std::lock_guard<std::mutex> lock(m->mutex);if(!m->state.ready||m->busy||m->stop)return {};m->busy=true;m->pending=std::move(job);}
    m->wake.notify_one();return future;
}
}
