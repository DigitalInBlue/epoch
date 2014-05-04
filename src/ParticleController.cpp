#include "ParticleController.h"

ParticleController::ParticleController() :
	maxAge(32),
	entropy(0)
{
}

void ParticleController::update()
{
	auto w = this->screenWidth;
	auto h = this->screenHeight;
	auto a = this->maxAge;

	this->particles.remove_if(
		[w, h, a](const Particle& p)->bool
		{
			return (p.age >= a || p.position[0] < 0 || p.position[1] < 0 || p.position[0] > w || p.position[1] > h);
		});

	std::for_each(std::begin(this->particles), std::end(this->particles),
		[](Particle& p)
		{
			p.update();
		});
}

void ParticleController::draw()
{
	// Two pass rendering.
	// Doing all the < 1 sized points in a single display list is really fast, but 
	// doesn't allow for adjustment to point size.

	// Little ones
	glPointSize(2);
	glBegin(GL_POINTS);
	{
		std::for_each(std::begin(this->particles), std::end(this->particles),
			[](Particle& p)
			{
				auto abs = fabs(p.scale[0]);
				if(abs <= 1.0f)
				{
					p.draw();
				}
			});
	}
	glEnd();

	// Little ones
	glPointSize(4);
	glBegin(GL_POINTS);
	{
		std::for_each(std::begin(this->particles), std::end(this->particles),
			[](Particle& p)
			{
				auto abs = fabs(p.scale[0]);
				if(abs > 1.0f && abs <= 2.0f)
				{
					p.draw();
				}
			});
	}
	glEnd();

	// Big Ones
	std::for_each(std::begin(this->particles), std::end(this->particles),
		[](Particle& p)
		{
			auto abs = fabs(p.scale[0]);
			if(abs > 2.0f)
			{
				p.drawScaled();
			}
		});
}

void ParticleController::addParticle(float x, float y, float value)
{
	this->addParticle(x, y, value, false);
}

void ParticleController::addParticle(float x, float y, float value, bool enableVelocityScale)
{
	std::array<float, 3> rgb = {1, 1, 1};
	this->addParticle(x, y, value, rgb, enableVelocityScale);
}

void ParticleController::addParticle(float x, float y, float value, std::array<float, 3> rgb)
{
	this->addParticle(x, y, value, rgb, false);
}

void ParticleController::addParticle(float x, float y, float value, std::array<float, 3> rgb, bool enableVelocityScale, bool xyVelocitySwap)
{
	auto e0 = ci::randFloat(-this->entropy, this->entropy) * pow(value, 2.0f);
	auto e1 = ci::randFloat(-this->entropy, this->entropy) * pow(value, 2.0f);
	auto e2 = ci::randFloat(-this->entropy, this->entropy) * pow(value, 2.0f);
	auto c0 = ci::randFloat(0.0f, 0.05f);
	auto c1 = ci::randFloat(0.0f, 0.05f);
	auto c2 = ci::randFloat(0.0f, 0.05f);

	Particle p;
	p.position.set(x, y, 0);

	auto scaleX = 1 + e2;
	auto scaleY = value + e1;
	auto scaleZ = 1 + e0;

	if(scaleX < -5.0f)
	{
		scaleX = -5;
	}

	if(scaleX > 5.0f)
	{
		scaleX = 5;
	}

	if(xyVelocitySwap == true)
	{
		std::swap(scaleX, scaleY);
	}

	p.scale.set(scaleX, scaleY, scaleZ);
	
	if(xyVelocitySwap == false)
	{
		p.velocity.set(e0, value + e1, e2);
	}
	else
	{
		p.velocity.set(value + e0, e1, e2);
	}

	p.color.r = rgb[0] + c0;
	p.color.g = rgb[1] + c1;
	p.color.b = rgb[2] + c2;
	p.velocityScale = enableVelocityScale;
	// p.ageFade = ci::randFloat(0.001f, 0.03f);

	if(fabs(value) < 0.2)
	{
		p.age = 16;

		if(xyVelocitySwap == false)
		{
			p.velocity.set(0, value * 1.666f, 0);
		}
		else
		{
			p.velocity.set(value * 1.666f, 0, 0);
		}
	}
	else if(fabs(value) < 0.4)
	{
		p.age = 16;
		
		if(xyVelocitySwap == false)
		{
			p.velocity.set(0, value * 1.333f, 0);
		}
		else
		{
			p.velocity.set(value * 1.333f, 0, 0);
		}
	}

	this->particles.push_back(std::move(p));
}
