#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/TextureFont.h"
#include "cinder/Rand.h"
#include "cinder/ImageIo.h"

#include "FMOD.hpp"
#include "Particle.h"
#include "ParticleController.h"

#define TAGLIB_STATIC 

#include "tag.h"
#include "fileref.h"
#include "tpropertymap.h"
#include <id3v2tag.h>
#include <mpegfile.h>
#include <attachedpictureframe.h>

#include <iostream>
#include <vector>
#include <list>
#include <array>

#include <stdio.h>

using namespace ci;
using namespace ci::app;
using namespace std;

class EpochVisualizer : public AppNative 
{
	public:
		EpochVisualizer() : AppNative(),
			playListTrackNumber(0),
			velocityScale(1.0f),
			fontSize(14.0f),
			masterVolume(0.75f),
			sampleSize(1024),
			waveSampleSize(1024),
			mouseX(0),
			mouseY(0),
			domain(Domain_Time),
			useAbsoluteValue(false),
			useGreyscale(false),
			useVelocityScale(false),
			useWaveColoring(false),
			enableHelp(false),
			enableAlbumArt(false),
			enableCredits(false),
			enableClearScreen(true),
			isShiftDown(false)
		{

		}

		enum Domain
		{
			Domain_Time,
			Domain_FreqMirrorDB,
			// Domain_StereoTime,
			Domain_Mixed,
			Domain_End
		};

		void prepareSettings(Settings* settings);
		void setup();
		void draw();
		void update();
		void keyDown(KeyEvent evt);
		void keyUp(KeyEvent evt);
		void mouseDown(MouseEvent evt);
		void mouseDrag(MouseEvent evt);
		void fileDrop(ci::app::FileDropEvent evt);
		
	protected:
		void drawHelp();

		std::array<float, 3> getValueColor(float x) const;
		std::vector<float> getWaveData();
		std::vector<float> getStereoWaveData();
		std::vector<float> getSpectrumDataMirrorDB();

		void soundComplete();

		void loadFile(const std::string& filename);
		void loadFileMP3(const std::string& filename);
		void loadFilePlaylist(const std::string& filename);

		void loadPlaylist();
		void savePlaylist();

		void nextTrack();
		void previousTrack();

	private:
		std::vector<std::string> playList;
		ParticleController particles;

		gl::Texture albumArt;
		
		gl::TextureFontRef fontTexture;
		ci::Font font;

		std::string debugMe;
		std::string artist;
		std::string album;
		std::string title;

		FMOD::System* fmodSystem;
		FMOD::Sound* fmodSound;
		FMOD::Channel* fmodChannel;
		FMOD::ChannelGroup* fmodChannelGroup;

		float velocityScale;
		float fontSize;
		float masterVolume;

		size_t playListTrackNumber;

		int sampleSize;
		int waveSampleSize;
		int mouseX;
		int mouseY;
		int domain;

		int albumArtSize;
		int albumArtReflectionShift;
		int albumArtReflectionHeight;
		int albumArtReflectionOffset;
		int albumArtBorder;
		int albumArtTopX;
		int albumArtTopY;

		bool useAbsoluteValue;
		bool useGreyscale;
		bool useVelocityScale;
		bool useWaveColoring;
		bool enableHelp;
		bool enableAlbumArt;
		bool enableCredits;
		bool enableClearScreen;
		bool isShiftDown;
		bool mixedDomainFlag;
};

void EpochVisualizer::prepareSettings(Settings* settings)
{
	settings->setTitle("Epoch");
	settings->setFrameRate(60.0f);
}

