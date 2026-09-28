#pragma once

#include "eatsbits/audio/procedural_ir_generator.hpp"

namespace eatsbits::audio::dsp {

// Backward compatibility aliases importing procedural IR types into eatsbits::audio::dsp
using AcousticMaterialType = eatsbits::audio::AcousticMaterialType;
using AcousticMaterial = eatsbits::audio::AcousticMaterial;
using AcousticSpaceParams = eatsbits::audio::AcousticSpaceParams;
using StereoIRBuffer = eatsbits::audio::StereoIRBuffer;
using ProceduralIRGenerator = eatsbits::audio::ProceduralIRGenerator;

} // namespace eatsbits::audio::dsp
