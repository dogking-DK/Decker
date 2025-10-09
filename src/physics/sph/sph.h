#pragma once
#include <memory>
#include <vector>

#include "kernal.h"
#include "World.h"

namespace dk {
struct HashGrid;

struct SPHParams {
        float h{ 0.05f };
        float rest_rho{ 1000.f };
        float mass{ 0.02f };
        float cfl{ 0.4f };
        float visc{ 0.1f };
        float eos_stiffness{ 2000.f }; // k in Tait EOS
        float eos_gamma{ 7.0f };
        glm::vec3 gravity{ 0,-9.81f,0 };
    };


    struct SPHParticle { glm::vec3 x{ 0 }, v{ 0 }; float rho{ 0 }, p{ 0 }; };


    class SPHFluid;


    class SPHTimeStep {
    public:
        virtual ~SPHTimeStep() = default;
        virtual void step(SPHFluid& f, float dt) = 0;
    };


    class SPHTimeStep_WCSPH final : public SPHTimeStep {
    public:
        void step(SPHFluid& f, float dt) override;
    };


    class SPHFluid final : public ISystem {
    public:
        explicit SPHFluid(SPHParams P) : P_(P), K_(P.h) {}
        void     setParticles(std::vector<SPHParticle> ps) { ps_ = std::move(ps); }
        void     setTimeStepper(std::unique_ptr<SPHTimeStep> ts) { ts_ = std::move(ts); }
        void     setBoundsY(float y_min) { y_min_ = y_min; }


        std::vector<SPHParticle>& particles() { return ps_; }
        const SPHParams& params() const { return P_; }
        //HashGrid& grid() { return grid_; }
        sphk::Kernels& kernels() { return K_; }


        void rebuildGrid();
        void computeDensityPressure();
        void step(float dt) override { if (ts_) ts_->step(*this, dt); }


    private:
        SPHParams P_;
        std::vector<SPHParticle> ps_;
        //HashGrid grid_{ /*cell=*/0.05f };
        sphk::Kernels K_;
        std::unique_ptr<SPHTimeStep> ts_;
        float y_min_{ -std::numeric_limits<float>::infinity() };


        friend class SPHTimeStep_WCSPH;
    };
} // namespace dk