void EpochVisualizer::setup()
{
	this->setFpsSampleInterval(1.0f/30.0f);
	
	FMOD::System_Create(&this->fmodSystem);
	this->fmodSystem->init(2, FMOD_INIT_NORMAL | FMOD_INIT_ENABLE_PROFILE, nullptr);
	this->fmodSystem->createChannelGroup(nullptr, &this->fmodChannelGroup);

	this->particles.maxAge = 32;
	this->velocityScale = 5;
	this->useAbsoluteValue = false;
	this->useGreyscale = false;
	this->useWaveColoring = false;
	this->mouseX = this->getWindowWidth()/2;
	this->mouseY = this->getWindowHeight()/2;

	this->font = Font(this->loadAsset("Arial.ttf" ), this->fontSize);
	this->fontTexture = gl::TextureFont::create(this->font);

	auto args = this->getArgs();

	if(args.size() <= 1)
	{
		this->fmodSystem->createSound(ci::app::getAssetPath( "Blank__Kytt_-_08_-_RSPN.mp3" ).string().c_str(), FMOD_SOFTWARE, nullptr, &fmodSound );
		this->fmodSound->setMode(FMOD_LOOP_NORMAL);
		this->fmodSystem->playSound(FMOD_CHANNEL_FREE, this->fmodSound, false, &this->fmodChannel);
		this->fmodChannel->setChannelGroup(this->fmodChannelGroup);
	}
	else
	{
		std::string fileName = args[1].c_str();
		this->loadFile(fileName);
	}	
}

void EpochVisualizer::fileDrop(ci::app::FileDropEvent evt)
{
	if(evt.getNumFiles() == 1)
	{
		auto file = evt.getFile(0);
		auto filePreferred = file.make_preferred();
		std::string fileName = filePreferred.string();
		this->loadFile(fileName);
	}
	else
	{
		if(this->isShiftDown == false)
		{
			this->playList.clear();
		}

		for(size_t i = 0; i < evt.getNumFiles(); i++)
		{
			auto file = evt.getFile(i);
			auto filePreferred = file.make_preferred();
			std::string fileName = filePreferred.string();
			this->playList.push_back(fileName);
		}
	}
}

void EpochVisualizer::loadFile(const std::string& fileName)
{
	// find the extension of the file we are loading.
	std::string dropExtension = fileName;

	if(dropExtension.find('.') != std::string::npos)
	{
		dropExtension = dropExtension.substr(dropExtension.find_last_of(".") + 1);
	}

	// Make lower case.
	std::transform(dropExtension.begin(), dropExtension.end(), dropExtension.begin(), ::tolower);

	if(dropExtension == "mp3")
	{
		this->loadFileMP3(fileName);
	}
	else if(dropExtension == "epoch")
	{
		this->loadFilePlaylist(fileName);
	}
}

void EpochVisualizer::loadFileMP3(const std::string& fileName)
{
	this->fmodChannel->stop();

	FMOD::System_Create(&this->fmodSystem);
	this->fmodSystem->init(2, FMOD_INIT_NORMAL | FMOD_INIT_ENABLE_PROFILE, nullptr);
	this->fmodSystem->createChannelGroup(nullptr, &this->fmodChannelGroup);
	this->fmodSystem->createSound(fileName.c_str(), FMOD_SOFTWARE, NULL, &this->fmodSound);
	this->fmodSound->setMode(FMOD_LOOP_OFF);
	this->fmodSystem->playSound(FMOD_CHANNEL_FREE, this->fmodSound, false, &this->fmodChannel);
	this->fmodChannel->setChannelGroup(this->fmodChannelGroup);

	// Album Art
	{
		TagLib::MPEG::File f(fileName.c_str());

		if(f.isValid() == true)
		{
			auto tag = f.ID3v2Tag();

			if(tag != nullptr)
			{
				this->artist = std::string(tag->artist().toCString());
				this->album = std::string(tag->album().toCString()) + " (" + std::to_string(tag->year()) + ")";
				this->title = std::string(tag->title().toCString());

				TagLib::ID3v2::FrameList l = tag->frameList("APIC");

				if(l.isEmpty() == false)
				{
					auto apf = static_cast<TagLib::ID3v2::AttachedPictureFrame*>(l.front());

					if(apf != nullptr && apf->picture().data() != nullptr && apf->picture().size() > 0)
					{
						std::string extension = std::string(apf->mimeType().toCString());

						if(extension.find('/') != std::string::npos)
						{
							extension = extension.substr(extension.find_last_of("/") + 1);
						}

						this->debugMe = extension;

						std::ofstream os;
						std::string debugFileName = "debug." + extension;
						os.open(debugFileName.c_str(), std::ios_base::binary);
						os.write(apf->picture().data(), apf->picture().size());
						os.close();

						try
						{
							cinder::Buffer buff(apf->picture().data(), apf->picture().size());
							auto albumImage = ci::loadImage(debugFileName.c_str());
							// albumImage = ci::loadImage(DataSourceBuffer::create(buff), ImageSource::Options(), extension.c_str());
							this->albumArt = gl::Texture(albumImage);
							this->enableAlbumArt = true;
						}
						catch(...)
						{
							this->enableAlbumArt = false;
							this->debugMe += " EXCEPTION";
						}
					}
				}
			}
		}
	}
}

