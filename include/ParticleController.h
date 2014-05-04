#pragma once

#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/Rand.h"

#include "FMOD.hpp"
#include "Particle.h"
#include "ParticleController.h"

#include <vector>
#include <list>
#include <array>

using namespace ci;
using namespace ci::app;
using namespace std;

class ParticleController
{
	public:
		ParticleController();

		void update();
		void draw();
		void addParticle(float x, float y, float value);
		void addParticle(float x, float y, float value, bool enableVelocityScale);
		void addParticle(float x, float y, float value, std::array<float, 3> rgb);
		void addParticle(float x, float y, float value, std::array<float, 3> rgb, bool enableVelocityScale, bool xyVelocitySwap = false);

		std::list<Particle> particles;
		uint32_t maxAge;
		float entropy;
		int screenWidth;
		int screenHeight;
};
