#pragma once

#include "cinder/gl/gl.h"

using namespace ci;

class Particle
{
	public:
		Particle();
		void update();
		void draw();
		void drawScaled();

		ci::Vec3f velocity;
		ci::Vec3f position;
		ci::Vec3f scale;
		Color color;
		int age;
		bool velocityScale;
};