void EpochVisualizer::loadFilePlaylist(const std::string& fileName)
{
	std::ifstream is;
	is.open(fileName);

	if(is.is_open() == true)
	{
		this->playList.clear();

		while(is.eof() == false)
		{
			std::string line;
			std::getline(is, line);

			if(line.empty() == false)
			{
				this->playList.push_back(line);
			}
		}
	}

	this->playListTrackNumber = this->playList.size();
	this->nextTrack();
}

void EpochVisualizer::keyDown(KeyEvent evt)
{
	auto key = evt.getChar();

	this->isShiftDown = evt.isShiftDown();

	switch(key)
	{
		case KeyEvent::KEY_UP:
		case '>':
			this->masterVolume = std::min(this->masterVolume + 0.1f, 1.0f);
			break;

		case KeyEvent::KEY_DOWN:
		case '<':
			this->masterVolume = std::max(this->masterVolume - 0.1f, 0.0f);
			break;

		case KeyEvent::KEY_ESCAPE:
			this->setFullScreen(false);
			break;

		//case 'a':
		//case 'A':
		//	this->useAbsoluteValue = !this->useAbsoluteValue;
		//	break;

		case 'd':
		case 'D':
			this->domain++;
			if(this->domain == Domain_End)
			{
				this->domain = 0;
			}
			break;

		case 'v':
			this->velocityScale -= 0.1f;
			break;

		case 'V':
			this->velocityScale += 0.1f;
			break;

		case 'g':
			if(this->particles.maxAge > 0)
			{
				this->particles.maxAge--;
			}
			break;

		case 'G':
			this->particles.maxAge++;
			break;

		case 'h':
		case 'H':
			this->enableHelp = !this->enableHelp;
			break;

		case 'b':
		case 'B':
			this->useGreyscale = !this->useGreyscale;
			break;

		case 'r':
		case 'R':
			this->useVelocityScale = !this->useVelocityScale;
			break;

		case 's':
		case 'S':
			if(evt.isControlDown() == true)
			{
				this->savePlaylist();
			}
			break;

		case 'o':
		case 'O':
			if(evt.isControlDown() == true)
			{
				this->loadPlaylist();
			}
			break;

		case 'w':
		case 'W':
			this->useWaveColoring = !this->useWaveColoring;
			break;

		case 'c':
		case 'C':
			this->enableCredits = !this->enableCredits;
			break;

		case 'e':
			this->particles.entropy -= 0.02f;

			if(this->particles.entropy < 0)
			{
				this->particles.entropy = 0;
			}
			break;

		case 'E':
			this->particles.entropy += 0.02f;
			break;

		case 'f':
		case 'F':
			this->setFullScreen(!this->isFullScreen());
			break;

		case 'q':
		case 'Q':
			this->quit();
			break;

		case 'i':
		case 'I':
			this->enableClearScreen = !this->enableClearScreen;
			break;

		case 'm':
		case 'M':
			{
				bool muted;
				this->fmodChannelGroup->getMute(&muted);
				this->fmodChannelGroup->setMute(!muted);
			}
			break;

		case '+':
			this->nextTrack();
			break;

		case '-':
			this->previousTrack();
			break;

		case ' ':
			{
				bool paused;
				this->fmodChannelGroup->getPaused(&paused);
				this->fmodChannelGroup->setPaused(!paused);
			}
			break;

		default:
			break;
	};
}

