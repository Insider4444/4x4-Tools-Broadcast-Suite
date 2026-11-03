#include "nvidia/video_effects.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

int wmain(int argc, wchar_t** argv) {
    using namespace fourbyfour;
    try {
        if (argc < 5) { std::cerr << "VideoEffectsCli SDK_ROOT matte|blur|relight|denoise|restore|upscale input.ppm output.ppm [scale]\n"; return 2; }
        const std::map<std::wstring,VideoEffect> effects{{L"matte",VideoEffect::matte},{L"blur",VideoEffect::blur},{L"relight",VideoEffect::relight},
            {L"denoise",VideoEffect::denoise},{L"restore",VideoEffect::superResolution},{L"upscale",VideoEffect::upscale}};
        const auto kind=effects.at(argv[2]);
        std::ifstream input(std::filesystem::path(argv[3]),std::ios::binary);
        std::string magic; int w{},h{},range{}; input>>magic>>w>>h>>range; input.get();
        if(magic!="P6"||w<1||h<1||range!=255) throw std::runtime_error("Use a binary P6 8-bit PPM.");
        std::vector<unsigned char> rgb(static_cast<std::size_t>(w)*h*3);
        if(!input.read(reinterpret_cast<char*>(rgb.data()),rgb.size())) throw std::runtime_error("Truncated input.");
        std::vector<float> bgr(rgb.size());
        for(std::size_t i=0;i<rgb.size();i+=3) for(int c=0;c<3;++c) bgr[i+c]=rgb[i+2-c]/255.f;
        VideoSettings settings; settings.strength=kind==VideoEffect::blur?.5f:1.f;
        if(argc>5) { const int scale=std::stoi(argv[5]); settings.outputWidth=w*scale; settings.outputHeight=h*scale; }
        auto start=std::chrono::steady_clock::now();
        auto result=processVideo(kind,bgr,w,h,settings,argv[1]);
        auto loaded=std::chrono::steady_clock::now();
        auto repeat=processVideo(kind,bgr,w,h,settings,argv[1]);
        auto end=std::chrono::steady_clock::now();
        float repeatError=0;
        for(std::size_t i=0;i<result.bgr.size();++i) repeatError=std::max(repeatError,std::abs(result.bgr[i]-repeat.bgr[i]));
        for(std::size_t i=0;i<result.matte.size();++i) repeatError=std::max(repeatError,std::abs(result.matte[i]-repeat.matte[i]));
        if(repeatError>1.f/255.f) throw std::runtime_error("Image output depends on previous calls.");
        rgb.resize(static_cast<std::size_t>(result.width)*result.height*3);
        double sum=0,change=0; float minimum=1.e9f,maximum=-1.e9f;
        for(std::size_t i=0;i<rgb.size();i+=3) for(int c=0;c<3;++c) {
            const float v=kind==VideoEffect::matte?result.matte[i/3]:result.bgr[i+2-c];
            if(!std::isfinite(v)) throw std::runtime_error("Non-finite model output.");
            sum+=v; minimum=std::min(minimum,v); maximum=std::max(maximum,v);
            if(result.bgr.size()==bgr.size()&&!result.bgr.empty()) change+=std::abs(result.bgr[i+c]-bgr[i+c]);
            rgb[i+c]=static_cast<unsigned char>(std::lround(std::clamp(v,0.f,1.f)*255));
        }
        std::ofstream output(std::filesystem::path(argv[4]),std::ios::binary);
        output<<"P6\n"<<result.width<<' '<<result.height<<"\n255\n";
        output.write(reinterpret_cast<const char*>(rgb.data()),rgb.size());
        std::cout<<"size="<<result.width<<'x'<<result.height<<" mean="<<sum/rgb.size()<<" range="<<minimum<<","<<maximum<<" change="<<change/rgb.size()
            <<" repeat_error="<<repeatError<<" load_and_first_ms="<<std::chrono::duration<double,std::milli>(loaded-start).count()
            <<" warm_ms="<<std::chrono::duration<double,std::milli>(end-loaded).count()<<'\n';
        shutdownVideo(); return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
