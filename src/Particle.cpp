#include "Particle.h"

#define PARTICLE_AGE_FADE (0.08f)

Particle::Particle() : 
	color(ci::CM_RGB, 1.0f, 1.0f, 1.0f),
	age(0),
	velocityScale(false)
{
}

void Particle::update()
{
	this->position += this->velocity;
	this->color.r *= 0.95f;
	this->color.g *= 0.95f;
	this->color.b *= 0.95f;

	if(this->color.r < PARTICLE_AGE_FADE &&
		this->color.g < PARTICLE_AGE_FADE &&
		this->color.b < PARTICLE_AGE_FADE)
	{
		this->age += 100;
	}
				
	this->age++;
}

void Particle::draw()
{
	gl::color(this->color);
	//gl::drawSolidRect(ci::Rectf(this->position[0], this->position[1], this->position[0] + this->scale[0], this->position[1] + this->scale[1]));
	//gl::drawSolidCircle(ci::Vec2f(this->position[0], this->position[1]), this->scale[0]);
	//glPointSize(this->scale[0]);
	glVertex2f(this->position[0], this->position[1]);
	//glEnd();
}


void Particle::drawScaled()
{
	gl::color(this->color);
	//gl::drawSolidRect(ci::Rectf(this->position[0], this->position[1], this->position[0] + this->scale[0], this->position[1] + this->scale[1]));
	//gl::drawSolidCircle(ci::Vec2f(this->position[0], this->position[1]), this->scale[0]);
	glPointSize(this->scale[0]);

	glBegin(GL_POINTS);
	glVertex2f(this->position[0], this->position[1]);
	glEnd();
	//glEnd();
}