void EpochVisualizer::keyUp(KeyEvent evt)
{
	this->isShiftDown = evt.isShiftDown();
}

void EpochVisualizer::mouseDown(MouseEvent evt)
{
	this->mouseDrag(evt);
}

void EpochVisualizer::mouseDrag(MouseEvent evt)
{
	if(evt.isShiftDown() == true)
	{
		this->mouseX = evt.getX();

		if(evt.isMiddleDown() == true)
		{
			this->waveSampleSize = evt.getX();
		}
		else if(evt.isLeftDown() == true)
		{
			this->velocityScale = (this->getWindowHeight() - evt.getY()) / 8.0f;
		}
		else
		{
			this->particles.entropy = (static_cast<float>(this->getWindowHeight() - evt.getY()) / static_cast<float>(this->getWindowHeight())) * 0.2f;
		}
	}
}

void EpochVisualizer::update()
{
	// Check to see if we need to load another track.
	{
		bool soundIsPlaying(false);
		this->fmodChannel->isPlaying(&soundIsPlaying);
		if(soundIsPlaying == false)
		{
			this->soundComplete();
		}
	}

	// Update master volume level.
	this->fmodChannelGroup->setVolume(this->masterVolume);

	// Update visualization Data
	{
		std::vector<float> waveData;
		
		if(this->domain == Domain_Time || (this->domain == Domain_Mixed && this->mixedDomainFlag == true))
		{
			waveData = this->getWaveData();
		}
		else 
		{
			waveData = this->getSpectrumDataMirrorDB();
		}

		this->mixedDomainFlag = !this->mixedDomainFlag;

		auto maxValue = fabs(*std::max_element(std::begin(waveData), std::end(waveData)));

		auto rgb = this->getValueColor(maxValue);

		for(size_t i = 0; i < waveData.size(); ++i)
		{
			auto value = waveData[i];
			auto absValue = std::abs(value);
		
			auto xPos = (static_cast<float>(this->getWindowWidth()) / static_cast<float>(waveData.size())) * i;
			auto yPos = this->getWindowCenter().y;

			if(this->useAbsoluteValue == true)
			{
				yPos = static_cast<float>(this->getWindowHeight());
			}

			if(this->useWaveColoring == false)
			{
				rgb = this->getValueColor(absValue);
			}

			this->particles.addParticle(xPos, yPos, value * this->velocityScale, rgb, this->useVelocityScale);
		}

		this->particles.screenHeight = this->getWindowHeight();
		this->particles.screenWidth = this->getWindowWidth();
		this->particles.update();
	}

	// Update album art and credits data
	{
		this->albumArtSize = this->getWindowWidth() / 10;
		this->albumArtReflectionShift = -(this->albumArtSize / 4);
		this->albumArtReflectionHeight = this->albumArtSize / 2;
		this->albumArtReflectionOffset = 2;
		this->albumArtBorder = this->getWindowWidth() / 40;

		this->albumArtTopX = this->getWindowWidth() - this->albumArtSize - this->albumArtBorder;
		this->albumArtTopY = this->getWindowHeight() - this->albumArtSize - this->albumArtReflectionHeight - this->albumArtReflectionOffset - this->albumArtBorder;
	}
}

void EpochVisualizer::draw()
{
	gl::enableAlphaBlending(true);

	if(this->enableClearScreen == true)
	{
		gl::clear(Color(0, 0, 0));
	}
	else
	{
		glColor4f(0, 0, 0, 0.05f);
		glBegin(GL_QUADS);
		{
			glVertex2f(0, 0);
			glVertex2f(this->getWindowWidth(), 0);
			glVertex2f(this->getWindowWidth(), this->getWindowHeight());
			glVertex2f(0, this->getWindowHeight());
		}
		glEnd();
	}

	this->particles.draw();

	if(this->enableHelp == true)
	{
		this->drawHelp();
	}
	
	if(this->enableCredits == true)
	{
		glColor3f(0.9f, 0.9f, 0.9f);
		
		TextLayout layout; 
		layout.setColor(cinder::ColorA(1.0f, 1.0f, 1.0f));
		layout.setFont(this->font);
		layout.setLeadingOffset(3.0f);

		layout.addLine(this->artist);
		layout.addLine(this->album);
		layout.addLine(this->title);

		auto creditTexture = gl::Texture(layout.render(true, true));
		creditTexture.enableAndBind();
		gl::draw(creditTexture, Vec2f(this->albumArtBorder, this->albumArtTopY));
		creditTexture.disable();

		if(this->enableAlbumArt == true)
		{
			this->albumArt.enableAndBind();

			glBegin(GL_QUADS);
			{
				glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
				glTexCoord2f(0, 0);
				glVertex2f(this->albumArtTopX, this->albumArtTopY);

				glTexCoord2f(1, 0);
				glVertex2f(this->albumArtTopX + this->albumArtSize, this->albumArtTopY);

				glTexCoord2f(1, 1);
				glVertex2f(this->albumArtTopX + this->albumArtSize, this->albumArtTopY + this->albumArtSize);

				glTexCoord2f(0, 1);
				glVertex2f(this->albumArtTopX, this->albumArtTopY + this->albumArtSize);

				// Reflection
				glColor4f(0.7f, 0.7f, 0.7f, 1.0f);
				glTexCoord2f(0, 1);
				glVertex2f(this->albumArtTopX, this->albumArtTopY + albumArtSize + this->albumArtReflectionOffset);

				glColor4f(0.75f, 0.75f, 0.75f, 1.0f);
				glTexCoord2f(1, 1);
				glVertex2f(this->albumArtTopX + this->albumArtSize , this->albumArtTopY + albumArtSize + this->albumArtReflectionOffset);

				glColor4f(0.0f, 0.0f, 0.0f, 0.01f);
				glTexCoord2f(1, 0);
				glVertex2f(this->albumArtTopX + this->albumArtSize + this->albumArtReflectionShift, this->albumArtTopY + albumArtSize + this->albumArtReflectionOffset + this->albumArtReflectionHeight);

				glTexCoord2f(0, 0);
				glVertex2f(this->albumArtTopX + this->albumArtReflectionShift, this->albumArtTopY + this->albumArtSize + this->albumArtReflectionOffset + this->albumArtReflectionHeight);
			}
			glEnd();

			this->albumArt.disable();
		}
	}
}

void EpochVisualizer::drawHelp()
{
	gl::color(cinder::ColorA(1.0f, 1.0f,1.0f));

	TextLayout layout; 
	layout.setColor(cinder::ColorA(1.0f, 1.0f, 1.0f));
	layout.setFont(this->font);
	layout.setLeadingOffset(3.0f);

	layout.addLine(std::to_string(this->getAverageFps()));
	layout.addLine("");
	layout.addLine("> - Volume Up");
	layout.addLine("< - Volume Down");
	layout.addLine("b - Toggle Enable Greyscale");
	layout.addLine("d - Domain Toggle");
	layout.addLine("E - Entropy Up");
	layout.addLine("e - Entropy Down");
	layout.addLine("f - Toggle Full Screen");
	layout.addLine("g - Particle Max Age Down");
	layout.addLine("G - Particle Max Age Up");
	layout.addLine("H - Help");
	layout.addLine("m - Mute");
	layout.addLine("q - Quit");
	layout.addLine("r - Toggle Enable Velocity Scale");
	layout.addLine("i - Toggle Image Clear / Image Fade & Burn");
	layout.addLine("v - Velocity Scale Down");
	layout.addLine("V - Velocity Scale Up");
	layout.addLine("w - Toggle Wave Coloring");
	layout.addLine("CTRL-S - Save Playlist");
	layout.addLine("CTRL-O - Open Playlist");

	auto helpTexture = gl::Texture(layout.render(true, true));
	helpTexture.enableAndBind();
	gl::draw(helpTexture, Vec2f(this->albumArtBorder, this->albumArtBorder));
	helpTexture.disable();
}

std::array<float, 3> EpochVisualizer::getValueColor(float x) const
{
	std::array<float, 3> rgb;

	if(this->domain == EpochVisualizer::Domain_Time)
	{
		if(this->useGreyscale == true)
		{
			rgb[0] = x * this->velocityScale/2;
			rgb[1] = rgb[0];
			rgb[2] = rgb[0];
		}
		else
		{
			if(x < 0.2f)
			{
				rgb[0] = (x / 0.2f)/2.0f;
				rgb[1] = rgb[0];
				rgb[2] = rgb[0];
			}
			else if(x < 0.4f)
			{
				rgb[0] = 0.2f;
				rgb[1] = 0.2f;
				rgb[2] = (x / 0.4f);
			}
			else if(x < 0.6f)
			{
				rgb[0] = ((x - 0.4f) / 0.2f);
				rgb[1] = 0.35f;
				rgb[2] = 0.38f;
			}
			else if(x < 0.8f)
			{
				rgb[0] = ((x - 0.6f) / 0.2f);
				rgb[1] = 0.556f;
				rgb[2] = 0.556f;
			}
			else if(x < 0.9f)
			{
				rgb[0] = 0.9f;
				rgb[1] = 0.9f;
				rgb[2] = x / 0.9f;
			}
			else
			{
				rgb[0] = x / 0.9f;
				rgb[1] = 0.5f + 1.0f - rgb[0];
				rgb[2] = rgb[1];
			}
		}
	}
	else
	{
		if(this->useGreyscale == true)
		{
			rgb[0] = x * this->velocityScale/2;
			rgb[1] = rgb[0];
			rgb[2] = rgb[0];
		}
		else
		{
			if(x < 0.05f)
			{
				rgb[0] = (x / 0.05f)/2;
				rgb[1] = rgb[0];
				rgb[2] = rgb[0];
			}
			else if(x < 0.1f)
			{
				rgb[0] = 0.1f;
				rgb[1] = 0.1f;
				rgb[2] = (x / 0.1f);
			}
			else if(x < 0.2f)
			{
				rgb[0] = ((x - 0.1f) / 0.1f);
				rgb[1] = 0.35f;
				rgb[2] = 0.38f;
			}
			else if(x < 0.4f)
			{
				rgb[0] = ((x - 0.2f) / 0.2f);
				rgb[1] = 0.556f;
				rgb[2] = 0.556f;
			}
			else if(x < 0.6f)
			{
				rgb[0] = 0.6f;
				rgb[1] = 0.6f;
				rgb[2] = x / 0.6f;
			}
			else
			{
				rgb[0] = x / 0.9f;
				rgb[1] = 0.5f + 1.0f - rgb[0];
				rgb[2] = rgb[1];
			}
		}
	}

	return std::move(rgb);
}

std::vector<float> EpochVisualizer::getWaveData()
{
	std::vector<float> waveData;
	waveData.resize(this->waveSampleSize);
	this->fmodSystem->getWaveData(waveData.data(), waveData.size(), 0);
	return std::move(waveData);
}

std::vector<float> EpochVisualizer::getStereoWaveData()
{
	std::vector<float> waveData;

	std::vector<float> waveDataLeft;
	waveDataLeft.resize(this->waveSampleSize);

	std::vector<float> waveDataRight;
	waveDataRight.resize(this->waveSampleSize);

	this->fmodSystem->getWaveData(waveDataLeft.data(), waveDataLeft.size(), 0);
	this->fmodSystem->getWaveData(waveDataRight.data(), waveDataRight.size(), 1);

	for(size_t i = 0; i < this->waveSampleSize; i++)
	{
		if(i % 2 == 0)
		{
			waveData.push_back(-fabs(waveDataLeft[i]));
		}
		else
		{
			waveData.push_back(fabs(waveDataRight[i]));
		}
	}

	return std::move(waveData);
}

std::vector<float> EpochVisualizer::getSpectrumDataMirrorDB()
{
	std::vector<float> waveDataLeft;
	waveDataLeft.resize(this->sampleSize);

	std::vector<float> waveDataRight;
	waveDataRight.resize(this->sampleSize);

	std::vector<float> waveDataReversed;
	waveDataReversed.resize(this->sampleSize);

	this->fmodSystem->getSpectrum(waveDataLeft.data(), waveDataLeft.size(), 0, FMOD_DSP_FFT_WINDOW::FMOD_DSP_FFT_WINDOW_HANNING);
	this->fmodSystem->getSpectrum(waveDataRight.data(), waveDataRight.size(), 1, FMOD_DSP_FFT_WINDOW::FMOD_DSP_FFT_WINDOW_HANNING);

	for(int i = waveDataLeft.size() / 2; i >= 0; i--)
	{
		auto db = 10.0f * log10(1.0f + waveDataLeft[i]) * 2.0f;
		db *= 1.5f;
		
		if(i % 2 == 0)
		{
			waveDataReversed[waveDataLeft.size()/2 - i] = db;
		}
		else
		{
			waveDataReversed[waveDataLeft.size()/2 - i] = -db;
		}
	}

	for(int i = waveDataRight.size() / 2; i >= 0; i--)
	{
		auto db = 10.0f * log10(1.0f + waveDataRight[i]) * 2.0f;
		db *= 1.5f;
		
		if(i % 2 == 0)
		{
			waveDataReversed[waveDataRight.size()/2 + i - 1] = db;
		}
		else
		{
			waveDataReversed[waveDataRight.size()/2 + i - 1] = -db;
		}
	}

	return std::move(waveDataReversed);
}

void EpochVisualizer::soundComplete()
{
	// Next in playlist.
	this->nextTrack();
}

void EpochVisualizer::loadPlaylist()
{
	std::vector<std::string> extensions;
	extensions.push_back("epoch");
	auto path = ci::app::getOpenFilePath("", extensions);

	if(path.empty() == false)
	{
		auto filePreferred = path.make_preferred();
		std::string fileName = filePreferred.string();
		this->loadFilePlaylist(fileName);
	}
}

void EpochVisualizer::savePlaylist()
{
	std::vector<std::string> extensions;
	extensions.push_back("epoch");
	auto path = ci::app::getSaveFilePath("", extensions);

	if(path.empty() == false)
	{
		std::ofstream os;
		os.open(path.c_str());

		for_each(std::begin(this->playList), std::end(this->playList), 
			[&os](const std::string& entry)
			{
				os << entry << "\n";
			});
	}
}

void EpochVisualizer::nextTrack()
{
	if(this->playList.empty() == false)
	{
		this->playListTrackNumber++;

		if(this->playListTrackNumber >= this->playList.size())
		{
			this->playListTrackNumber = 0;
		}

		auto track = this->playList[this->playListTrackNumber];
		this->loadFileMP3(track);
	}
}

void EpochVisualizer::previousTrack()
{
	if(this->playList.empty() == false)
	{
		this->playListTrackNumber--;

		if(this->playListTrackNumber >= this->playList.size())
		{
			this->playListTrackNumber = this->playList.size() - 1;
		}

		auto track = this->playList[this->playListTrackNumber];
		this->loadFileMP3(track);
	}
}

CINDER_APP_NATIVE(EpochVisualizer, RendererGl)
